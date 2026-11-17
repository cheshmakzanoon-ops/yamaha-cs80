// Inert initializer SDK fakes. Loader operations never touch an actual module.
#define INIT_CLASS_IID
#include "native_observer_processing.hpp"
#include <latch>
#include <cstdlib>
#include <memory>
#include <iostream>
using namespace cs80_observer;
static const auto uiThread=std::this_thread::get_id();
static std::atomic<bool> processActive=false,processExited=false;
static std::atomic<int> processCalls=0;
static std::function<tresult(ProcessData&)> processHook;
static std::function<void(const char*)> lifecycleHook;
static std::atomic<bool> failNextAllocation=false;
void* operator new(std::size_t n){if(failNextAllocation.exchange(false))throw std::bad_alloc{};if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc{};}
void operator delete(void* p) noexcept{std::free(p);}void operator delete(void* p,std::size_t) noexcept{std::free(p);}
void* operator new[](std::size_t n){return ::operator new(n);}void operator delete[](void* p) noexcept{::operator delete(p);}void operator delete[](void* p,std::size_t) noexcept{::operator delete(p);}
static std::vector<std::string> trace;
static std::string failure;
static bool faultObserved=false;
static bool throwFailure=false,restoreTracing=false,badAllocationFailure=false,releaseAllocationFailure=false;
static bool pass(const char* name){require(std::this_thread::get_id()==uiThread && !processActive,"native lifecycle UI and outside process");if(lifecycleHook)lifecycleHook(name);trace.push_back(name);if(failure==name)faultObserved=true;if(failure==name && badAllocationFailure)throw std::bad_alloc();if(failure==name && throwFailure)throw std::runtime_error("injected native exception");return failure!=name;}
struct InitFake final : IComponent, IAudioProcessor, IEditController, IConnectionPoint {
    std::string name;uint32 refs;int terminated=0;bool throwStop=false,failStop=false,failActive=false,failDisconnect=false,failTerminate=false;
    bool combined=true;bool busActive[2]{false,false};
    ObserverPhase phase=ObserverPhase::Created;int classQueries=0,metricsQueries=0;
    bool badStream=false;double normalized=.25,defaultNormalized=.5;int parameterCount=1,parameterCountQueries=0,parameterInfoQueries=0;
    explicit InitFake(std::string n,uint32 r):name(std::move(n)),refs(r){}
    tresult PLUGIN_API queryInterface(const TUID id,void** p) override {
        if(!p){return kInvalidArgument;}*p=nullptr;
        if(std::memcmp(id,IEditController::iid,16)==0){if(!pass("combined-query"))return kInternalError;if(!combined)return kNoInterface;*p=static_cast<IEditController*>(this);}
        else if(std::memcmp(id,IAudioProcessor::iid,16)==0){if(!pass("audio-query"))return kInternalError;*p=static_cast<IAudioProcessor*>(this);}
        else if(std::memcmp(id,FUnknown::iid,16)==0){if(!pass(name=="controller"?"controller-identity":"component-identity"))return kInternalError;*p=static_cast<FUnknown*>(static_cast<IComponent*>(this));}
        else if(std::memcmp(id,IConnectionPoint::iid,16)==0){if(!pass(name=="controller"?"controller-connection-query":"component-connection-query"))return kInternalError;*p=static_cast<IConnectionPoint*>(this);}
        else return kNoInterface;
        addRef();return kResultOk;
    }
    uint32 PLUGIN_API addRef() override {return ++refs;}
    uint32 PLUGIN_API release() override {require(std::this_thread::get_id()==uiThread && !processActive,"release on UI after process");if(faultObserved && lifecycleHook)lifecycleHook("failure-interface-release");trace.push_back("interface-release");require(refs>0,"reference underflow");--refs;if(releaseAllocationFailure)throw std::bad_alloc();return refs;}
    tresult PLUGIN_API initialize(FUnknown* host) override {require(host,"mandatory host");if(!pass(name=="controller"?"controller-initialize":"component-initialize"))return kResultFalse;phase=ObserverPhase::Initialized;return kResultOk;}
    tresult PLUGIN_API terminate() override {const bool result=pass(name=="controller"?"controller-terminate":"component-terminate");++terminated;return !result || failTerminate || terminated>1?kResultFalse:kResultOk;}
    tresult PLUGIN_API getControllerClassId(TUID id) override {++classQueries;if(!pass("controller-class-id"))return kResultFalse;if(phase!=ObserverPhase::Created)return kResultFalse;std::memset(id,7,sizeof(TUID));return kResultOk;}
    tresult PLUGIN_API setIoMode(IoMode) override {return kResultOk;}
    int32 PLUGIN_API getBusCount(MediaType media,BusDirection direction) override {require(phase==ObserverPhase::Connected,"fake bus phase");return (media==kAudio && direction==kOutput) || (media==kEvent && direction==kInput)?1:0;}
    tresult PLUGIN_API getBusInfo(MediaType m,BusDirection d,int32,BusInfo& b) override {if(!pass(m==kAudio?"audio-bus-info":"event-bus-info"))return kResultFalse;b={};b.mediaType=m;b.direction=d;b.channelCount=2;b.name[0]='O';return kResultOk;}
    tresult PLUGIN_API getRoutingInfo(RoutingInfo&,RoutingInfo&) override {return kNotImplemented;}
    tresult PLUGIN_API activateBus(MediaType media,BusDirection,int32,TBool active) override {
        const int i=media==kAudio?0:1;const char* op=active?(i==0?"audio-bus-activate":"event-bus-activate"):(i==0?"audio-bus-deactivate":"event-bus-deactivate");
        if(!pass(op))return kResultFalse;
        require(phase==ObserverPhase::Setup,"bus activation/deactivation Setup phase");busActive[i]=active;return kResultOk;
    }
    tresult PLUGIN_API setActive(TBool active) override {if(!pass(active?"activate":"deactivate"))return kResultFalse;phase=active?ObserverPhase::Activated:ObserverPhase::Setup;return kResultOk;}
    tresult PLUGIN_API setState(IBStream*) override {return pass(name=="controller"?"controller-restore":"component-restore")?kResultOk:kResultFalse;}
    tresult PLUGIN_API getState(IBStream* s) override {if(!pass("component-state"))return kResultFalse;char raw[]{'A',0,'Z'};int32 n=0;s->write(badStream?nullptr:raw,3,&n);return kResultOk;}
    tresult PLUGIN_API setBusArrangements(SpeakerArrangement*,int32 inputs,SpeakerArrangement* output,int32 outputs) override {require(inputs==0 && outputs==1 && output && *output==SpeakerArr::kStereo,"exact stereo arrangement");return pass("bus-arrangements")?kResultOk:kResultFalse;}
    tresult PLUGIN_API getBusArrangement(BusDirection,int32,SpeakerArrangement& a) override {a=3;return pass("bus-readback")?kResultOk:kResultFalse;}
    tresult PLUGIN_API canProcessSampleSize(int32 size) override {require(size==kSample32,"32-bit samples");return pass("sample-size")?kResultOk:kResultFalse;}
    uint32 PLUGIN_API getLatencySamples() override {++metricsQueries;require(phase==ObserverPhase::Setup,"fake metrics before setup");return 17;}
    tresult PLUGIN_API setupProcessing(ProcessSetup& p) override {require(phase==ObserverPhase::Connected && p.sampleRate==48000 && p.maxSamplesPerBlock==64,"frozen setup");if(!pass("setup"))return kResultFalse;phase=ObserverPhase::Setup;return kResultOk;}
    tresult PLUGIN_API setProcessing(TBool processing) override {if(!pass(processing?"start-processing":"stop-processing"))return kResultFalse;require(phase==(processing?ObserverPhase::Activated:ObserverPhase::Processing),"processing phase");phase=processing?ObserverPhase::Processing:ObserverPhase::Activated;return kResultOk;}
    tresult PLUGIN_API process(ProcessData& data) override {
        require(std::this_thread::get_id()!=uiThread,"native process dedicated thread");++processCalls;processActive=true;
        struct Exit {~Exit(){processActive=false;processExited=true;}} exit;
        return processHook?processHook(data):kResultOk;
    }
    uint32 PLUGIN_API getTailSamples() override {++metricsQueries;require(phase==ObserverPhase::Setup,"fake metrics before setup");return 23;}
    tresult PLUGIN_API setComponentState(IBStream* stream) override {int64 pos=-1;require(stream && stream->tell(&pos)==kResultOk && pos==0,"rewound component state");return pass(restoreTracing?"restore-sync":"controller-state")?kResultOk:kResultFalse;}
    int32 PLUGIN_API getParameterCount() override {++parameterCountQueries;require(phase==ObserverPhase::Connected,"fake parameter phase");return parameterCount;}
    tresult PLUGIN_API getParameterInfo(int32,ParameterInfo& p) override {++parameterInfoQueries;p={};p.id=31;p.title[0]='P';p.defaultNormalizedValue=defaultNormalized;return kResultOk;}
    tresult PLUGIN_API getParamStringByValue(ParamID,ParamValue,String128) override {return kNotImplemented;}
    tresult PLUGIN_API getParamValueByString(ParamID,TChar*,ParamValue&) override {return kNotImplemented;}
    ParamValue PLUGIN_API normalizedParamToPlain(ParamID,ParamValue v) override {return v;}
    ParamValue PLUGIN_API plainParamToNormalized(ParamID,ParamValue v) override {return v;}
    ParamValue PLUGIN_API getParamNormalized(ParamID) override {return normalized;}
    tresult PLUGIN_API setParamNormalized(ParamID,ParamValue) override {return kResultOk;}
    tresult PLUGIN_API setComponentHandler(IComponentHandler* h) override {if(!pass(h?"handler-attach":"handler-detach"))return kResultFalse;if(h)phase=ObserverPhase::Connected;return kResultOk;}
    IPlugView* PLUGIN_API createView(FIDString) override {return nullptr;}
    tresult PLUGIN_API connect(IConnectionPoint*) override {if(!pass(name=="controller"?"controller-connect":"component-connect"))return kResultFalse;phase=ObserverPhase::Connected;return kResultOk;}
    tresult PLUGIN_API disconnect(IConnectionPoint*) override {return pass(name=="controller"?"controller-disconnect":"component-disconnect") && !failDisconnect?kResultOk:kResultFalse;}
    tresult PLUGIN_API notify(IMessage*) override {return kResultOk;}
};
struct Host final : IHostApplication {
    uint32 refs=1;
    tresult PLUGIN_API queryInterface(const TUID id,void** p) override {if(!p){return kInvalidArgument;}*p=nullptr;if(!pass("host-query"))return kNoInterface;if(std::memcmp(id,IHostApplication::iid,16))return kNoInterface;*p=this;addRef();return kResultOk;}
    uint32 PLUGIN_API addRef() override {return ++refs;}
    uint32 PLUGIN_API release() override {require(std::this_thread::get_id()==uiThread && !processActive,"release on UI after process");if(faultObserved && lifecycleHook)lifecycleHook("failure-interface-release");trace.push_back("host-release");require(refs>1,"host query reference");return --refs;}
    tresult PLUGIN_API getName(String128 name) override {name[0]='F';name[1]=0;return kResultOk;}
    tresult PLUGIN_API createInstance(TUID,TUID,void** p) override {if(p)*p=nullptr;return kNoInterface;}
};
struct InitFactory final : IPluginFactory3 {
    InitFake& component;InitFake& controller;uint32 refs=0;int classes=1;bool wrongCid=false,separate=false,aliasController=false,factory3Supported=false;FUnknown* hostContext=nullptr;
    explicit InitFactory(InitFake& c,InitFake& d):component(c),controller(d){}
    tresult PLUGIN_API queryInterface(const TUID id,void** p) override {if(p)*p=nullptr;if(!pass("factory3-query"))return kInternalError;if(factory3Supported && p && std::memcmp(id,IPluginFactory3::iid,16)==0){*p=static_cast<IPluginFactory3*>(this);addRef();return kResultOk;}return kNoInterface;}
    uint32 PLUGIN_API addRef() override{return ++refs;}
    uint32 PLUGIN_API release() override {require(std::this_thread::get_id()==uiThread && !processActive,"release on UI after process");if(faultObserved && lifecycleHook)lifecycleHook("failure-interface-release");trace.push_back("factory-release");require(refs>0,"factory reference");--refs;if(refs==0 && hostContext){auto* h=std::exchange(hostContext,nullptr);h->release();}return refs;}
    tresult PLUGIN_API getFactoryInfo(PFactoryInfo* p) override {*p={};return kResultOk;}
    tresult PLUGIN_API getClassInfo2(int32,PClassInfo2*) override{return kNotImplemented;}
    tresult PLUGIN_API getClassInfoUnicode(int32,PClassInfoW*) override{return kNotImplemented;}
    tresult PLUGIN_API setHostContext(FUnknown* context) override {if(!pass("factory3-host"))return kResultFalse;require(context && !hostContext,"one factory context");hostContext=context;hostContext->addRef();return kResultOk;}
    int32 PLUGIN_API countClasses() override {return classes;}
    tresult PLUGIN_API getClassInfo(int32 index,PClassInfo* p) override {if(!pass("class-info"))return kResultFalse;*p={};const bool ctrl=separate && index==1;std::memset(p->cid,ctrl?7:wrongCid?9:5,16);std::strcpy(p->category,ctrl?kVstComponentControllerClass:kVstAudioEffectClass);return kResultOk;}
    tresult PLUGIN_API createInstance(FIDString cid,FIDString iid,void** out) override {
        *out=nullptr;TUID componentCid{},controllerCid{};std::memset(componentCid,5,16);std::memset(controllerCid,7,16);
        if(std::memcmp(cid,controllerCid,16)==0) {
            if(!pass("controller-create")){return kResultFalse;}require(std::memcmp(iid,IEditController::iid,16)==0,"controller IID");
            if(aliasController){component.addRef();*out=static_cast<IEditController*>(&component);}else{require(controller.refs==0,"single controller creation");controller.refs=1;*out=static_cast<IEditController*>(&controller);}return kResultOk;
        }
        if(!pass("component-create")){return kResultFalse;}require(std::memcmp(cid,componentCid,16)==0 && std::memcmp(iid,IComponent::iid,16)==0,"frozen component request");
        require(component.refs==0,"single component creation");component.refs=1;*out=static_cast<IComponent*>(&component);return kResultOk;
    }
};
static InitFactory* factory=nullptr;
static bool entry(void*){return pass("ModuleEntry");}
static IPluginFactory* factoryExport(){if(!pass("GetPluginFactory"))return nullptr;factory->addRef();return factory;}
static bool moduleExit(){return pass("ModuleExit");}
struct FakeLoader final : LoaderOps {
    int opens=0,closes=0;
    void* open(const char* path) override {require(std::string(path)=="/sealed/fake.so","exact path");++opens;return pass("loader-open")?this:nullptr;}
    ModuleBindings resolve(void* token) override {require(token==this,"loader token");return pass("loader-resolve")?ModuleBindings{entry,factoryExport,moduleExit}:ModuleBindings{};}
    int close(void* token) override {require(token==this,"same loader owns close");++closes;return pass("loader-close")?0:-1;}
};
[[maybe_unused]] static int count(const char* name){return static_cast<int>(std::count(trace.begin(),trace.end(),name));}
using namespace std::chrono_literals;
template<class F> bool failed(F&& f){try{f();return false;}catch(...){return true;}}
worker::WaitPolicy waitPolicy(){return {worker::Clock::now()+2s,10000,1ms};}
int main(int argc,char** argv){try{
    require(argc==2,"case");const std::string test=argv[1];trace.reserve(1000);
    InitFake component("component",0),controller("controller",0);InitFactory factoryObject(component,controller);factory=&factoryObject;
    Host host;FakeLoader loader;TUID cid{};std::memset(cid,5,16);InjectedModule module(loader);module.load("/sealed/fake.so",cid,host);Handler handler;
    if(test=="synchronous-parity"){
        failure="setup";InjectedInstance old(module,handler);require(failed([&]{old.start({});}) && component.refs==0 && old.primaryError,"default synchronous auto cleanup unchanged");module.close();return 0;
    }
    if(test=="worker-startup-oom"){
        failNextAllocation=true;require(failed([&]{ProcessingInstanceOwner partial(module,handler);}),"worker startup allocation failure");
        require(component.refs==0 && controller.refs==0 && processCalls==0 && count("component-create")==0,"construction failure cannot own native instance");module.close();return 0;
    }
    bool cleanupObserved=false;
    auto owner=std::make_unique<ProcessingInstanceOwner>(module,handler);
    lifecycleHook=[&](const char* op){
        if(std::string(op)=="stop-processing" || std::string(op)=="component-terminate" || std::string(op)=="failure-interface-release"){
            require(owner && owner->joinedForCleanup(),"native final cleanup after actual worker join");cleanupObserved=true;
        }
    };
    bool rejected=false;
    if(test=="initialize-failure")failure="component-initialize";
    if(test=="prepare-failure")failure="setup";
    if(test=="capture-failure")failure="audio-bus-info";
    try{
        owner->initializeConnected();owner->captureConnected();owner->prepareAndStart({},true);
        RenderProtocol render;render.frames=64;FlushProtocol flush;flush.controls={{0,31,.5}};flush.expectedIds={31};
        if(test=="destructor" || test=="destructor-wrong-thread"){
            if(test=="destructor-wrong-thread"){
                std::set_terminate([]{std::_Exit(77);});std::thread foreign([&]{owner.reset();});foreign.join();require(false,"destructor must terminate wrong thread");
            }
            // Keep a stable nonowning pointer for lifecycle diagnostic during unique_ptr reset.
            auto* raw=owner.get();lifecycleHook=[&](const char* op){if(std::string(op)=="component-terminate"){require(raw->joinedForCleanup(),"destructor joined first");cleanupObserved=true;}};
            owner.reset();require(component.refs==0 && cleanupObserved,"destructor joins then UI cleanup");lifecycleHook={};module.close();return 0;
        }
        if(test=="normal-render" || test=="normal-flush" || test=="render-then-flush"){
            if(test!="normal-flush"){auto result=owner->render(render,waitPolicy(),[]{});require(result.interleaved.size()==128 && owner->taskReceipt()->thread()!=uiThread,"actual render via worker");}
            if(test!="normal-render"){auto receipt=owner->flush(flush,waitPolicy(),[]{});require(receipt.accepted && receipt.processAttempted && owner->taskReceipt()->kind()==worker::Kind::Flush,"actual flush via worker");}
            owner->close();require(owner->acceptedClosure() && owner->joinedForCleanup(),"normal joined closure");
        }else{
            std::atomic<bool> entered=false,allow=false;bool pumpOnce=false;
            if(test=="process-fail")processHook=[](ProcessData&){return kResultFalse;};
            else if(test=="process-throw" || test=="flush-throw" || test=="process-and-cleanup-fail")processHook=[](ProcessData&)->tresult{throw std::runtime_error("native process throw");};
            else if(test=="worker-misuse")processHook=[&](ProcessData&){require(failed([&]{owner->suspend();}),"wrongthread owner rejected");require(component.refs>0 && !cleanupObserved,"no native free on worker misuse");return kResultOk;};
            else if(test=="nested-close" || test=="nested-suspend" || test=="pump-throw" || test=="destructor-in-pump")processHook=[&](ProcessData&){entered=true;while(!allow.load())std::this_thread::yield();return kResultOk;};
            else if(test=="timeout")processHook=[](ProcessData&){std::this_thread::sleep_for(30ms);return kResultOk;};
            else if(test=="hang")processHook=[](ProcessData&)->tresult{for(;;)std::this_thread::yield();};
            if(test=="process-and-cleanup-fail")failure="stop-processing";
            if(test=="wrong-thread-idle"){std::thread foreign([&]{require(failed([&]{owner->suspend();}),"wrongthread lifecycle rejected");});foreign.join();}
            if(test=="setup-mismatch")render.rate=44100;
            if(test=="flush-invalid")flush.expectedIds={32};
            if(test=="submit-oom")failNextAllocation=true;
            auto policy=waitPolicy();if(test=="expired")policy.deadline=worker::Clock::now()-1ms;
            if(test=="timeout" || test=="hang")policy.deadline=worker::Clock::now()+5ms;
            auto pump=[&]{
                if(test=="nested-close" || test=="nested-suspend" || test=="pump-throw" || test=="destructor-in-pump"){
                    if(pumpOnce)return;
                    pumpOnce=true;while(!entered.load())std::this_thread::yield();
                    if(test=="destructor-in-pump"){std::set_terminate([]{std::_Exit(77);});owner.reset();}
                    if(test=="nested-close")require(failed([&]{owner->close();}),"nested close rejected");
                    if(test=="nested-suspend")require(failed([&]{owner->suspend();}),"nested suspend rejected");
                    require(component.refs>0 && !cleanupObserved && processActive,"native remains live after caught nested misuse");allow=true;
                    if(test=="pump-throw")throw std::runtime_error("pump failure");
                }
            };
            if(test=="flush-invalid" || test=="flush-throw")owner->flush(flush,policy,pump);else owner->render(render,policy,pump);
            require(false,"negative case accepted");
        }
    }catch(...){rejected=true;}
    const bool good=test=="normal-render" || test=="normal-flush" || test=="render-then-flush";
    require(rejected!=good,"expected operation result");
    require(component.refs==0 && controller.refs==0 && !processActive,"native refs freed only after processing exit");
    require(owner->joinedForCleanup(),"authoritative cleanup join exists");
    if(!good)require(!owner->acceptedClosure() && owner->primaryError(),"failed work cannot become clean close admission");
    if(test=="process-and-cleanup-fail"){
        const auto receipt=owner->cleanupReceipt();bool stopFailed=false;for(std::size_t i=0;i<receipt.lifecycle.count;++i){const auto& row=receipt.lifecycle.rows[i];if(std::string(row.name)=="stop-processing")stopFailed=!row.ok;}
        require(receipt.error && receipt.error!=owner->primaryError() && stopFailed,"preserve primary and cleanup failure receipts");
    }
    if(test=="wrong-thread-idle")require(processCalls==0 && std::string(owner->failureReason())=="wrong-thread","poison before dispatch prevents native process");
    if(test=="expired")require(processCalls==0 && owner->joinReceipt()->canceled()==1,"expired canceled before native call");
    if(test=="submit-oom")require(processCalls==0 && owner->joinReceipt()->failure()==worker::Failure::Allocation,"actual task submit allocation failure");
    if(test=="setup-mismatch" || test=="flush-invalid")require(processCalls==0,"invalid setup/protocol precedes native call");
    if(test=="timeout" || test=="nested-close" || test=="nested-suspend" || test=="pump-throw")require(processExited && cleanupObserved,"callback exited before UI cleanup");
    lifecycleHook={};owner.reset();module.close();require(loader.closes==1 && host.refs==1 && factoryObject.refs==0,"module remains until owner finished");
    std::cout<<"native_worker_lease_checked=1\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
