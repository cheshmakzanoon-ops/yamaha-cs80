#pragma once
// Independent host implementation, NOT the official SDK helper or a vendor patch.
#include "pluginterfaces/vst/ivsthostapplication.h"
#include "pluginterfaces/vst/ivstmessage.h"
#include <atomic>
#include <algorithm>
#include <cstring>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

namespace cs80_observer::bounded_host {
using namespace Steinberg;
using namespace Steinberg::Vst;
struct Limits {
    std::size_t services=1024,keys=65536,listKeys=4096,listBytes=8*1024*1024,
        valueBytes=1024*1024,keyBytes=1024,messageBytes=4096,totalBytes=64*1024*1024;
    bool valid() const noexcept {
        const Limits cap;
        return services && services<=cap.services && keys && keys<=cap.keys &&
            listKeys && listKeys<=cap.listKeys && listBytes && listBytes<=cap.listBytes &&
            valueBytes>=sizeof(TChar) && valueBytes<=cap.valueBytes && keyBytes &&
            keyBytes<=cap.keyBytes && messageBytes && messageBytes<=cap.messageBytes &&
            totalBytes && totalBytes<=cap.totalBytes;
    }
};
struct LimitFailure {};
struct State {
    const Limits limits;
    std::atomic<bool> healthy{true};
    std::atomic<std::size_t> services{0},keys{0},bytes{0};
    explicit State(Limits l):limits(l){if(!l.valid())throw std::invalid_argument("host limits");}
    void fail() noexcept {healthy.store(false);}
    bool reserve(std::atomic<std::size_t>& counter,std::size_t n,std::size_t cap) noexcept {
        auto old=counter.load();
        do {if(old>cap || n>cap-old){fail();return false;}}
        while(!counter.compare_exchange_weak(old,old+n));
        return true;
    }
    void debit(std::atomic<std::size_t>& counter,std::size_t n) noexcept {
        auto old=counter.load();
        do {if(n>old){fail();return;}}while(!counter.compare_exchange_weak(old,old-n));
    }
};
struct Reservation {
    State& state;std::size_t bytes=0,keys=0;
    Reservation(State& s,std::size_t b,std::size_t k):state(s){
        if(!state.reserve(state.bytes,b,state.limits.totalBytes))throw LimitFailure{};
        bytes=b;
        if(!state.reserve(state.keys,k,state.limits.keys)){
            state.debit(state.bytes,bytes);bytes=0;throw LimitFailure{};
        }
        keys=k;
    }
    ~Reservation(){state.debit(state.bytes,bytes);state.debit(state.keys,keys);}
    void commit() noexcept {bytes=keys=0;}
};
inline bool equalId(const TUID a,const FUID& b) noexcept {return a && FUnknownPrivate::iidEqual(a,b);}
inline bool length(const char* text,std::size_t maximum,std::size_t& result) noexcept {
    if(!text)return false;
    for(result=0;result<=maximum;++result)if(text[result]==0)return true;
    return false;
}
class RefObject {
protected:
    std::shared_ptr<State> state;
    std::atomic<uint32> refs{1};
    explicit RefObject(std::shared_ptr<State> s):state(std::move(s)){
        if(!state->reserve(state->services,1,state->limits.services))throw LimitFailure{};
    }
    virtual ~RefObject(){state->debit(state->services,1);}
public:
    uint32 retain() noexcept {
        auto n=refs.load();
        do {if(n==0 || n==std::numeric_limits<uint32>::max()){state->fail();return 0;}}
        while(!refs.compare_exchange_weak(n,n+1));return n+1;
    }
    uint32 drop() noexcept {
        auto n=refs.load();
        do {if(n==0){state->fail();return 0;}}while(!refs.compare_exchange_weak(n,n-1));
        if(n==1){delete this;return 0;}return n-1;
    }
    uint32 references() const noexcept{return refs.load();}
};
using String=std::basic_string<TChar>;
using Binary=std::vector<unsigned char>;
using Value=std::variant<int64,double,String,Binary>;
struct Entry {Value value;std::size_t bytes;};
class Attributes final:public IAttributeList,public RefObject {
    std::mutex mutex;
    std::map<std::string,Entry,std::less<>> values;
    std::size_t charged=0;
    template<class F> tresult put(AttrID id,std::size_t payload,F make) noexcept {
        std::size_t keyLength=0;
        if(!id)return kInvalidArgument;
        if(!length(id,state->limits.keyBytes,keyLength) || payload>state->limits.valueBytes){state->fail();return kOutOfMemory;}
        try {
            std::lock_guard lock(mutex);
            auto old=values.find(id);const bool insert=old==values.end();
            const auto keyCharge=insert?keyLength+1:0;
            const auto oldCharge=insert?0:old->second.bytes;
            if((insert && values.size()>=state->limits.listKeys) || charged<oldCharge ||
                keyCharge>state->limits.listBytes || payload>state->limits.listBytes-keyCharge ||
                charged-oldCharge>state->limits.listBytes-keyCharge-payload){state->fail();return kOutOfMemory;}
            Reservation reserve(*state,payload+keyCharge,insert?1:0);
            { Entry fresh{make(),payload};
            if(insert)values.emplace(std::string(id,keyLength),std::move(fresh));
            else {static_assert(std::is_nothrow_swappable_v<Value>);old->second.value.swap(fresh.value);old->second.bytes=payload;} }
            charged=charged-oldCharge+payload+keyCharge;
            reserve.commit();state->debit(state->bytes,oldCharge);
            return kResultOk;
        }catch(const std::bad_alloc&){state->fail();return kOutOfMemory;}
        catch(const LimitFailure&){state->fail();return kOutOfMemory;}
        catch(...){state->fail();return kInternalError;}
    }
    template<class F> tresult read(AttrID id,F use) noexcept {
        if(!id)return kInvalidArgument;
        std::size_t n=0;if(!length(id,state->limits.keyBytes,n))return kInvalidArgument;
        try {std::lock_guard lock(mutex);auto found=values.find(id);return found==values.end()?kResultFalse:use(found->second.value);}
        catch(...){state->fail();return kInternalError;}
    }
public:
    explicit Attributes(std::shared_ptr<State> s):RefObject(std::move(s)){}
    ~Attributes() override {const auto count=values.size();values.clear();state->debit(state->bytes,charged);state->debit(state->keys,count);}
    tresult PLUGIN_API queryInterface(const TUID id,void** out) override {
        if(!out)return kInvalidArgument;
        *out=nullptr;
        if(!id)return kInvalidArgument;
        if(equalId(id,IAttributeList::iid)||equalId(id,FUnknown::iid)){if(!addRef())return kInternalError;*out=static_cast<IAttributeList*>(this);return kResultOk;}return kNoInterface;
    }
    uint32 PLUGIN_API addRef() override{return retain();}
    uint32 PLUGIN_API release() override{return drop();}
    tresult PLUGIN_API setInt(AttrID id,int64 n) override{return put(id,sizeof(n),[=]{return Value{n};});}
    tresult PLUGIN_API setFloat(AttrID id,double n) override{return put(id,sizeof(n),[=]{return Value{n};});}
    tresult PLUGIN_API getInt(AttrID id,int64& out) override {out=0;return read(id,[&](const Value& v){if(auto p=std::get_if<int64>(&v)){out=*p;return kResultOk;}return kResultFalse;});}
    tresult PLUGIN_API getFloat(AttrID id,double& out) override {out=0;return read(id,[&](const Value& v){if(auto p=std::get_if<double>(&v)){out=*p;return kResultOk;}return kResultFalse;});}
    tresult PLUGIN_API setString(AttrID id,const TChar* text) override {
        if(!id || !text)return kInvalidArgument;
        const auto maxUnits=state->limits.valueBytes/sizeof(TChar);
        std::size_t n=0;while(n<maxUnits && text[n])++n;
        if(n==maxUnits){state->fail();return kOutOfMemory;}
        return put(id,(n+1)*sizeof(TChar),[=]{return Value{String(text,n)};});
    }
    tresult PLUGIN_API getString(AttrID id,TChar* out,uint32 bytes) override {
        if(!out && bytes)return kInvalidArgument;
        if(bytes)std::memset(out,0,bytes);
        return read(id,[&](const Value& v){if(auto p=std::get_if<String>(&v)){const auto n=std::min<std::size_t>(bytes,(p->size()+1)*sizeof(TChar));if(n)std::memcpy(out,p->c_str(),n);return kResultOk;}return kResultFalse;});
    }
    tresult PLUGIN_API setBinary(AttrID id,const void* data,uint32 bytes) override {
        if(bytes && !data)return kInvalidArgument;
        return put(id,bytes,[=]{Binary b;if(bytes){const auto* p=static_cast<const unsigned char*>(data);b.assign(p,p+bytes);}return Value{std::move(b)};});
    }
    tresult PLUGIN_API getBinary(AttrID id,const void*& out,uint32& bytes) override {
        out=nullptr;bytes=0;return read(id,[&](const Value& v){if(auto p=std::get_if<Binary>(&v)){out=p->empty()?nullptr:p->data();bytes=static_cast<uint32>(p->size());return kResultOk;}return kResultFalse;});
    }
};
class Message final:public IMessage,public RefObject {
    std::mutex mutex;std::string id;bool hasId=false;std::size_t charged=0;
    Attributes* attributes;
public:
    explicit Message(std::shared_ptr<State> s):RefObject(s),attributes(new Attributes(std::move(s))){}
    ~Message() override {attributes->release();{std::string empty;id.swap(empty);}state->debit(state->bytes,charged);}
    tresult PLUGIN_API queryInterface(const TUID iid,void** out) override {
        if(!out)return kInvalidArgument;
        *out=nullptr;
        if(!iid)return kInvalidArgument;
        if(equalId(iid,IMessage::iid)||equalId(iid,FUnknown::iid)){if(!addRef())return kInternalError;*out=static_cast<IMessage*>(this);return kResultOk;}return kNoInterface;
    }
    uint32 PLUGIN_API addRef() override{return retain();}
    uint32 PLUGIN_API release() override{return drop();}
    FIDString PLUGIN_API getMessageID() override {
        try {std::lock_guard lock(mutex);return hasId?id.c_str():nullptr;}catch(...){state->fail();return nullptr;}
    }
    void PLUGIN_API setMessageID(FIDString text) override {
        if(!text){
            try {std::lock_guard lock(mutex);{std::string empty;id.swap(empty);}
                hasId=false;const auto old=charged;charged=0;state->debit(state->bytes,old);
            }catch(...){state->fail();}
            return;
        }
        std::size_t n=0;if(!length(text,state->limits.messageBytes,n)){state->fail();return;}
        try {std::lock_guard lock(mutex);Reservation reserve(*state,n+1,0);{std::string fresh(text,n);id.swap(fresh);}
            hasId=true;const auto old=charged;charged=n+1;reserve.commit();state->debit(state->bytes,old);
        }catch(...){state->fail();}
    }
    IAttributeList* PLUGIN_API getAttributes() override{return attributes;}
};
class Host final:public IHostApplication,public RefObject {
public:
    explicit Host(std::shared_ptr<State> s):RefObject(std::move(s)){}
    tresult PLUGIN_API queryInterface(const TUID id,void** out) override {
        if(!out)return kInvalidArgument;
        *out=nullptr;
        if(!id)return kInvalidArgument;
        if(equalId(id,IHostApplication::iid)||equalId(id,FUnknown::iid)){if(!addRef())return kInternalError;*out=static_cast<IHostApplication*>(this);return kResultOk;}return kNoInterface;
    }
    uint32 PLUGIN_API addRef() override{return retain();}
    uint32 PLUGIN_API release() override{return drop();}
    tresult PLUGIN_API getName(String128 out) override {
        if(!out)return kInvalidArgument;
        std::fill(out,out+128,TChar{});constexpr char name[]="CS80 bounded observer";
        for(std::size_t i=0;i<sizeof(name)-1;++i)out[i]=name[i];
        return kResultOk;
    }
    tresult PLUGIN_API createInstance(TUID cid,TUID iid,void** out) override {
        if(!out)return kInvalidArgument;
        *out=nullptr;
        if(!cid || !iid)return kInvalidArgument;
        try {
            if(equalId(cid,IMessage::iid) && equalId(iid,IMessage::iid)){*out=static_cast<IMessage*>(new Message(state));return kResultOk;}
            if(equalId(cid,IAttributeList::iid) && equalId(iid,IAttributeList::iid)){*out=static_cast<IAttributeList*>(new Attributes(state));return kResultOk;}
            return kNoInterface;
        }catch(const std::bad_alloc&){state->fail();return kOutOfMemory;}
        catch(const LimitFailure&){state->fail();return kOutOfMemory;}
        catch(...){state->fail();return kInternalError;}
    }
};
class Context {
    std::shared_ptr<State> state;
    Host* host;
public:
    explicit Context(Limits limits={}):state(std::make_shared<State>(limits)),host(new Host(state)){}
    ~Context(){host->release();}
    Context(const Context&)=delete;Context& operator=(const Context&)=delete;
    IHostApplication& application() noexcept{return *host;}
    // Requires externally guaranteed quiescence; cleanup/destruction is NOT admission.
    bool finish() const noexcept {
        return state->healthy.load() && host->references()==1 && state->services.load()==1 &&
            state->keys.load()==0 && state->bytes.load()==0;
    }
    bool healthy() const noexcept{return state->healthy.load();}
    std::size_t outstandingBytes() const noexcept{return state->bytes.load();}
    std::size_t outstandingKeys() const noexcept{return state->keys.load();}
    std::size_t outstandingServices() const noexcept{return state->services.load();}
};
}
