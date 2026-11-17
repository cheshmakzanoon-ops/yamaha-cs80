#pragma once
#include "native_observer_processing.hpp"
#include "workload_bridge.hpp"
namespace cs80_observer {
enum class HistoryStage {NotStarted,Connected,Prepared,InitialFlushed,A,PostASaved,AlterFlushed,B,FirstRestored,C,SecondResumed,D,Closed,Failed};
struct HistoryObservation {
    HistoryStage lastCompleted=HistoryStage::NotStarted;const char* attemptedOperation="none";
    std::vector<std::uint8_t> caseIdentity,initialProtocol,alterProtocol,renderProtocol;
    Bytes connectedMetadata,setupMetrics;ConnectedParameterRoster roster;
    std::array<RenderObservation,4> rendered;std::array<Bytes,4> encodedRender;
    std::array<bool,4> renderPresent{};
    FlushReceipt initialFlush,alterFlush;Bytes encodedInitialFlush,encodedAlterFlush;
    StateObservation postAState;Bytes encodedPostAState;StateRestoreReceipt restoredState;Bytes canonicalPayload;
    std::array<std::optional<worker::TaskReceipt>,6> tasks;
    std::optional<ProcessingCleanupSnapshot> cleanup;std::optional<worker::JoinReceipt> joined;
};
// Owns this fixed instance/worker history, NOT module/load/runloop lifetime.
// External module and handler must remain live until this owner is destroyed.
class HistoryOwner final {
    const std::thread::id ui_=std::this_thread::get_id();
    const packed_filter_workload::Case case_;const ControllerStatePolicy policy_;
    ProcessingInstanceOwner owner_;HistoryObservation evidence_;
    std::shared_ptr<const HistoryObservation> final_;
    HistoryStage stage_=HistoryStage::NotStarted;bool attempted_=false,running_=false;
    std::atomic<bool> violation_{false};std::exception_ptr primary_,cleanupError_;
    void check(){if(std::this_thread::get_id()!=ui_){violation_=true;throw std::runtime_error("history wrong thread");}require(!violation_ && !primary_,"history terminal");}
    void advance(HistoryStage stage){stage_=stage;evidence_.lastCompleted=stage;}
    void task(std::size_t index,worker::Kind kind){
        auto receipt=owner_.taskReceipt();require(receipt && receipt->sequence()==index+1 && receipt->kind()==kind && receipt->returned() && receipt->thread()!=ui_,"exact history task sequence/kind");
        require(index==0 || (evidence_.tasks[0] && receipt->thread()==evidence_.tasks[0]->thread()),"one owned processing thread across history");
        evidence_.tasks[index]=std::move(receipt);
    }
    void freeze(){
        Bytes b;b.text("cs80-instance-history-A-D-v1");
        const auto blob=[&](const auto& bytes){b.blob(bytes.data(),bytes.size());};
        blob(evidence_.caseIdentity);blob(evidence_.initialProtocol);blob(evidence_.alterProtocol);blob(evidence_.renderProtocol);
        blob(evidence_.connectedMetadata.data);blob(evidence_.setupMetrics.data);blob(evidence_.encodedInitialFlush.data);blob(evidence_.encodedAlterFlush.data);blob(evidence_.encodedPostAState.data);
        for(std::size_t i=0;i<4;++i){require(evidence_.renderPresent[i] && !evidence_.encodedRender[i].data.empty(),"complete history render evidence");blob(evidence_.encodedRender[i].data);}
        for(const auto& receipt:evidence_.tasks){require(receipt && receipt->returned() && !receipt->exception(),"complete successful task evidence");b.u64(receipt->sequence());b.u32(receipt->kind()==worker::Kind::Flush?1:2);}
        require(evidence_.restoredState.attempted && evidence_.restoredState.accepted,"complete owned restore evidence");
        for(const auto* r:{&evidence_.restoredState.component,&evidence_.restoredState.componentSync,&evidence_.restoredState.controller}){b.u32(r->attempted?1:0);if(r->attempted){require(r->returned && !r->exception && !r->streamFailed && r->result==kResultOk,"successful restore call receipt");b.u32(static_cast<std::uint32_t>(r->result));blob(r->bytes);}}
        require(evidence_.cleanup && !evidence_.cleanup->error && evidence_.joined,"successful close evidence");const auto& close=*evidence_.cleanup;
        require(close.lifecycle.count<=close.lifecycle.rows.size() && close.interfaceCount<=close.interfaces.size(),"bounded close evidence");
        b.u64(close.lifecycle.count);for(std::size_t i=0;i<close.lifecycle.count;++i){const auto& r=close.lifecycle.rows[i];require(r.name && r.normal && r.ok && !r.error,"successful lifecycle evidence");b.text(r.name);b.u64(static_cast<std::uint64_t>(r.returned));}
        b.u64(close.interfaceCount);for(std::size_t i=0;i<close.interfaceCount;++i){const auto& r=close.interfaces[i];require(r.operation && r.returnedNormally && r.ok,"successful interface close evidence");b.text(r.operation);b.u64(static_cast<std::uint64_t>(r.returned));}
        b.u64(evidence_.joined->submitted());b.u64(evidence_.joined->attempted());b.u64(evidence_.joined->completed());b.u64(evidence_.joined->consumed());b.u64(evidence_.joined->canceled());
        evidence_.canonicalPayload=std::move(b);final_=std::make_shared<const HistoryObservation>(std::move(evidence_));
    }
    void cleanup(){
        if(!owner_.joinedForCleanup())owner_.close();
        evidence_.cleanup=owner_.cleanupReceipt();evidence_.joined=owner_.joinReceipt();if(evidence_.cleanup->error && !cleanupError_)cleanupError_=evidence_.cleanup->error;
    }
public:
    HistoryOwner(InjectedModule& module,Handler& handler,packed_filter_workload::Case c,ControllerStatePolicy policy)
        :case_(c),policy_(policy),owner_(module,handler){}
    HistoryOwner(const HistoryOwner&)=delete;HistoryOwner& operator=(const HistoryOwner&)=delete;
    template<class Pump> void run(worker::WaitPolicy budget,Pump&& pump){
        if(std::this_thread::get_id()!=ui_){violation_=true;throw std::runtime_error("history wrong thread");}
        if(running_){violation_=true;throw std::runtime_error("history reentry");}
        running_=true;struct Reset {bool& flag;~Reset(){flag=false;}} reset{running_};
        try{
            check();require(!attempted_,"history retry forbidden");attempted_=true;
            packed_filter_workload::validate(case_);validateStatePolicy(policy_);
            evidence_.caseIdentity=packed_filter_workload::encode_draft(case_);
            evidence_.attemptedOperation="initialize-connected";owner_.initializeConnected();evidence_.attemptedOperation="capture-connected";owner_.captureConnected();advance(HistoryStage::Connected);check();
            evidence_.roster=owner_.connectedRoster();
            const auto plan=workload_detail::planFromRoster(case_,evidence_.roster);
            evidence_.initialProtocol=encodeFlushProtocol(plan.initial).data;evidence_.alterProtocol=encodeFlushProtocol(plan.alter).data;evidence_.renderProtocol=encodeRenderProtocol(plan.render).data;
            evidence_.attemptedOperation="prepare-start";owner_.prepareAndStart(plan.setup,true);const auto metadata=owner_.connectedSnapshot();evidence_.connectedMetadata=metadata.metadata;evidence_.setupMetrics=metadata.setupMetrics;require(metadata.roster.ids==evidence_.roster.ids && metadata.roster.declaredCount==evidence_.roster.declaredCount,"owned metadata roster drift");advance(HistoryStage::Prepared);check();
            evidence_.attemptedOperation="initial-flush";evidence_.initialFlush=owner_.flush(plan.initial,budget,pump);task(0,worker::Kind::Flush);evidence_.encodedInitialFlush=encodeFlush(plan.initial,evidence_.initialFlush);advance(HistoryStage::InitialFlushed);check();
            const auto render=[&](std::size_t index,std::size_t sequence,HistoryStage stage){
                const char* names[]{"render-A","render-B","render-C","render-D"};evidence_.attemptedOperation=names[index];
                evidence_.rendered[index]=owner_.render(plan.render,budget,pump);evidence_.renderPresent[index]=true;task(sequence,worker::Kind::Render);
                evidence_.encodedRender[index]=encodeRenderObservation(plan.render,evidence_.rendered[index]);advance(stage);check();
            };
            render(0,1,HistoryStage::A);
            evidence_.attemptedOperation="post-A-state";owner_.saveState(policy_);evidence_.postAState=owner_.savedStateSnapshot();evidence_.encodedPostAState=encodeStates(evidence_.postAState);advance(HistoryStage::PostASaved);check();
            // This is EXACT bridge alter (two IDs/values), never caller-supplied subset.
            evidence_.attemptedOperation="alter-flush";evidence_.alterFlush=owner_.flushKnownSubset(plan.alter,budget,pump);task(2,worker::Kind::Flush);evidence_.encodedAlterFlush=encodeFlush(plan.alter,evidence_.alterFlush);advance(HistoryStage::AlterFlushed);check();
            render(1,3,HistoryStage::B);
            evidence_.attemptedOperation="first-suspend";owner_.suspend();evidence_.attemptedOperation="first-restore";owner_.restoreSavedState();evidence_.restoredState=owner_.restoreStateSnapshot();evidence_.attemptedOperation="first-resume";owner_.resume();advance(HistoryStage::FirstRestored);check();
            render(2,4,HistoryStage::C);
            evidence_.attemptedOperation="second-suspend";owner_.suspend();evidence_.attemptedOperation="second-resume";owner_.resume();advance(HistoryStage::SecondResumed);check();
            render(3,5,HistoryStage::D);
            evidence_.attemptedOperation="joined-close";cleanup();require(owner_.acceptedClosure() && evidence_.joined && evidence_.joined->submitted()==6 && evidence_.joined->attempted()==6 && evidence_.joined->completed()==6 && evidence_.joined->consumed()==6 && evidence_.joined->canceled()==0,"history requires six actual joined tasks and successful instance close");advance(HistoryStage::Closed);check();freeze();
        }catch(...){if(!primary_)primary_=std::current_exception();stage_=HistoryStage::Failed;
            try{cleanup();}catch(...){cleanupError_=std::current_exception();}
            std::rethrow_exception(primary_);
        }
    }
    bool admitted()const{require(std::this_thread::get_id()==ui_ && !running_,"history UI idle receipt");return final_ && stage_==HistoryStage::Closed && !primary_ && !cleanupError_ && !violation_ && owner_.acceptedClosure();}
    const HistoryObservation& observation()const{require(std::this_thread::get_id()==ui_ && !running_,"history UI idle evidence");return final_?*final_:evidence_;}
    HistoryStage stage()const{require(std::this_thread::get_id()==ui_ && !running_,"history UI idle stage");return stage_;}
    std::exception_ptr primaryError()const{require(std::this_thread::get_id()==ui_ && !running_,"history UI idle error");return primary_;}
    std::exception_ptr cleanupError()const{require(std::this_thread::get_id()==ui_ && !running_,"history UI idle cleanup error");return cleanupError_;}
};
}
