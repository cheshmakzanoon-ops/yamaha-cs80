#define main original_initializer_main
#include "native_observer_initializer_fixture.cpp"
#undef main
#include "native_observer_flush.hpp"

int main(int argc,char** argv){try {
    require(argc==2,"case required");std::string test=argv[1];
    InitFake component("component",0),controller("controller",0);InitFactory f(component,controller);factory=&f;
    const bool separate=test=="separate-history" || test.find("controller-restore")!=std::string::npos;
    if(separate){component.combined=false;f.separate=true;f.classes=2;}
    Host host;FakeLoader loader;TUID cid{};std::memset(cid,5,16);InjectedModule module(loader);module.load("/sealed/fake.so",cid,host);
    Handler handler;InjectedInstance instance(module,handler);bool failed=false,outerStillLive=false,outerProcessorUsed=false;
    try {
        if(test=="receipt-spoof"){instance.started=true;instance.initializationSucceeded=true;instance.withProcessingOperation([](IAudioProcessor&,const SynthSetup&){});}
        else if(test=="fresh-processing")instance.withProcessingOperation([](IAudioProcessor&,const SynthSetup&){});
        else if(test=="fresh-prepare")instance.prepareAndStart({});
        else if(test=="fresh-suspend")instance.suspend();
        else if(test=="fresh-resume")instance.resume();
        else if(test=="fresh-restore")instance.restoreSavedState();
        else if(test=="invalid-wrapper-config")instance.start({1,kRealtime,64});
        else {
            instance.initializeConnected();
            require(instance.phase()==InjectedInstance::Phase::Connected && count("setup")==0 && count("activate")==0,"connected before setup");
            if(test=="duplicate-initialize")instance.initializeConnected();
            if(test=="connected-processing")instance.processingAudio();
            if(test=="invalid-split-config")instance.prepareAndStart({1,kRealtime,64});
            if(test=="metadata-allocation"){failure="audio-bus-info";badAllocationFailure=true;}
            if(test=="metadata-duplicate-id")component.parameterCount=2;
            instance.captureConnected();require(!instance.connectedMetadataReceipt().data.empty(),"owned metadata");require(instance.connectedParameterRoster().declaredCount==1 && instance.connectedParameterRoster().ids==std::vector<ParamID>{31},"same-enumeration typed parameter roster");require((separate?controller:component).parameterCountQueries==1 && (separate?controller:component).parameterInfoQueries==1,"roster from single enumeration");
            if(test=="duplicate-metadata")instance.captureConnected();
            instance.prepareAndStart({},true);
            require(!instance.setupMetricsReceipt().data.empty() && instance.processingSetup().rate==48000 && instance.processingSetup().block==64,"setup binding/metrics");
            if(test=="late-metadata")instance.captureConnected();
            if(test=="duplicate-prepare")instance.prepareAndStart({});
            if(test=="operation-exception")instance.withProcessingOperation([](IAudioProcessor&,const SynthSetup&){throw std::bad_alloc();});
            if(test=="operation-nested-caught")instance.withProcessingOperation([&](IAudioProcessor& audio,const SynthSetup&){
                try{instance.withProcessingOperation([](IAudioProcessor&,const SynthSetup&){});}catch(...){}
                outerStillLive=component.refs>0 && count("stop-processing")==0;
                if(outerStillLive)outerProcessorUsed=audio.canProcessSampleSize(kSample32)==kResultOk;
            });
            if(test=="operation-caught-reentrant")instance.withProcessingOperation([&](IAudioProcessor&,const SynthSetup&){try{instance.suspend();}catch(...){}require(component.refs>0,"cleanup deferred until active callback exits");});
            if(test=="operation-reentrant")instance.withProcessingOperation([&](IAudioProcessor&,const SynthSetup&){instance.suspend();});
            if(test=="flush" || test=="flush-invalid" || test=="flush-repeat"){
                FlushProtocol p;p.controls={{0,31,.5}};p.expectedIds={31};if(test=="flush-invalid")p.expectedIds={32};FlushReceipt receipt;
                flushObserved(instance,p,receipt);require(receipt.accepted,"real session template flush integration");
                if(test=="flush-repeat")flushObserved(instance,p,receipt);
            }
            if(test=="suspend-unsaved")instance.suspend();
            instance.saveState(ControllerStatePolicy::Supported);
            require(instance.savedStateReceipt().accepted,"owned same-instance state");
            if(test=="save-repeat")instance.saveState(ControllerStatePolicy::Supported);
            if(test=="restore-processing")instance.restoreSavedState();
            if(test.rfind("suspend-fail:",0)==0)failure=test.substr(13);
            if(test.rfind("suspend-throw:",0)==0){failure=test.substr(14);throwFailure=true;}
            instance.suspend();require(instance.phase()==InjectedInstance::Phase::Suspended && !instance.started,"suspended exact phase");
            if(test=="suspend-repeat")instance.suspend();
            if(test=="resume-unrestored")instance.resume();
            if(test=="suspended-operation")instance.withProcessingOperation([](IAudioProcessor&,const SynthSetup&){});
            if(test.rfind("restore-fail:",0)==0)failure=test.substr(13);
            if(test.rfind("restore-throw:",0)==0){failure=test.substr(14);throwFailure=true;}
            restoreTracing=true;instance.restoreSavedState();require(instance.restoreReceipt().accepted,"owned restore");
            if(test=="restore-repeat")instance.restoreSavedState();
            if(test.rfind("resume-fail:",0)==0)failure=test.substr(12);
            if(test.rfind("resume-throw:",0)==0){failure=test.substr(13);throwFailure=true;}
            instance.resume();require(instance.resumeCount()==1 && instance.processingSetup().mode==kRealtime,"first same-setup resume");
            instance.suspend();
            if(test=="second-restore")instance.restoreSavedState();
            instance.resume();require(instance.resumeCount()==2 && instance.suspensionCount()==2,"exact two transition histories");
            if(test=="third-suspend")instance.suspend();
            if(test=="extra-resume")instance.resume();
            require(count("setup")==1 && count("activate")==3 && count("start-processing")==3,"one setup three activations");
            require(count("component-restore")== (separate?1:2) && count("restore-sync")==1,"restore exact native call roster");
            if(test=="cleanup-allocation")releaseAllocationFailure=true;
            instance.close();
            if(test=="closed-access")instance.processingAudio();
        }
    }catch(...){failed=true;}
    const bool good=test=="history" || test=="separate-history" || test=="flush";
    require(failed!=good,"expected lifecycle acceptance/rejection");
    if(test=="operation-nested-caught")require(outerStillLive && outerProcessorUsed && count("stop-processing")==1,"nested rejection preserves live outer processor until unwind");
    require(component.refs==0 && controller.refs==0,"all interfaces released on completion or terminal failure");
    require(instance.calls.count<=InjectedInstance::maximumScheduledCalls,"fixed call schedule bound");
    if(!good){require(instance.phase()==InjectedInstance::Phase::Failed && instance.primaryError,"terminal primary failure receipt");
        const auto n=trace.size();try{instance.resume();}catch(...){}require(trace.size()==n,"failed session cannot retry native operation");}
    if(test=="invalid-wrapper-config")require(count("component-create")==0,"wrapper config validation precedes create");
    if(test.rfind("suspend-",0)==0 && test.find(':')!=std::string::npos){
        require(count("stop-processing")==1 && count("deactivate")==1 && count("event-bus-deactivate")==1 && count("audio-bus-deactivate")==1,"all suspension cleanup attempted once after failure");}
    module.close();require(loader.closes==1 && host.refs==1 && f.refs==0,"balanced module lifetime");
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
