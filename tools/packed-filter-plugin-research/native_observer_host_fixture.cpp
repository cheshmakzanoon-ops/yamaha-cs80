// Exercises exact official host helpers; never creates or loads a plugin.
#include "native_observer_host.hpp"
#include "native_observer_support.hpp"
#include <atomic>
#include <cstddef>
#include <cstdlib>
#include <cstdio>
#include <new>
#include <iostream>
using namespace Steinberg;
using namespace Steinberg::Vst;
using cs80_observer::require;
struct alignas(std::max_align_t) AllocationHeader {std::size_t bytes;};
static std::atomic<std::size_t> liveBytes{0},liveBlocks{0};
void* operator new(std::size_t n){auto* h=static_cast<AllocationHeader*>(std::malloc(sizeof(AllocationHeader)+(n?n:1)));if(!h)throw std::bad_alloc{};h->bytes=n;liveBytes+=n;++liveBlocks;return h+1;}
void operator delete(void* p) noexcept {if(p){auto* h=static_cast<AllocationHeader*>(p)-1;liveBytes-=h->bytes;--liveBlocks;std::free(h);}}
void operator delete(void* p,std::size_t) noexcept {::operator delete(p);}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete[](void* p) noexcept {::operator delete(p);}
void operator delete[](void* p,std::size_t) noexcept {::operator delete(p);}
IAttributeList* createAttributes(IHostApplication& host){TUID id{};IAttributeList::iid.toTUID(id);IAttributeList* p=nullptr;require(host.createInstance(id,id,reinterpret_cast<void**>(&p))==kResultOk && p,"create attributes");return p;}
int main(int argc,char** argv) {
    try {
        require(argc==2,"case required");const std::string test=argv[1];
        if(test.rfind("allocation-",0)==0) {
            const auto bytesBefore=liveBytes.load(),blocksBefore=liveBlocks.load();
            {
                auto attrs=HostAttributeList::make();const TChar text[]{'a','b','c',0};const unsigned char binary[]{1,0,3,4,5};
                if(test=="allocation-int-control"){require(attrs->setInt("key",1)==kResultOk && attrs->setInt("key",2)==kResultOk,"int setters");}
                else if(test=="allocation-string-control"){require(attrs->setString("key",text)==kResultOk,"string setter");}
                else if(test=="allocation-binary-control"){require(attrs->setBinary("key",binary,5)==kResultOk,"binary setter");}
                else if(test=="allocation-string-overwrite"){require(attrs->setString("key",text)==kResultOk && attrs->setString("key",text)==kResultOk,"string overwrite");}
                else if(test=="allocation-string-to-int"){require(attrs->setString("key",text)==kResultOk && attrs->setInt("key",7)==kResultOk,"string type replacement");}
                else if(test=="allocation-binary-overwrite"){require(attrs->setBinary("key",binary,5)==kResultOk && attrs->setBinary("key",binary,2)==kResultOk,"binary overwrite");}
                else if(test=="allocation-binary-to-float"){require(attrs->setBinary("key",binary,5)==kResultOk && attrs->setFloat("key",.5)==kResultOk,"binary type replacement");}
                else if(test=="allocation-empty-binary"){require(attrs->setBinary("key",binary,0)==kResultOk,"empty binary");}
                else require(false,"unknown allocation case");
            }
            const auto bytes=liveBytes.load()-bytesBefore,blocks=liveBlocks.load()-blocksBefore;
            std::printf("outstanding_bytes=%zu outstanding_blocks=%zu\n",bytes,blocks);
            // These are exact reproductions of a pinned SDK limitation, not
            // assertions that this leaking behavior qualifies a real host.
            if(test=="allocation-string-overwrite" || test=="allocation-string-to-int")require(bytes==8 && blocks==1,"reproduce old UTF16 allocation leak");
            else if(test=="allocation-binary-overwrite" || test=="allocation-binary-to-float")require(bytes==5 && blocks==1,"reproduce old binary allocation leak");
            else if(test=="allocation-empty-binary")require(bytes==0 && blocks==1,"reproduce zero-size binary block leak");
            else require(bytes==0 && blocks==0,"non-overwrite allocation control");
            return 0;
        }
        cs80_observer::OfficialHostContext owner{cs80_observer::OfficialHostContext::DefectControlOnly{}};auto& host=owner.application();
        if(test=="host-identity") {String128 name{};require(host.getName(name)==kResultOk && name[0]=='M',"official name");void* p=nullptr;require(host.queryInterface(IHostApplication::iid,&p)==kResultOk && p==&host,"host interface");require(static_cast<IHostApplication*>(p)->release()==1,"official host reference is non-owning");}
        else if(test=="unsupported-create") {TUID zero{};void* p=&host;require(host.createInstance(zero,zero,&p)!=kResultOk && !p,"unknown class rejected");}
        else if(test=="message") {TUID id{};IMessage::iid.toTUID(id);IMessage* message=nullptr;require(host.createInstance(id,id,reinterpret_cast<void**>(&message))==kResultOk && message,"message factory");require(!message->getMessageID(),"unset ID");message->setMessageID("first");message->setMessageID("second");require(std::strcmp(message->getMessageID(),"second")==0,"message ID replacement");auto* attrs=message->getAttributes();require(attrs && attrs==message->getAttributes(),"owned attribute identity");require(attrs->setInt("number",9)==kResultOk,"message attribute");void* queried=nullptr;require(message->queryInterface(IMessage::iid,&queried)==kResultOk,"message QI");require(static_cast<IMessage*>(queried)->release()==1 && message->release()==0,"message references balanced");}
        else {
            auto* attrs=createAttributes(host);
            if(test=="int-float") {int64 integer=0;double real=0;require(attrs->setInt("i",-1234567890123LL)==kResultOk && attrs->getInt("i",integer)==kResultOk && integer==-1234567890123LL,"integer roundtrip");require(attrs->setFloat("f",-.25)==kResultOk && attrs->getFloat("f",real)==kResultOk && real==-.25,"float roundtrip");require(attrs->getFloat("i",real)==kResultFalse,"type mismatch");}
            else if(test=="utf16") {const TChar text[]{'A',0x03b1,0};TChar out[3]{};require(attrs->setString("s",text)==kResultOk && attrs->getString("s",out,sizeof(out))==kResultOk && std::memcmp(out,text,sizeof(text))==0,"UTF16 size is bytes");}
            else if(test=="binary-copy") {char input[]{'A',0,'Z'};require(attrs->setBinary("b",input,3)==kResultOk,"binary set");input[0]='B';const void* out=nullptr;uint32 size=0;require(attrs->getBinary("b",out,size)==kResultOk && size==3 && static_cast<const char*>(out)[0]=='A' && static_cast<const char*>(out)[1]==0,"binary owned copy");}
            else if(test=="missing-invalid") {int64 n=7;require(attrs->getInt("missing",n)==kResultFalse && n==7,"missing key");require(attrs->setInt(nullptr,3)==kInvalidArgument,"null attribute ID");const void* p=nullptr;uint32 size=5;require(attrs->getBinary("missing",p,size)==kResultFalse && size==0,"missing binary size");}
            else require(false,"unknown host case");
            require(attrs->release()==0,"attribute reference balanced");
        }
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
