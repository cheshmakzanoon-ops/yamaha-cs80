#define INIT_CLASS_IID
#include "native_observer_runloop.hpp"
#include "native_observer_support.hpp"
#include <unistd.h>
#include <fcntl.h>
#include <cstdio>
#include <iostream>
#include <stdexcept>
using namespace cs80_observer::runloop;
using cs80_observer::require;
struct ManualClock:Clock {uint64 value=0;uint64 now() noexcept override{return value;}};
struct Pipe {int fds[2]{};Pipe(){require(::pipe(fds)==0,"pipe");}~Pipe(){::close(fds[0]);::close(fds[1]);}void send(){char c='x';require(::write(fds[1],&c,1)==1,"write");}};
struct Handler final:IEventHandler,ITimerHandler {
    uint32 refs=1,calls=0;Dispatcher* loop=nullptr;Handler* other=nullptr;ManualClock* clock=nullptr;
    enum Action {Read,None,SelfRemove,RemoveOther,ReRegister,Throw,Hang,Nest,LastRelease,Advance,TimerRemove} action=Read;
    bool failRef=false,throwRef=false,reenterRef=false;int expectedFd=-1;uint64 jump=0;
    tresult PLUGIN_API queryInterface(const TUID id,void** out) override {
        if(!out)return kInvalidArgument;
        *out=nullptr;
        if(FUnknownPrivate::iidEqual(id,IEventHandler::iid)||FUnknownPrivate::iidEqual(id,FUnknown::iid))*out=static_cast<IEventHandler*>(this);
        else if(FUnknownPrivate::iidEqual(id,ITimerHandler::iid))*out=static_cast<ITimerHandler*>(this);else return kNoInterface;
        addRef();return kResultOk;
    }
    uint32 PLUGIN_API addRef() override {if(throwRef)throw std::runtime_error("reference");if(failRef)return 0;if(reenterRef)loop->unregisterEventHandler(this);return ++refs;}
    uint32 PLUGIN_API release() override {require(refs>1,"fixture base ref retained");return --refs;}
    void PLUGIN_API onFDIsSet(int fd) override {
        ++calls;if(expectedFd>=0)require(fd==expectedFd,"fd identity");
        if(action==Read){char c;require(::read(fd,&c,1)==1 && c=='x',"plugin reads own pipe");}
        if(action==SelfRemove)require(loop->unregisterEventHandler(this)==kResultOk,"self remove");
        if(action==RemoveOther){require(loop->unregisterEventHandler(other)==kResultOk,"remove other");loop->unregisterEventHandler(this);}
        if(action==ReRegister){loop->unregisterEventHandler(other);require(loop->registerEventHandler(other,fd)==kResultOk,"replace generation");loop->unregisterEventHandler(this);}
        if(action==Throw)throw std::runtime_error("callback");
        if(action==Hang)for(;;)::pause();
        if(action==Nest)require(!loop->pump({10000000,4,4}).ok,"nested rejected");
        if(action==LastRelease)require(loop->release()==1,"last active release retained");
        if(action==Advance)clock->value=jump;
    }
    void PLUGIN_API onTimer() override {++calls;if(action==TimerRemove)loop->unregisterTimer(this);if(action==Advance)clock->value=jump;}
};
void run(const std::string& name){
    ManualClock clock;auto* loop=new Dispatcher(clock);Handler a,b;a.loop=b.loop=loop;a.clock=b.clock=&clock;
    if(name=="qi") {void* p=nullptr;require(loop->queryInterface(IRunLoop::iid,&p)==kResultOk && p==loop,"QI");require(static_cast<IRunLoop*>(p)->release()==1,"QI ref");}
    else if(name=="thread") {std::thread worker([&]{require(loop->registerTimer(&a,1)==kResultFalse,"wrong thread rejected");});worker.join();require(loop->failure()==Failure::WrongThread && !loop->registrations() && a.refs==1,"thread no mutation");}
    else if(name=="thread-release") {std::thread worker([&]{require(loop->release()==1,"wrong thread final release retained");});worker.join();require(loop->failure()==Failure::WrongThread,"wrong thread release failure");}
    else if(name=="thread-unregister") {require(loop->registerTimer(&a,1)==kResultOk,"timer");std::thread worker([&]{require(loop->unregisterTimer(&a)==kResultFalse,"wrong thread unregister");});worker.join();require(a.refs==2 && loop->registrations()==1,"no foreign mutation");loop->unregisterTimer(&a);}
    else if(name=="thread-pump") {std::thread worker([&]{require(!loop->pump({1000000,1,1}).ok,"wrong thread pump");});worker.join();require(loop->failure()==Failure::WrongThread,"pump thread failure");}
    else if(name=="timer-conflict") {require(loop->registerTimer(&a,1)==kResultOk && loop->registerTimer(&a,2)==kInvalidArgument && a.refs==2,"interval change rejected");loop->unregisterTimer(&a);}
    else if(name=="closed") {require(loop->shutdown(),"close");require(loop->registerTimer(&a,1)==kResultFalse && a.refs==1,"closed mutation");}
    else if(name=="timer-zero")require(loop->registerTimer(&a,0)==kInvalidArgument && a.refs==1,"zero timer");
    else if(name=="timer-overflow")require(loop->registerTimer(&a,std::numeric_limits<uint64>::max())==kInvalidArgument && a.refs==1,"interval overflow");
    else if(name=="deadline-overflow") {clock.value=std::numeric_limits<uint64>::max()-1;require(loop->registerTimer(&a,1)==kInvalidArgument,"deadline overflow");}
    else if(name=="timer-real") {
        loop->release();MonotonicClock real;loop=new Dispatcher;a.loop=loop;
        require(loop->registerTimer(&a,1)==kResultOk,"actual timer");std::this_thread::sleep_for(std::chrono::milliseconds(3));auto r=loop->pump({real.now()+100000000,8,4});require(r.ok && r.sampledIdle && a.calls>=1,"real timer due");loop->unregisterTimer(&a);require(a.refs==1 && loop->shutdown(),"real timer shutdown");loop->release();return;
    }else if(name=="timer-order" || name=="timer-skips" || name=="timer-self" || name=="timer-advance-overflow"){
        if(name=="timer-self")a.action=Handler::TimerRemove;
        require(loop->registerTimer(&a,1)==kResultOk,"timer register");
        require(loop->registerTimer(&a,1)==kResultOk && a.refs==2,"timer duplicate idempotent");
        if(name=="timer-order")require(loop->registerTimer(&b,2)==kResultOk,"second timer");
        clock.value=name=="timer-advance-overflow"?std::numeric_limits<uint64>::max()-1:5500000;
        auto result=loop->pump({std::numeric_limits<uint64>::max(),8,4});
        if(name=="timer-advance-overflow")require(!result.ok && loop->failure()==Failure::Overflow && !a.calls,"advance overflow");
        else {require(result.ok && result.sampledIdle && a.calls==1,"one firing not catchup");uint64 last=0;uint32 seen=0;
            for(std::size_t i=0;i<loop->receiptSize();++i){const auto& r=loop->receipt(i);if(r.event==Event::CallbackBegin){require(r.generation>last,"timer ordering");last=r.generation;++seen;if(r.generation==1)require(r.skipped==4,"missed periods receipt");}}
            require(seen==(name=="timer-order"?2u:1u),"timer callback count");}
        loop->unregisterTimer(&a);loop->unregisterTimer(&b);
    }else if(name=="receipt-cap"){
        for(int i=0;i<5000 && loop->healthy();++i){require(loop->registerTimer(&a,1)==kResultOk || !loop->healthy(),"registration");loop->unregisterTimer(&a);}
        loop->unregisterTimer(&a);require(loop->failure()==Failure::Receipt && a.refs==1,"receipt fail closed balanced");
    }else {
        Pipe pipe;Pipe second;a.expectedFd=pipe.fds[0];
        if(name=="ref-fail")a.failRef=true;
                if(name=="ref-throw")a.throwRef=true;
                if(name=="ref-reenter")a.reenterRef=true;
        if(name.rfind("ref-",0)==0){require(loop->registerEventHandler(&a,pipe.fds[0])==kResultFalse && a.refs==1 && !loop->registrations(),"reference failure rollback");}
        else if(name=="capacity"){
            for(int i=0;i<256;++i)require(loop->registerEventHandler(&a,1000+i)==kResultOk,"fd capacity setup");
            require(loop->registerEventHandler(&a,9999)==kOutOfMemory && a.refs==257,"capacity rejected");loop->unregisterEventHandler(&a);
        }else {
            require(loop->registerEventHandler(&a,pipe.fds[0])==kResultOk,"fd register");
            if(name=="many-fds"){require(loop->registerEventHandler(&a,second.fds[0])==kResultOk && a.refs==3,"same handler multiplefds");require(loop->registerEventHandler(&a,second.fds[0])==kResultOk && a.refs==3,"duplicate pair");require(loop->unregisterEventHandler(&a)==kResultOk && a.refs==1 && !loop->registrations(),"unregister all");}
            else if(name=="shutdown-live"){require(!loop->shutdown() && loop->registrations()==1 && a.refs==2,"no fake cleanup");loop->unregisterEventHandler(&a);}
            else if(name=="final-release-live"){require(loop->release()==1 && a.refs==2 && loop->registrations()==1,"owner ref retained");loop->unregisterEventHandler(&a);}
            else {
                if(name=="self")a.action=Handler::SelfRemove;
                if(name=="other" || name=="generation"){a.action=name=="other"?Handler::RemoveOther:Handler::ReRegister;a.other=&b;require(loop->registerEventHandler(&b,pipe.fds[0])==kResultOk,"other snapshot");b.action=Handler::SelfRemove;}
                if(name=="throw")a.action=Handler::Throw;
                if(name=="hang")a.action=Handler::Hang;
                if(name=="nested")a.action=Handler::Nest;
                if(name=="active-release")a.action=Handler::LastRelease;
                if(name=="snapshot-ref-fail" || name=="snapshot-ref-throw"){
                    require(loop->registerEventHandler(&b,pipe.fds[0])==kResultOk,"second snapshot ref");
                    b.failRef=name=="snapshot-ref-fail";b.throwRef=name=="snapshot-ref-throw";
                }
                if(name=="budget" || name=="round-budget")a.action=Handler::None;
                if(name=="deadline-snapshot"){a.action=Handler::Advance;a.jump=10000000;require(loop->registerEventHandler(&b,pipe.fds[0])==kResultOk,"second snapshot");}
                if(name=="hangup"){::close(pipe.fds[1]);pipe.fds[1]=-1;}else if(name=="invalid-fd"){::close(pipe.fds[0]);pipe.fds[0]=-1;}else if(name!="idle")pipe.send();
                const uint32 calls=name=="budget"?1:8,rounds=name=="round-budget"?1:4;
                auto result=loop->pump({name=="expired"?0u:10000000u,calls,rounds});
                if(name=="read" || name=="idle" || name=="self" || name=="other" || name=="generation")require(result.ok && result.sampledIdle,"successful sampled idle");
                else require(!result.ok,"terminal failure");
                if(name=="read")require(a.calls==1,"read callback once");
                if(name=="idle")require(!a.calls,"no readiness no callback");
                if(name=="snapshot-ref-fail" || name=="snapshot-ref-throw")require(!a.calls && !b.calls && a.refs==2 && b.refs==2,"partial snapshot refs rolled back");
                if(name=="other" || name=="deadline-snapshot")require(!b.calls && b.refs==(name=="other"?1u:2u),"stale or deadline snapshot released");
                if(name=="generation")require(b.calls==1,"replacement dispatched next fresh snapshot only");
                loop->unregisterEventHandler(&a);loop->unregisterEventHandler(&b);
                if(name!="invalid-fd")require(::fcntl(pipe.fds[0],F_GETFD)!=-1,"host never closes plugin fd");
            }
        }
    }
    require(a.refs==1 && b.refs==1 && !loop->registrations(),"all handler ownership balanced");const bool wasHealthy=loop->healthy();require(loop->shutdown()==wasHealthy,"explicit shutdown health");require(loop->release()==0,"release clean object");
}
int main(int argc,char** argv){try{require(argc==2,"case");run(argv[1]);std::puts("handler_refs_balanced=1");return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
