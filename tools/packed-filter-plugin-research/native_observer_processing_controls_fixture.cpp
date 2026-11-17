#define main original_processing_main
#include "native_observer_processing_fixture.cpp"
#undef main
FlushProtocol controls(std::initializer_list<ParamID> ids){FlushProtocol p;for(auto id:ids){p.expectedIds.push_back(id);p.controls.push_back({0,id,.5});}return p;}
Bytes golden(const FlushProtocol& p){Bytes b;b.text("cs80-control-input-v1");b.u32(static_cast<std::uint32_t>(kRealtime));b.u64(p.controls.size());for(const auto& q:p.controls){b.u32(q.id);b.f64(.5);}return b;}
int main(int argc,char** argv){try{
    require(argc==2,"case");const std::string test=argv[1];trace.reserve(1000);
    InitFake component("component",0),controller("controller",0);component.parameterIds={33,31,32};
    InitFactory f(component,controller);factory=&f;Host host;FakeLoader loader;TUID cid{};std::memset(cid,5,16);InjectedModule module(loader);module.load("/sealed/fake.so",cid,host);Handler handler;
    ProcessingInstanceOwner owner(module,handler);
    lifecycleHook=[&](const char* op){if(std::string(op)=="component-terminate")require(owner.joinedForCleanup(),"join before control/snapshot failure cleanup");};
    const bool good=test=="full-order-golden" || test=="full-reordered-golden" || test=="subset-success" || test=="snapshot-copy" || test=="restore-copy";
    bool rejected=false;int callsBeforeRejection=0;
    try{
        if(test=="snapshot-fresh")owner.savedStateSnapshot();
        owner.initializeConnected();owner.captureConnected();
        if(test=="snapshot-unprepared")owner.connectedSnapshot();
        owner.prepareAndStart({},test!="snapshot-no-metrics");
        if(test.rfind("restore-",0)==0){
            if(test=="restore-early")owner.restoreStateSnapshot();
            owner.saveState(ControllerStatePolicy::Supported);owner.suspend();
            if(test=="restore-failed"){failure="restore-sync";restoreTracing=true;require(failed([&]{owner.restoreSavedState();}),"native restore failure");owner.restoreStateSnapshot();}
            owner.restoreSavedState();auto restored=owner.restoreStateSnapshot();
            require(restored.attempted && restored.accepted && restored.component.returned && restored.componentSync.returned && restored.controller.returned,"actual restore calls retained");
            const auto bytes=restored.component.bytes;restored.component.bytes.clear();restored.accepted=false;
            require(owner.restoreStateSnapshot().component.bytes==bytes && owner.restoreStateSnapshot().accepted,"restore copy independence");
            owner.resume();require(owner.restoreStateSnapshot().component.bytes==bytes,"restore evidence after resume");
            owner.close();
            if(test=="restore-post-close")owner.restoreStateSnapshot();
            require(owner.acceptedClosure(),"restore healthy closure");
        }else if(test.rfind("snapshot-",0)==0){
            if(test=="snapshot-unsaved")owner.savedStateSnapshot();
            if(test=="snapshot-no-metrics")owner.connectedSnapshot();
            if(test=="snapshot-copy-oom"){failNextAllocation=true;owner.connectedSnapshot();}
            if(test=="snapshot-rejected-save"){component.badStream=true;owner.saveState(ControllerStatePolicy::Supported);}
            owner.saveState(ControllerStatePolicy::Supported);
            if(test=="snapshot-wrong-thread"){
                std::thread foreign([&]{require(failed([&]{owner.savedStateSnapshot();}),"foreign snapshot rejected");});foreign.join();owner.connectedSnapshot();
            }
            auto connected=owner.connectedSnapshot();auto state=owner.savedStateSnapshot();
            require(!connected.metadata.data.empty() && !connected.setupMetrics.data.empty() && connected.roster.ids==component.parameterIds,"owned metadata enumeration preserved");
            require(state.attempted && state.accepted && !state.component.bytes.empty(),"accepted saved observation");
            const auto originalMetadata=connected.metadata.data,originalSetup=connected.setupMetrics.data,originalState=state.component.bytes;
            connected.metadata.data.clear();connected.setupMetrics.data.clear();connected.roster.ids.clear();state.component.bytes.clear();state.accepted=false;
            const auto again=owner.connectedSnapshot();const auto saved=owner.savedStateSnapshot();
            require(again.metadata.data==originalMetadata && again.setupMetrics.data==originalSetup && again.roster.ids==component.parameterIds && saved.component.bytes==originalState && saved.accepted,"deep value snapshots do not mutate native-owned evidence");
            owner.close();require(owner.acceptedClosure(),"snapshot healthy closure");
        }else{
            auto full=controls({31,32,33});
            if(test=="full-reordered-golden")full=controls({32,33,31});
            if(test=="full-missing")full=controls({31,32});
            if(test=="full-extra")full=controls({31,32,33,34});
            if(test=="full-duplicate")full=controls({31,31,32});
            if(test=="full-positional")full.controls.front().id=33;
            if(test=="subset-before-full")owner.flushKnownSubset(controls({31,32}),waitPolicy(),[]{});
            if(test=="failed-full-subset"){
                processHook=[](ProcessData&){return kResultFalse;};require(failed([&]{owner.flush(full,waitPolicy(),[]{});}),"full native failure");
                callsBeforeRejection=processCalls;require(failed([&]{owner.flushKnownSubset(controls({31,32}),waitPolicy(),[]{});}),"failed full cannot authorize subset");
                require(processCalls==callsBeforeRejection && !owner.acceptedClosure(),"no subset after failed full");rejected=true;
            }else{
                processHook=[&](ProcessData& data){
                    require(data.numSamples==0 && data.inputParameterChanges->getParameterCount()==static_cast<int32>(full.expectedIds.size()),"full queue cardinality");
                    for(int32 i=0;i<data.inputParameterChanges->getParameterCount();++i)require(data.inputParameterChanges->getParameterData(i)->getParameterId()==full.expectedIds[static_cast<std::size_t>(i)],"full canonical protocol order not sorted to metadata");
                    return kResultOk;
                };
                const auto receipt=owner.flush(full,waitPolicy(),[]{});require(receipt.protocol==golden(full).data,"full protocol exact golden bytes");
                require(receipt.protocol!=golden(controls({33,31,32})).data,"metadata order not substituted for canonical controls");
                if(test.rfind("subset-",0)==0){
                    auto subset=controls({32,31});
                    if(test=="subset-unknown")subset=controls({31,34});
                    if(test=="subset-duplicate")subset=controls({31,31});
                    if(test=="subset-all")subset=controls({31,32,33});
                    if(test=="subset-empty")subset=controls({});
                    callsBeforeRejection=processCalls;
                    processHook=[&](ProcessData& data){require(data.inputParameterChanges->getParameterCount()==2 && data.inputParameterChanges->getParameterData(0)->getParameterId()==32 && data.inputParameterChanges->getParameterData(1)->getParameterId()==31,"subset exact protocol order");return kResultOk;};
                    const auto receipt2=owner.flushKnownSubset(subset,waitPolicy(),[]{});require(receipt2.accepted && receipt2.protocol==golden(subset).data,"subset own golden protocol");
                }
                owner.close();require(owner.acceptedClosure(),"controls healthy closure");
            }
        }
    }catch(...){rejected=true;}
    require(rejected!=good,"expected control/snapshot outcome");
    require(owner.joinedForCleanup() && component.refs==0 && controller.refs==0,"all outcomes join before native release");
    if(!good)require(!owner.acceptedClosure(),"invalid controls/snapshot never admission");
    if(test.rfind("full-",0)==0 && !good)require(processCalls==0,"bad full inputs before native process");
    if(test=="subset-before-full")require(processCalls==0,"no subset without full authority");
    if(test=="subset-unknown" || test=="subset-duplicate" || test=="subset-all" || test=="subset-empty")require(processCalls==callsBeforeRejection && processCalls==1,"invalid subset cannot process");
    if(test=="snapshot-rejected-save")require(failed([&]{owner.savedStateSnapshot();}),"failed saved state cannot be copied as accepted");
    lifecycleHook={};module.close();return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
