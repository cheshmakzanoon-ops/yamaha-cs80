#define INIT_CLASS_IID
#include "native_observer_bounded_host.hpp"
#include "native_observer_support.hpp"
#include <cstddef>
#include <cstdlib>
#include <cstdio>
#include <iostream>
#include <thread>
using namespace Steinberg;
using namespace Steinberg::Vst;
using namespace cs80_observer::bounded_host;
using cs80_observer::require;
struct alignas(std::max_align_t) AllocationHeader {std::size_t bytes;};
static std::atomic<std::size_t> liveBytes{0},liveBlocks{0};
static std::atomic<long> failAfter{-1};
void* operator new(std::size_t n){
    auto f=failAfter.load();
    if(f>=0){if(f==0)throw std::bad_alloc{};--failAfter;}
    auto* h=static_cast<AllocationHeader*>(std::malloc(sizeof(AllocationHeader)+(n?n:1)));
    if(!h)throw std::bad_alloc{};
    h->bytes=n;liveBytes+=n;++liveBlocks;return h+1;
}
void operator delete(void* p) noexcept {if(p){auto* h=static_cast<AllocationHeader*>(p)-1;liveBytes-=h->bytes;--liveBlocks;std::free(h);}}
void operator delete(void* p,std::size_t) noexcept {::operator delete(p);}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete[](void* p) noexcept {::operator delete(p);}
void operator delete[](void* p,std::size_t) noexcept {::operator delete(p);}
template<class T> T* create(IHostApplication& host){TUID id{};T::iid.toTUID(id);T* p=nullptr;require(host.createInstance(id,id,reinterpret_cast<void**>(&p))==kResultOk && p,"create");return p;}
struct RefProbe final:RefObject {
    explicit RefProbe(std::shared_ptr<State> s):RefObject(std::move(s)){}
    void force(uint32 n){refs=n;}
};
void run(const std::string& name){
    if(name=="reference-guards"){
        auto state=std::make_shared<State>(Limits{});auto* probe=new RefProbe(state);
        probe->force(std::numeric_limits<uint32>::max());require(probe->retain()==0 && !state->healthy,"overflow guarded");
        state->healthy=true;probe->force(0);require(probe->drop()==0 && !state->healthy && state->services==1,"observable zero counter guard");probe->force(1);probe->drop();require(state->services==0,"probe cleanup");return;
    }
    if(name=="global-keys"){
        Limits l;l.keys=1;Context c(l);auto* a=create<IAttributeList>(c.application());auto* b=create<IAttributeList>(c.application());
        require(a->setInt("a",1)==kResultOk && b->setInt("b",2)==kOutOfMemory && c.outstandingKeys()==1,"global separate-list key cap");
        a->release();b->release();require(c.outstandingKeys()==0 && c.outstandingBytes()==0 && !c.finish(),"global rollback");return;
    }
    if(name=="zero-allocation-binary"){
        Context c;auto* a=create<IAttributeList>(c.application());require(a->setBinary("k",nullptr,0)==kResultOk,"initial empty");
        const auto before=liveBlocks.load();failAfter=0;const auto result=a->setBinary("k",nullptr,0);failAfter=-1;
        require(result==kResultOk && liveBlocks==before && c.outstandingBytes()==2,"empty replacement no allocation");a->release();require(c.finish(),"empty finish");return;
    }
    if(name=="noalloc-qi"){
        Context c;void* p=nullptr;failAfter=0;const auto result=c.application().queryInterface(IHostApplication::iid,&p);failAfter=-1;
        require(result==kResultOk && p,"QI no allocation");static_cast<IHostApplication*>(p)->release();require(c.finish(),"QI finish");return;
    }
    if(name=="key-allocation-rollback"){
        Context c;auto* a=create<IAttributeList>(c.application());std::string key(100,'k');
        for(long stage=0;stage<2;++stage){failAfter=stage;const auto result=a->setInt(key.c_str(),1);failAfter=-1;require(result==kOutOfMemory && c.outstandingKeys()==0 && c.outstandingBytes()==0,"key and map allocation rollback");}
        a->release();require(!c.finish(),"rollback sticky");return;
    }

    if(name=="owner-loss"){
        IHostApplication* h=nullptr;IAttributeList* a=nullptr;
        {Context c;h=&c.application();h->addRef();a=create<IAttributeList>(*h);require(!c.finish(),"live refs reject");}
        String128 title{};require(h->getName(title)==kResultOk,"retained host valid");require(a->setInt("x",9)==kResultOk,"retained attr valid");a->release();h->release();return;
    }
    if(name=="invalid-policy"){Limits l;l.valueBytes=1;bool rejected=false;try{Context c(l);}catch(const std::invalid_argument&){rejected=true;}require(rejected,"invalid policy");return;}
    if(name=="oom-create"){
        Context c;TUID id{};IMessage::iid.toTUID(id);void* p=&c;failAfter=0;auto result=c.application().createInstance(id,id,&p);failAfter=-1;
        require(result==kOutOfMemory && !p && c.outstandingServices()==1 && !c.healthy(),"create rollback");return;
    }
    if(name=="oom-message-child"){
        Context c;TUID id{};IMessage::iid.toTUID(id);void* p=&c;failAfter=1;auto result=c.application().createInstance(id,id,&p);failAfter=-1;
        require(result==kOutOfMemory && !p && c.outstandingServices()==1 && !c.healthy(),"child rollback");return;
    }
    Limits l;
    if(name=="limit-services")l.services=2;
    if(name=="limit-keys")l.keys=1;
    if(name=="limit-list-keys")l.listKeys=1;
    if(name=="limit-list-bytes")l.listBytes=12;
    if(name=="limit-value")l.valueBytes=8;
    if(name=="limit-key")l.keyBytes=2;
    if(name=="limit-message")l.messageBytes=3;
    if(name=="transient-budget")l.totalBytes=20;
    Context c(l);auto& h=c.application();
    if(name=="host-qi"){
        void* p=nullptr;require(h.queryInterface(FUnknown::iid,&p)==kResultOk && p==&h && !c.finish(),"QI owns");require(static_cast<FUnknown*>(p)->release()==1 && c.finish(),"QI release");
        require(h.queryInterface(nullptr,&p)==kInvalidArgument && !p,"null IID");TUID zero{};p=&h;require(h.queryInterface(zero,&p)==kNoInterface && !p,"unknown QI");require(h.queryInterface(zero,nullptr)==kInvalidArgument,"null out");
        String128 title{};require(h.getName(title)==kResultOk && title[0]=='C' && title[127]==0,"name");return;
    }
    if(name=="unsupported-create"){
        TUID zero{};void* p=&h;require(h.createInstance(zero,zero,&p)==kNoInterface && !p && c.finish(),"unsupported");require(h.createInstance(nullptr,zero,&p)==kInvalidArgument && !p,"null CID");return;
    }
    if(name=="limit-services"){
        TUID id{};IMessage::iid.toTUID(id);void* p=&h;require(h.createInstance(id,id,&p)==kOutOfMemory && !p && c.outstandingServices()==1 && !c.healthy(),"service child failure rollback");return;
    }
    if(name.rfind("message",0)==0 || name=="limit-message"){
        auto* m=create<IMessage>(h);require(!m->getMessageID(),"initial ID");m->setMessageID("old");
        if(name=="message-oom"){std::string next(100,'x');failAfter=0;m->setMessageID(next.c_str());failAfter=-1;require(std::strcmp(m->getMessageID(),"old")==0 && !c.healthy(),"ID OOM preserves old");}
        else if(name=="message-null"){std::string large(100,'x');m->setMessageID(large.c_str());
            failAfter=0;m->setMessageID(nullptr);m->setMessageID(nullptr);failAfter=-1;
            require(!m->getMessageID() && c.healthy() && c.outstandingBytes()==0,"null clears without allocation");}
        else if(name=="limit-message"){m->setMessageID("long");require(std::strcmp(m->getMessageID(),"old")==0 && !c.healthy(),"ID limit preserves");}
        else if(name=="message-retained-attrs"){auto* a=m->getAttributes();a->addRef();m->release();require(a->setInt("x",1)==kResultOk && !c.finish(),"retained attributes");a->release();require(c.finish(),"retained release");return;}
        else {m->setMessageID("new");require(std::strcmp(m->getMessageID(),"new")==0 && m->getAttributes()==m->getAttributes(),"replacement stable attrs");void* p=nullptr;require(m->queryInterface(IMessage::iid,&p)==kResultOk && static_cast<IMessage*>(p)->release()==1,"message QI");}
        m->release();require(c.outstandingBytes()==0 && c.outstandingServices()==1,"message accounting");return;
    }
    auto* a=create<IAttributeList>(h);
    bool expectedFailure=false;
    if(name=="variants"){
        int64 n=0;double d=0;require(a->setInt("i",-123)==kResultOk && a->getInt("i",n)==kResultOk && n==-123,"int");require(a->setFloat("f",.75)==kResultOk && a->getFloat("f",d)==kResultOk && d==.75,"float");
        n=7;require(a->getInt("f",n)==kResultFalse && n==0,"type mismatch zero");const void* p=a;uint32 count=8;require(a->getBinary("missing",p,count)==kResultFalse && !p && !count,"missing outputs reset");
        require(a->setInt(nullptr,3)==kInvalidArgument && a->setString("s",nullptr)==kInvalidArgument && a->setBinary("b",nullptr,1)==kInvalidArgument,"invalid setters");
    }else if(name=="attributes-qi"){
        void* p=nullptr;require(a->queryInterface(FUnknown::iid,&p)==kResultOk && p==a && static_cast<FUnknown*>(p)->release()==1,"attribute QI");
    }else if(name=="utf16"){
        const TChar text[]{'A',0x3b1,0};TChar out[4]{};require(a->setString("s",text)==kResultOk,"string");
        require(a->getString("s",out,6)==kResultOk && std::memcmp(out,text,6)==0,"full");
        std::memset(out,0xff,sizeof(out));require(a->getString("s",out,3)==kResultOk && std::memcmp(out,text,3)==0 && reinterpret_cast<unsigned char*>(out)[3]==255,"odd bytes");
        require(a->getString("s",nullptr,0)==kResultOk && a->getString("s",nullptr,1)==kInvalidArgument,"zero/null");
        require(a->getString("missing",out,4)==kResultFalse && out[0]==0 && out[1]==0,"failed output cleared");
    }else if(name=="binary"){
        unsigned char b[]{1,2,3};require(a->setBinary("b",b,3)==kResultOk,"binary");b[0]=9;const void* p=nullptr;uint32 n=0;require(a->getBinary("b",p,n)==kResultOk && n==3 && static_cast<const unsigned char*>(p)[0]==1,"owned copy");
        require(a->setBinary("b",nullptr,0)==kResultOk && a->getBinary("b",p,n)==kResultOk && !p && !n,"zero binary");
    }else if(name=="overwrite"){
        String text(100,'x');Binary b(100,7);
        for(int i=0;i<100;++i){require(a->setString("key",text.c_str())==kResultOk && a->setBinary("key",b.data(),100)==kResultOk && a->setInt("key",i)==kResultOk,"replace variants");}
        require(c.outstandingKeys()==1 && c.outstandingBytes()==12,"one key accounting");
    }else if(name=="concurrent"){
        std::atomic<bool> ok{true};std::vector<std::thread> workers;
        for(int t=0;t<4;++t)workers.emplace_back([&,t]{for(int i=0;i<1000;++i){int64 n=0;TChar out[8]{};const TChar text[]{'x',0};if(a->setInt("i",i+t)!=kResultOk || a->getInt("i",n)!=kResultOk || a->setString("s",text)!=kResultOk || a->getString("s",out,sizeof(out))!=kResultOk || out[0]!='x')ok=false;}});
        for(auto& thread:workers)thread.join();
        require(ok,"concurrent copied getters");
    }else if(name=="oom-insert" || name=="oom-node"){
        String text(100,'x');failAfter=name=="oom-insert"?0:1;auto result=a->setString("key",text.c_str());failAfter=-1;
        require(result==kOutOfMemory && c.outstandingKeys()==0 && c.outstandingBytes()==0,"failed insertion rollback");expectedFailure=true;
    }else if(name=="oom-replace"){
        require(a->setInt("key",42)==kResultOk,"initial");String text(100,'x');failAfter=0;auto result=a->setString("key",text.c_str());failAfter=-1;int64 n=0;
        require(result==kOutOfMemory && a->getInt("key",n)==kResultOk && n==42 && c.outstandingKeys()==1 && c.outstandingBytes()==12,"replacement rollback");expectedFailure=true;
    }else if(name=="transient-budget"){
        char data[8]{};require(a->setBinary("k",data,8)==kResultOk,"initial charge 10");require(a->setBinary("k",data,8)==kResultOk,"replacement peak18");
        char next[12]{};require(a->setBinary("k",next,12)==kOutOfMemory && c.outstandingBytes()==10,"peak22 rejected not final14");const void* p=nullptr;uint32 n=0;require(a->getBinary("k",p,n)==kResultOk && n==8,"old preserved");expectedFailure=true;
    }else if(name=="limit-keys" || name=="limit-list-keys"){
        require(a->setInt("a",1)==kResultOk && a->setInt("a",2)==kResultOk,"replacement no key charge");require(a->setInt("b",2)==kOutOfMemory && c.outstandingKeys()==1,"key limit");expectedFailure=true;
    }else if(name=="limit-list-bytes"){
        require(a->setInt("a",1)==kResultOk && a->setInt("b",2)==kOutOfMemory && c.outstandingBytes()==10,"list bytes");expectedFailure=true;
    }else if(name=="limit-value"){
        const TChar good[]{'a','b','c',0},bad[]{'a','b','c','d',0};require(a->setString("s",good)==kResultOk && a->setString("s",bad)==kOutOfMemory,"terminator in value cap");expectedFailure=true;
    }else if(name=="limit-key"){
        require(a->setInt("ab",1)==kResultOk && a->setInt("abc",2)==kOutOfMemory,"key bound");expectedFailure=true;
    }else require(false,"unknown case");
    require(!c.finish(),"live object not finished");a->release();require(c.outstandingServices()==1 && !c.outstandingKeys() && !c.outstandingBytes(),"balanced accounting");require(c.finish()!=expectedFailure,"sticky finish status");
}
int main(int argc,char** argv){
    try{require(argc==2,"case argument");const std::string name=argv[1];const auto bytes=liveBytes.load(),blocks=liveBlocks.load();run(name);require(failAfter.load()==-1,"failure injector reset");require(liveBytes==bytes && liveBlocks==blocks,"all actual allocations released");std::puts("allocation_balance=0");return 0;}
    catch(const std::exception& e){failAfter=-1;std::cerr<<e.what()<<'\n';return 1;}
}
