#pragma once
// Scoped UI-thread-only dispatcher. Not advertised by the observer host yet.
#include "pluginterfaces/gui/iplugview.h"
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <limits>
#include <poll.h>
#include <thread>
#include <algorithm>
namespace cs80_observer::runloop {
using namespace Steinberg;
using namespace Steinberg::Linux;
struct Clock {virtual ~Clock()=default;virtual uint64 now() noexcept=0;};
struct MonotonicClock final:Clock {
    uint64 now() noexcept override{return static_cast<uint64>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());}
};
enum class Failure:uint32 {None,WrongThread,Reentrant,Closed,Invalid,Capacity,Reference,Overflow,Poll,Hangup,Deadline,Budget,Callback,Receipt,LiveRelease,Outstanding};
enum class Event:uint32 {RegisterFd,UnregisterFd,RegisterTimer,UnregisterTimer,CallbackBegin,CallbackEnd,SampleIdle,Shutdown};
struct Record {Event event{};uint64 generation=0,time=0;int fd=-1;uint64 skipped=0;};
struct Budget {uint64 deadline=0;uint32 callbacks=1024,rounds=64;};
struct PumpResult {bool ok=false,sampledIdle=false;uint32 callbacks=0,rounds=0;};
class Dispatcher final:public IRunLoop {
    static constexpr std::size_t capacity=256,receiptCapacity=4096;
    struct Fd {IEventHandler* handler=nullptr;int fd=-1;uint64 generation=0;};
    struct Timer {ITimerHandler* handler=nullptr;uint64 interval=0,due=0,generation=0;};
    struct Pending {FUnknown* handler=nullptr;uint64 generation=0,due=0;std::size_t slot=0;int fd=-1;bool timer=false;};
    const std::thread::id ui=std::this_thread::get_id();
    MonotonicClock ownedClock;
    Clock& clock;
    std::atomic<uint32> references{1};
    std::atomic<Failure> error{Failure::None};
    std::array<Fd,capacity> fds{};
    std::array<Timer,capacity> timers{};
    std::array<Record,receiptCapacity> records{};
    std::size_t recorded=0;
    uint64 generation=0;
    bool pumping=false,refBoundary=false,closed=false;
    void fail(Failure f) noexcept {auto expected=Failure::None;error.compare_exchange_strong(expected,f);}
    bool owner() noexcept {if(std::this_thread::get_id()!=ui){fail(Failure::WrongThread);return false;}return true;}
    bool mutableNow(bool cleanup=false) noexcept {
        if(!owner())return false;
        if(refBoundary){fail(Failure::Reentrant);return false;}
        if(closed){fail(Failure::Closed);return false;}
        return cleanup || healthy();
    }
    void record(Event e,uint64 g=0,int fd=-1,uint64 skipped=0) noexcept {
        if(recorded==records.size()){fail(Failure::Receipt);return;}
        records[recorded++]={e,g,clock.now(),fd,skipped};
    }
    uint64 nextGeneration() noexcept {
        if(generation==std::numeric_limits<uint64>::max()){fail(Failure::Overflow);return 0;}return ++generation;
    }
    bool retain(FUnknown* h) noexcept {
        refBoundary=true;bool ok=false;
        try{ok=h->addRef()!=0;}catch(...){fail(Failure::Reference);}
        refBoundary=false;if(!ok)fail(Failure::Reference);return ok;
    }
    void releaseHandler(FUnknown* h) noexcept {
        refBoundary=true;try{h->release();}catch(...){fail(Failure::Reference);}refBoundary=false;
    }
    std::size_t live() const noexcept {
        std::size_t n=0;for(const auto& f:fds)n+=f.handler!=nullptr;for(const auto& t:timers)n+=t.handler!=nullptr;return n;
    }
    ~Dispatcher()=default; // final release refuses registered/active state
public:
    Dispatcher():clock(ownedClock){}
    // Test clock injection: injected clock must outlive every dispatcher reference.
    explicit Dispatcher(Clock& c):clock(c){}
    bool healthy() const noexcept{return error.load()==Failure::None;}
    Failure failure() const noexcept{return error.load();}
    // UI-only diagnostics; caller must not inspect receipt concurrently with pumping.
    std::size_t receiptSize() const noexcept{return recorded;}
    const Record& receipt(std::size_t i) const{return records.at(i);}
    std::size_t registrations() const noexcept{return live();}
    tresult PLUGIN_API queryInterface(const TUID id,void** out) override {
        if(!out)return kInvalidArgument;
        *out=nullptr;if(!id)return kInvalidArgument;
        if(FUnknownPrivate::iidEqual(id,IRunLoop::iid)||FUnknownPrivate::iidEqual(id,FUnknown::iid)){
            if(!addRef())return kInternalError;
            *out=static_cast<IRunLoop*>(this);return kResultOk;
        }return kNoInterface;
    }
    uint32 PLUGIN_API addRef() override {
        auto n=references.load();do{if(!n || n==std::numeric_limits<uint32>::max()){fail(Failure::Reference);return 0;}}while(!references.compare_exchange_weak(n,n+1));return n+1;
    }
    uint32 PLUGIN_API release() override {
        auto n=references.load();
        for(;;){
            if(!n){fail(Failure::Reference);return 0;}
            if(n==1){
                if(!owner())return 1;
                if(pumping || refBoundary || live()){fail(Failure::LiveRelease);return 1;}
            }
            if(references.compare_exchange_weak(n,n-1))break;
        }
        if(n==1){delete this;return 0;}return n-1;
    }
    tresult PLUGIN_API registerEventHandler(IEventHandler* h,FileDescriptor fd) override {
        if(!mutableNow())return kResultFalse;
        if(!h || fd<0){fail(Failure::Invalid);return kInvalidArgument;}
        for(const auto& f:fds)if(f.handler==h && f.fd==fd)return kResultOk;
        auto slot=std::find_if(fds.begin(),fds.end(),[](const Fd& f){return !f.handler;});
        if(slot==fds.end()){fail(Failure::Capacity);return kOutOfMemory;}
        if(recorded==records.size()){fail(Failure::Receipt);return kResultFalse;}
        const auto g=nextGeneration();if(!g || !retain(h))return kResultFalse;
        // Ref callbacks cannot mutate registry; publish only after ownership acquired.
        if(!healthy()){releaseHandler(h);return kResultFalse;}
        *slot={h,fd,g};record(Event::RegisterFd,g,fd);return healthy()?kResultOk:kResultFalse;
    }
    tresult PLUGIN_API unregisterEventHandler(IEventHandler* h) override {
        if(!mutableNow(true))return kResultFalse;
        if(!h){fail(Failure::Invalid);return kInvalidArgument;}
        std::array<IEventHandler*,capacity> removed{};std::size_t n=0;
        for(auto& f:fds)if(f.handler==h){removed[n++]=f.handler;record(Event::UnregisterFd,f.generation,f.fd);f={};}
        for(std::size_t i=0;i<n;++i)releaseHandler(removed[i]);
        return n?kResultOk:kResultFalse;
    }
    tresult PLUGIN_API registerTimer(ITimerHandler* h,TimerInterval ms) override {
        if(!mutableNow())return kResultFalse;
        if(!h || !ms){fail(Failure::Invalid);return kInvalidArgument;}
        if(ms>std::numeric_limits<uint64>::max()/1000000){fail(Failure::Overflow);return kInvalidArgument;}
        const uint64 interval=ms*1000000,now=clock.now();
        if(interval>std::numeric_limits<uint64>::max()-now){fail(Failure::Overflow);return kInvalidArgument;}
        for(const auto& t:timers)if(t.handler==h){if(t.interval==interval)return kResultOk;fail(Failure::Invalid);return kInvalidArgument;}
        auto slot=std::find_if(timers.begin(),timers.end(),[](const Timer& t){return !t.handler;});
        if(slot==timers.end()){fail(Failure::Capacity);return kOutOfMemory;}
        if(recorded==records.size()){fail(Failure::Receipt);return kResultFalse;}
        const auto g=nextGeneration();if(!g || !retain(h))return kResultFalse;
        if(!healthy()){releaseHandler(h);return kResultFalse;}
        *slot={h,interval,now+interval,g};record(Event::RegisterTimer,g);return healthy()?kResultOk:kResultFalse;
    }
    tresult PLUGIN_API unregisterTimer(ITimerHandler* h) override {
        if(!mutableNow(true))return kResultFalse;
        if(!h){fail(Failure::Invalid);return kInvalidArgument;}
        for(auto& t:timers)if(t.handler==h){const auto g=t.generation;t={};record(Event::UnregisterTimer,g);releaseHandler(h);return kResultOk;}
        return kResultFalse;
    }
    PumpResult pump(Budget budget) noexcept {
        PumpResult result;
        if(!mutableNow())return result;
        if(pumping){fail(Failure::Reentrant);return result;}
        if(!budget.callbacks || !budget.rounds){fail(Failure::Budget);return result;}
        pumping=true;
        for(uint32 round=0;round<budget.rounds && healthy();++round){
            if(clock.now()>=budget.deadline){fail(Failure::Deadline);break;}
            ++result.rounds;
            std::array<pollfd,capacity> polled{};std::array<std::size_t,capacity> indices{};nfds_t count=0;
            for(std::size_t i=0;i<capacity;++i)if(fds[i].handler){polled[count]={fds[i].fd,POLLIN,0};indices[count++]=i;}
            int status=::poll(polled.data(),count,0);
            if(status<0){fail(Failure::Poll);break;}
            std::array<Pending,capacity*2> ready{};std::size_t size=0;
            for(nfds_t i=0;i<count;++i){
                if(polled[i].revents&(POLLERR|POLLHUP|POLLNVAL)){fail(Failure::Hangup);break;}
                if(polled[i].revents&POLLIN){const auto slot=indices[i];const auto& f=fds[slot];ready[size++]={f.handler,f.generation,0,slot,f.fd,false};}
            }
            const auto now=clock.now();
            for(std::size_t i=0;i<capacity;++i){const auto& t=timers[i];if(t.handler && t.due<=now)ready[size++]={t.handler,t.generation,t.due,i,-1,true};}
            // FD callbacks first by registration order, then timers by deadline/ordinal.
            std::sort(ready.begin(),ready.begin()+size,[](const Pending& a,const Pending& b){if(a.timer!=b.timer)return !a.timer;if(a.due!=b.due)return a.due<b.due;return a.generation<b.generation;});
            std::size_t held=0;
            if(healthy())for(;held<size && healthy();++held)if(!retain(ready[held].handler))break;
            if(healthy() && size==0){result.sampledIdle=true;record(Event::SampleIdle);}
            for(std::size_t i=0;i<held && healthy();++i){
                const auto& p=ready[i];
                if(p.timer ? timers[p.slot].generation!=p.generation || !timers[p.slot].handler : fds[p.slot].generation!=p.generation || !fds[p.slot].handler)continue;
                if(clock.now()>=budget.deadline){fail(Failure::Deadline);break;}
                if(result.callbacks==budget.callbacks){fail(Failure::Budget);break;}
                uint64 skipped=0;
                if(p.timer){
                    auto& t=timers[p.slot];const auto current=clock.now();
                    const auto periods=(current-t.due)/t.interval;
                    if(periods==std::numeric_limits<uint64>::max() || periods+1>(std::numeric_limits<uint64>::max()-t.due)/t.interval){fail(Failure::Overflow);break;}
                    t.due+=(periods+1)*t.interval;skipped=periods;
                }
                record(Event::CallbackBegin,p.generation,p.fd,skipped);if(!healthy())break;
                ++result.callbacks;
                try{if(p.timer)static_cast<ITimerHandler*>(p.handler)->onTimer();else static_cast<IEventHandler*>(p.handler)->onFDIsSet(p.fd);}
                catch(...){fail(Failure::Callback);}
                record(Event::CallbackEnd,p.generation,p.fd);
            }
            for(std::size_t i=0;i<held;++i)releaseHandler(ready[i].handler);
            if(result.sampledIdle)break;
            if(round+1==budget.rounds && healthy())fail(Failure::Budget);
        }
        pumping=false;result.ok=healthy();return result;
    }
    bool shutdown() noexcept {
        if(!owner())return false;
        if(pumping || refBoundary){fail(Failure::Reentrant);return false;}
        if(live()){fail(Failure::Outstanding);return false;}
        closed=true;record(Event::Shutdown);return healthy();
    }
};
}
