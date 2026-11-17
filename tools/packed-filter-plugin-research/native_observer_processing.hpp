#pragma once
#include "native_observer_initializer.hpp"
#include "native_observer_worker.hpp"
#include "native_observer_flush.hpp"
namespace cs80_observer {
// UI-owned native instance + worker. External module/handler must outlive owner.
struct ProcessingCleanupSnapshot {
    InitializerCalls<96> lifecycle;std::array<NativeCloseResult,15> interfaces{};
    std::size_t interfaceCount=0;std::exception_ptr error;
};
class ProcessingInstanceOwner {
    enum class Violation {None,WrongThread,Nested,Closed,Failed};
    const std::thread::id ui_=std::this_thread::get_id();
    std::atomic<Violation> violation_{Violation::None};
    InjectedInstance instance_;
    struct Results {
        RenderProtocol renderProtocol;FlushProtocol flushProtocol;
        RenderObservation render;FlushReceipt flush;
    } results_;
    worker::ProcessingWorker worker_; // destroyed before results_/instance_
    bool operation_=false,leaseHeld_=false,closed_=false;
    std::exception_ptr firstError_,cleanupError_;
    std::optional<worker::TaskReceipt> taskReceipt_;
    std::optional<worker::JoinReceipt> joinReceipt_;
    void mark(Violation v) noexcept{auto expected=Violation::None;violation_.compare_exchange_strong(expected,v);}
    struct Guard {
        ProcessingInstanceOwner& owner;
        explicit Guard(ProcessingInstanceOwner& o):owner(o){
            if(std::this_thread::get_id()!=owner.ui_){owner.mark(Violation::WrongThread);throw worker::Error();}
            if(owner.operation_){owner.mark(Violation::Nested);throw worker::Error();}
            if(owner.closed_){owner.mark(Violation::Closed);throw worker::Error();}
            owner.operation_=true;
        }
        ~Guard(){owner.operation_=false;}
    };
    void check() const {require(violation_.load()==Violation::None && !firstError_,"owned processing session terminal");}
    // Call ONLY after a task publication/consumption proof or authoritative join.
    void releaseLease(){require(leaseHeld_ && instance_.operation_,"missing instance lease");instance_.operation_=false;leaseHeld_=false;}
    void cleanup(std::exception_ptr error={}) {
        if(error){mark(Violation::Failed);if(!firstError_)firstError_=error;}
        // If this throws/hangs, do NOT clear guard or release any native object.
        if(!joinReceipt_)joinReceipt_=worker_.close();
        require(worker_.authorizesCleanup(*joinReceipt_),"missing actual joined cleanup authority");
        if(leaseHeld_)releaseLease();
        if(firstError_){if(!instance_.primaryError)instance_.primaryError=firstError_;instance_.phase_=InjectedInstance::Phase::Failed;}
        if(!instance_.closeAttempted){try{instance_.close();}catch(...){cleanupError_=std::current_exception();mark(Violation::Failed);}}
        closed_=instance_.closeAttempted;
        if(cleanupError_)std::rethrow_exception(cleanupError_);
    }
    [[noreturn]] void abort(std::exception_ptr error){
        if(!firstError_)firstError_=error;
        mark(Violation::Failed);
        try{cleanup(error);}catch(...){if(!cleanupError_)cleanupError_=std::current_exception();}
        std::rethrow_exception(firstError_);
    }
    template<class F> void uiCall(F&& f){Guard guard(*this);try{check();f();check();}catch(...){abort(std::current_exception());}}
    struct LeaseView {
        IAudioProcessor& processor;SynthSetup setup;std::thread::id ui;bool invoked=false;
        template<class F> void withProcessingOperation(F&& f){
            require(std::this_thread::get_id()!=ui && !invoked,"private worker lease invocation");invoked=true;
            std::forward<F>(f)(processor,static_cast<const SynthSetup&>(setup));
        }
    };
    struct Task {
        Results* output;IAudioProcessor* processor;SynthSetup setup;std::thread::id ui;worker::Kind kind;
        void operator()()const{
            require(std::this_thread::get_id()!=ui,"native processing must use dedicated worker");
            if(kind==worker::Kind::Render){
                const auto& p=output->renderProtocol;
                require(p.rate==setup.rate && p.mode==setup.mode && p.block==setup.block,"render frozen setup mismatch");
                output->render=renderObserved(*processor,p);
            }else {LeaseView lease{*processor,setup,ui};flushObserved(lease,output->flushProtocol,output->flush);}
        }
    };
    Task begin(worker::Kind kind){
        check();instance_.phaseGuard(InjectedInstance::Phase::Processing);
        instance_.operation_=true;leaseHeld_=true;
        return Task{&results_,instance_.own.audio,instance_.setup_,ui_,kind};
    }
    template<class Pump> void execute(Task task,worker::WaitPolicy policy,Pump&& pump){
        check();const auto sequence=worker_.submit(task.kind,task);check();
        auto receipt=worker_.await(sequence,policy,std::forward<Pump>(pump));
        require(receipt.sequence()==sequence && receipt.kind()==task.kind && receipt.returned() && receipt.thread()!=ui_,"actual processing task correspondence");
        taskReceipt_=receipt;check();
        require(instance_.phase_==InjectedInstance::Phase::Processing && !instance_.primaryError,"leased instance became terminal");
    }
public:
    ProcessingInstanceOwner(InjectedModule& module,Handler& handler):instance_(module,handler){instance_.deferFailureCleanup_=true;}
    ProcessingInstanceOwner(const ProcessingInstanceOwner&)=delete;ProcessingInstanceOwner& operator=(const ProcessingInstanceOwner&)=delete;
    void initializeConnected(){uiCall([&]{instance_.initializeConnected();});}
    void captureConnected(){uiCall([&]{instance_.captureConnected();});}
    void prepareAndStart(SynthSetup setup,bool metrics=false){uiCall([&]{instance_.prepareAndStart(setup,metrics);});}
    void saveState(ControllerStatePolicy policy){uiCall([&]{instance_.saveState(policy);});}
    void suspend(){uiCall([&]{instance_.suspend();});}
    void restoreSavedState(){uiCall([&]{instance_.restoreSavedState();});}
    void resume(){uiCall([&]{instance_.resume();});}
    template<class Pump> RenderObservation render(const RenderProtocol& protocol,worker::WaitPolicy policy,Pump&& pump){
        Guard guard(*this);try{
            auto task=begin(worker::Kind::Render);results_.renderProtocol=protocol;results_.render={};
            execute(task,policy,std::forward<Pump>(pump));releaseLease();return std::move(results_.render);
        }catch(...){abort(std::current_exception());}
    }
    template<class Pump> FlushReceipt flush(const FlushProtocol& protocol,worker::WaitPolicy policy,Pump&& pump){
        Guard guard(*this);try{
            auto task=begin(worker::Kind::Flush);results_.flushProtocol=protocol;results_.flush={};
            require(!instance_.connectedRoster_.ids.empty() && instance_.connectedRoster_.declaredCount==static_cast<int32>(instance_.connectedRoster_.ids.size()) && protocol.expectedIds==instance_.connectedRoster_.ids,"flush binds owned complete parameter roster");
            execute(task,policy,std::forward<Pump>(pump));releaseLease();return std::move(results_.flush);
        }catch(...){abort(std::current_exception());}
    }
    void close(){Guard guard(*this);try{check();cleanup();}catch(...){abort(std::current_exception());}}
    // Read-only UI snapshots, no instance/processor/lease escape.
    ConnectedParameterRoster connectedRoster(){Guard guard(*this);try{check();return instance_.connectedRoster_;}catch(...){abort(std::current_exception());}}
    const char* failureReason()const noexcept{switch(violation_.load()){case Violation::None:return "none";case Violation::WrongThread:return "wrong-thread";case Violation::Nested:return "nested-operation";case Violation::Closed:return "closed";case Violation::Failed:return "operation-failed";}return "invalid";}
    ProcessingCleanupSnapshot cleanupReceipt()const{require(std::this_thread::get_id()==ui_ && !operation_ && closed_,"UI closed cleanup receipt");return {instance_.calls,instance_.own.results,instance_.own.resultCount,cleanupError_};}
    bool joinedForCleanup()const{require(std::this_thread::get_id()==ui_,"UI joined diagnostic");return joinReceipt_ && worker_.authorizesCleanup(*joinReceipt_);}
    std::optional<worker::TaskReceipt> taskReceipt()const{require(std::this_thread::get_id()==ui_ && !operation_,"UI idle receipt access");return taskReceipt_;}
    std::optional<worker::JoinReceipt> joinReceipt()const{require(std::this_thread::get_id()==ui_ && !operation_,"UI idle join receipt access");return joinReceipt_;}
    bool acceptedClosure()const {require(std::this_thread::get_id()==ui_ && !operation_,"UI idle closure access");return closed_ && !firstError_ && !cleanupError_ && violation_.load()==Violation::None && instance_.closedCleanly && joinReceipt_ && worker_.authorizesAdmission(*joinReceipt_);}
    std::exception_ptr primaryError()const{require(std::this_thread::get_id()==ui_ && !operation_,"UI idle error access");return firstError_;}
    ~ProcessingInstanceOwner(){
        if(std::this_thread::get_id()!=ui_ || operation_)std::terminate();
        if(!closed_){try{cleanup();}catch(...){} }
        // Failed join cannot permit member destruction/unload beneath live native work.
        if(leaseHeld_)std::terminate();
    }
};
}
