// Loads only test-produced inert libraries with a permanently-null factory.
#define INIT_CLASS_IID
#include "native_observer_dlopen.hpp"
#include <iostream>
using namespace cs80_observer;
struct FakeHost final : IHostApplication {
    int refs=1;
    tresult PLUGIN_API queryInterface(const TUID id,void** out) override {
        if(!out){return kInvalidArgument;}*out=nullptr;
        if(std::memcmp(id,IHostApplication::iid,16))return kNoInterface;
        *out=this;addRef();return kResultOk;
    }
    uint32 PLUGIN_API addRef() override{return ++refs;}
    uint32 PLUGIN_API release() override{return --refs;}
    tresult PLUGIN_API getName(String128 name) override{name[0]='F';name[1]=0;return kResultOk;}
    tresult PLUGIN_API createInstance(TUID,TUID,void** out) override{if(out)*out=nullptr;return kNoInterface;}
};
int main(int argc,char** argv) {
    try {
        require(argc==3,"case and inert library path required");const std::string test=argv[1];DlopenOps loader;
        if(test=="invalid-elf") {require(loader.open(argv[2])==nullptr && loader.nativeOpenCalls==1 && loader.nativeCloseCalls==0,"invalid ELF rejected");}
        else if(test=="noncanonical" || test=="missing" || test=="symlink") {
            bool failed=false;try{loader.open(argv[2]);}catch(...){failed=true;}
            require(failed && loader.nativeOpenCalls==0 && loader.nativeCloseCalls==0,"invalid path rejected before native loader");
        }
        else if(test=="direct" || test=="wrong-close" || test=="wrong-resolve" || test=="no-reopen") {
            void* handle=loader.open(argv[2]);require(handle,"open inert library");
            if(test=="wrong-close"){bool rejected=false;try{loader.close(reinterpret_cast<void*>(1));}catch(...){rejected=true;}require(rejected && loader.nativeCloseCalls==0,"foreign close blocked");}
            if(test=="wrong-resolve"){bool rejected=false;try{loader.resolve(reinterpret_cast<void*>(1));}catch(...){rejected=true;}require(rejected,"foreign resolution blocked");}
            const auto bindings=loader.resolve(handle);require(bindings.entry(handle),"inert entry");require(!bindings.factory(),"fixture is not a plugin");require(bindings.exit(),"inert exit");
            require(loader.close(handle)==0 && loader.nativeCloseCalls==1,"native close once");
            bool rejected=false;try{loader.close(handle);}catch(...){rejected=true;}require(rejected && loader.nativeCloseCalls==1,"close retry blocked");
            if(test=="no-reopen"){rejected=false;try{loader.open(argv[2]);}catch(...){rejected=true;}require(rejected && loader.nativeOpenCalls==1,"reopen blocked");}
        }else {
            FakeHost host;InjectedModule module(loader);TUID cid{};std::memset(cid,5,16);bool failed=false;
            try{module.load(argv[2],cid,host);}catch(...){failed=true;}
            require(failed && !module.loadSucceeded && host.refs==1 && loader.nativeOpenCalls==1 && loader.nativeCloseCalls==1,"module failure cleaned exact loader handle and host reference");
            require(module.closedCleanly==(test!="exit-failure"),"cleanup admission");
            if(test=="data-export" || test=="dependency-export" || test=="missing-entry" || test=="missing-factory" || test=="missing-exit")require(!loader.resolutionSucceeded,"invalid export provenance rejected");
            else require(loader.resolutionSucceeded,"complete inert exports resolved");
        }
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
