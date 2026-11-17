#pragma once
// Independent processing-thread ownership. No native module integration.
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <exception>
#include <functional>
#include <limits>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <thread>
#include <utility>
namespace cs80_observer::worker {
using Clock=std::chrono::steady_clock;
enum class Kind {Render,Flush};
enum class Failure {None,WrongThread,Reentrant,Closed,Outstanding,Sequence,InvalidKind,Allocation,Construction,Callback,Pump,Deadline,Budget,Unconsumed};
class Error:public std::runtime_error {public:Error():std::runtime_error("processing worker terminal failure") {}};
class ProcessingWorker;
class TaskReceipt {
    friend class ProcessingWorker;
    std::uint64_t identity_=0,sequence_=0;Kind kind_=Kind::Render;
    std::thread::id thread_;Clock::time_point completed_;std::exception_ptr exception_;
    bool returned_=false;
    TaskReceipt(std::uint64_t id,std::uint64_t seq,Kind kind,std::thread::id thread,Clock::time_point completed,std::exception_ptr exception)
        :identity_(id),sequence_(seq),kind_(kind),thread_(thread),completed_(completed),exception_(exception),returned_(!exception){}
public:
    std::uint64_t sequence() const noexcept{return sequence_;}
    Kind kind() const noexcept{return kind_;}
    std::thread::id thread() const noexcept{return thread_;}
    Clock::time_point completionTime() const noexcept{return completed_;}
    bool returned() const noexcept{return returned_;}
    std::exception_ptr exception() const noexcept{return exception_;}
};
class JoinReceipt {
    friend class ProcessingWorker;
    std::uint64_t identity_=0,submitted_=0,attempted_=0,completed_=0,consumed_=0,canceled_=0;
    Failure failure_=Failure::None;bool healthy_=false;
    JoinReceipt(std::uint64_t id,std::uint64_t submitted,std::uint64_t attempted,std::uint64_t completed,std::uint64_t consumed,std::uint64_t canceled,Failure failure)
        :identity_(id),submitted_(submitted),attempted_(attempted),completed_(completed),consumed_(consumed),canceled_(canceled),failure_(failure),healthy_(failure==Failure::None && submitted==completed && completed==consumed && canceled==0){}
public:
    std::uint64_t submitted() const noexcept{return submitted_;}
    std::uint64_t attempted() const noexcept{return attempted_;}
    std::uint64_t completed() const noexcept{return completed_;}
    std::uint64_t consumed() const noexcept{return consumed_;}
    std::uint64_t canceled() const noexcept{return canceled_;}
    Failure failure() const noexcept{return failure_;}
    bool healthy() const noexcept{return healthy_;}
};
struct WaitPolicy {
    Clock::time_point deadline;
    std::uint32_t maxPumpSteps=10000;
    std::chrono::milliseconds wakeQuantum{1};
};
class ProcessingWorker {
    inline static std::atomic<std::uint64_t> identities{0};
    static std::uint64_t nextIdentity(){
        auto n=identities.load();do{if(n==std::numeric_limits<std::uint64_t>::max())throw Error();}while(!identities.compare_exchange_weak(n,n+1));return n+1;
    }
    const std::uint64_t identity=nextIdentity();
    const std::thread::id ui=std::this_thread::get_id();
    mutable std::mutex mutex;
    std::condition_variable changed;
    std::thread thread;
    std::thread::id processingThread;
    bool started=false,operation=false,stop=false,joined=false,queued=false,dispatch=false,running=false,complete=false;
    Failure error=Failure::None;
    std::exception_ptr firstException;
    std::function<void()> slot;
    Kind currentKind=Kind::Render;
    std::uint64_t submitted=0,attempted=0,completed=0,consumed=0,canceled=0;
    std::optional<TaskReceipt> last;
    std::optional<JoinReceipt> final;
    void failLocked(Failure f,std::exception_ptr ex={}) noexcept {
        if(error==Failure::None)error=f;
        if(ex && !firstException)firstException=ex;
    }
    void fail(Failure f,std::exception_ptr ex={}) {std::lock_guard lock(mutex);failLocked(f,ex);changed.notify_all();}
    struct OperationGuard {
        ProcessingWorker& owner;
        explicit OperationGuard(ProcessingWorker& w):owner(w){
            std::lock_guard lock(owner.mutex);
            if(std::this_thread::get_id()!=owner.ui){owner.failLocked(Failure::WrongThread);throw Error();}
            if(owner.operation){owner.failLocked(Failure::Reentrant);throw Error();}
            owner.operation=true;
        }
        ~OperationGuard(){std::lock_guard lock(owner.mutex);owner.operation=false;}
    };
    void run() noexcept {
        {std::lock_guard lock(mutex);processingThread=std::this_thread::get_id();started=true;changed.notify_all();}
        for(;;){
            std::function<void()> job;Kind kind;std::uint64_t sequence;bool exit=false;
            {
                std::unique_lock lock(mutex);changed.wait(lock,[&]{return stop || dispatch;});
                if(stop || error!=Failure::None){if(queued){job.swap(slot);queued=false;dispatch=false;++canceled;}exit=true;}
                else {job.swap(slot);queued=false;dispatch=false;running=true;++attempted;kind=currentKind;sequence=submitted;}
            }
            if(exit)return; // discarded queued closure destroyed without mutex
            std::exception_ptr exception;
            try{job();}catch(...){exception=std::current_exception();}
            job={}; // foreign capture destructor runs on processing thread, outside mutex
            {
                std::lock_guard lock(mutex);const auto timestamp=Clock::now();running=false;complete=true;++completed;
                // Healthy prior receipts never contain foreign exceptions. A failed task is terminal.
                last=TaskReceipt(identity,sequence,kind,processingThread,timestamp,exception);
                if(exception)failLocked(Failure::Callback,exception);
                changed.notify_all();
            }
        }
    }
    void stopAndJoin(bool explicitClose) {
        if(std::this_thread::get_id()==thread.get_id())std::terminate();
        {
            std::lock_guard lock(mutex);
            if(explicitClose && (queued || running || complete))failLocked(Failure::Unconsumed);
            stop=true;changed.notify_all();
        }
        if(thread.joinable())thread.join();
        std::lock_guard lock(mutex);joined=true;
        if(explicitClose)final=JoinReceipt(identity,submitted,attempted,completed,consumed,canceled,error);
    }
public:
    ProcessingWorker(){
        thread=std::thread([this]{run();});
        try{std::unique_lock lock(mutex);changed.wait(lock,[&]{return started;});}
        catch(...){stopAndJoin(false);throw;}
    }
    ~ProcessingWorker(){if(thread.joinable())stopAndJoin(false);}
    ProcessingWorker(const ProcessingWorker&)=delete;ProcessingWorker& operator=(const ProcessingWorker&)=delete;
    template<class F> std::uint64_t submit(Kind kind,F&& callable){
        OperationGuard guard(*this);
        {
            std::lock_guard lock(mutex);
            if(stop || joined){failLocked(Failure::Closed);throw Error();}
            if(error!=Failure::None)throw Error();
            if(kind!=Kind::Render && kind!=Kind::Flush){failLocked(Failure::InvalidKind);throw Error();}
            if(queued || running || complete){failLocked(Failure::Outstanding);throw Error();}
            if(submitted==std::numeric_limits<std::uint64_t>::max()){failLocked(Failure::Sequence);throw Error();}
        }
        std::function<void()> candidate;
        try{candidate=std::function<void()>(std::forward<F>(callable));}
        catch(const std::bad_alloc&){fail(Failure::Allocation,std::current_exception());throw;}
        catch(...){fail(Failure::Construction,std::current_exception());throw;}
        if(!candidate){fail(Failure::Allocation);throw Error();}
        std::uint64_t sequence;
        {
            std::lock_guard lock(mutex);if(error!=Failure::None)throw Error();
            slot.swap(candidate);queued=true;currentKind=kind;sequence=++submitted;
        }
        return sequence; // queued only; await authorizes actual dispatch
    }
    template<class Pump> TaskReceipt await(std::uint64_t sequence,WaitPolicy policy,Pump&& pump){
        OperationGuard guard(*this);
        {
            std::lock_guard lock(mutex);
            if(stop || joined){failLocked(Failure::Closed);throw Error();}
            if(error!=Failure::None)throw Error();
            if(!queued || sequence!=submitted){failLocked(Failure::Sequence);throw Error();}
            if(!policy.maxPumpSteps || policy.wakeQuantum.count()<=0 || policy.wakeQuantum>std::chrono::seconds(1)){failLocked(Failure::Budget);throw Error();}
            if(Clock::now()>=policy.deadline){failLocked(Failure::Deadline);throw Error();}
            dispatch=true;changed.notify_all();
        }
        std::uint32_t pumps=0;
        for(;;){
            {
                std::unique_lock lock(mutex);
                if(error!=Failure::None)throw Error();
                if(complete){
                    if(last->completed_>policy.deadline){failLocked(Failure::Deadline);throw Error();}
                    complete=false;++consumed;return *last;
                }
                if(Clock::now()>=policy.deadline){failLocked(Failure::Deadline);throw Error();}
                if(pumps==policy.maxPumpSteps){failLocked(Failure::Budget);throw Error();}
            }
            ++pumps;
            try{pump();}catch(...){fail(Failure::Pump,std::current_exception());throw;}
            std::unique_lock lock(mutex);
            const auto now=Clock::now();
            const auto wake=(policy.deadline-now<=policy.wakeQuantum)?policy.deadline:now+policy.wakeQuantum;
            changed.wait_until(lock,wake,[&]{return complete || error!=Failure::None;});
        }
    }
    JoinReceipt close(){
        OperationGuard guard(*this);
        {std::lock_guard lock(mutex);if(joined || stop){failLocked(Failure::Closed);throw Error();}}
        stopAndJoin(true);
        std::lock_guard lock(mutex);return *final;
    }
    bool authorizesCleanup(const JoinReceipt& receipt) const {
        std::lock_guard lock(mutex);return final && joined && !thread.joinable() && receipt.identity_==identity;
    }
    bool authorizesAdmission(const JoinReceipt& receipt) const {
        std::lock_guard lock(mutex);return final && joined && !thread.joinable() && receipt.identity_==identity && final->healthy_ && receipt.healthy_ && error==Failure::None;
    }
    std::optional<TaskReceipt> lastTask() const {std::lock_guard lock(mutex);return last;}
    Failure failure() const {std::lock_guard lock(mutex);return error;}
    std::exception_ptr exception() const {std::lock_guard lock(mutex);return firstException;}
};
}
