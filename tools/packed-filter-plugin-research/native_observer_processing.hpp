#pragma once
#include "native_observer_initializer.hpp"
#include "native_observer_worker.hpp"
#include "native_observer_flush.hpp"
namespace cs80_observer {
// UI-owned native instance + worker. External module/handler must outlive owner.
struct ProcessingConnectedSnapshot {Bytes metadata,setupMetrics;ConnectedParameterRoster roster;};
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
    bool operation_=false,leaseHeld_=false,closed_=false,completeFlushAccepted_=false;
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
    void bindFlush(const FlushProtocol& protocol,bool complete) {
        validateFlush(protocol);const auto& roster=instance_.connectedRoster_;
        require(!roster.ids.empty() && roster.declaredCount==static_cast<int32>(roster.ids.size()),"owned complete native roster required");
        const std::set<ParamID> known(roster.ids.begin(),roster.ids.end()),ids(protocol.expectedIds.begin(),protocol.expectedIds.end());
        require(known.size()==roster.ids.size() && ids.size()==protocol.expectedIds.size(),"owned/flush ID uniqueness");
        if(complete)require(ids==known,"full flush complete ID-set binding");
        else require(completeFlushAccepted_ && !ids.empty() && ids.size()<known.size() && std::includes(known.begin(),known.end(),ids.begin(),ids.end()),"known subset requires prior complete flush");
    }
    template<class Pump> FlushReceipt flushImpl(const FlushProtocol& protocol,worker::WaitPolicy policy,Pump&& pump,bool complete){
        Guard guard(*this);try{
            auto task=begin(worker::Kind::Flush);bindFlush(protocol,complete);results_.flushProtocol=protocol;results_.flush={};
            execute(task,policy,std::forward<Pump>(pump));releaseLease();if(complete)completeFlushAccepted_=true;return std::move(results_.flush);
        }catch(...){abort(std::current_exception());}
    }
    template<class Pump> void execute(Task task,worker::WaitPolicy policy,Pump&& pump){
        check();const auto sequence=worker_.submit(task.kind,task);check();
        auto receipt=worker_.await(sequence,policy,std::forward<Pump>(pump));
        require(receipt.sequence()==sequence && receipt.kind()==task.kind && receipt.returned() && receipt.thread()!=ui_,"actual processing task correspondence");
        taskReceipt_=receipt;check();
        require(instance_.phase_==InjectedInstance::Phase::Processing && !instance_.primaryError,"leased instance became terminal");
    }
public:
    ProcessingInstanceOwner(InjectedModule& module,Handler& handler,ControllerQueryPolicy q=ControllerQueryPolicy::StrictSdk,AudioInputTopology topology=AudioInputTopology::ZeroAudioInput):instance_(module,handler,q,topology){instance_.deferFailureCleanup_=true;}
    ControllerQueryObservation controllerQueryObservation()const noexcept{return instance_.controllerQueryObservation();}
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
            require(protocol.topology==instance_.audioInputTopology(),"render input topology differs from owned lifecycle");auto task=begin(worker::Kind::Render);results_.renderProtocol=protocol;results_.render={};
            execute(task,policy,std::forward<Pump>(pump));releaseLease();return std::move(results_.render);
        }catch(...){abort(std::current_exception());}
    }
    template<class Pump> FlushReceipt flush(const FlushProtocol& protocol,worker::WaitPolicy policy,Pump&& pump){return flushImpl(protocol,policy,std::forward<Pump>(pump),true);}
    template<class Pump> FlushReceipt flushKnownSubset(const FlushProtocol& protocol,worker::WaitPolicy policy,Pump&& pump){return flushImpl(protocol,policy,std::forward<Pump>(pump),false);}
    ProcessingConnectedSnapshot connectedSnapshot(){
        Guard guard(*this);try{check();require(instance_.phase_==InjectedInstance::Phase::Processing || instance_.phase_==InjectedInstance::Phase::Suspended,"connected snapshot requires prepared phase");
            require(instance_.metadataAttempted_ && !instance_.connectedMetadata_.data.empty() && !instance_.setupMetrics_.data.empty() && instance_.initializationSucceeded,"accepted owned metadata/setup snapshot required");
            return {instance_.connectedMetadata_,instance_.setupMetrics_,instance_.connectedRoster_};
        }catch(...){abort(std::current_exception());}
    }
    StateObservation savedStateSnapshot(){
        Guard guard(*this);try{check();require(instance_.phase_==InjectedInstance::Phase::Processing || instance_.phase_==InjectedInstance::Phase::Suspended,"state snapshot requires prepared phase");
            require(instance_.saved_.attempted,"state snapshot unattempted");validateStateObservation(instance_.saved_);require(instance_.saved_.accepted,"state snapshot rejected");return instance_.saved_;
        }catch(...){abort(std::current_exception());}
    }
    StateRestoreReceipt restoreStateSnapshot(){
        Guard guard(*this);try{check();require(instance_.phase_==InjectedInstance::Phase::Processing || instance_.phase_==InjectedInstance::Phase::Suspended,"restore snapshot requires prepared phase");
            require(instance_.restored_.attempted,"restore snapshot unattempted");require(instance_.restored_.accepted,"restore snapshot rejected");return instance_.restored_;
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
