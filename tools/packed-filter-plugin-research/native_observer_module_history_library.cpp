// Inert initializer SDK fakes. Loader operations never touch an actual module.
#define INIT_CLASS_IID
#include "native_observer_history.hpp"
#include "pluginterfaces/gui/iplugview.h"
#include <cstdio>
#include <dlfcn.h>
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
static std::atomic<bool> failNextAllocation=false;static bool allocationOnFinalRelease=false;
void* operator new(std::size_t n){if(failNextAllocation.exchange(false))throw std::bad_alloc{};if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc{};}
void operator delete(void* p) noexcept{std::free(p);}void operator delete(void* p,std::size_t) noexcept{std::free(p);}
void* operator new[](std::size_t n){return ::operator new(n);}void operator delete[](void* p) noexcept{::operator delete(p);}void operator delete[](void* p,std::size_t) noexcept{::operator delete(p);}
static std::vector<std::string> trace;
static std::string failure;
static bool faultObserved=false;
static bool throwFailure=false,restoreTracing=false,badAllocationFailure=false,releaseAllocationFailure=false;
static bool pass(const char* name){require(std::this_thread::get_id()==uiThread && !processActive,"native lifecycle UI and outside process");if(lifecycleHook)lifecycleHook(name);trace.push_back(name);if(failure==name)faultObserved=true;if(failure==name && badAllocationFailure)throw std::bad_alloc();if(failure==name && throwFailure)throw std::runtime_error("injected native exception");return failure!=name;}
static std::vector<ParamID> fullIds;
static Steinberg::Linux::IRunLoop* pluginLoop=nullptr;static FUnknown* lastContext=nullptr;
static std::atomic<bool> backgroundReady=false;
static void background(){auto* thread=new std::thread([]{backgroundReady=true;for(;;)std::this_thread::sleep_for(std::chrono::milliseconds(10));});(void)thread;while(!backgroundReady)std::this_thread::yield();}
static void misuse(){auto* p=::dlsym(RTLD_DEFAULT,"cs80_test_misuse");require(p,"inert misuse callback");void(*callback)()=nullptr;std::memcpy(&callback,&p,sizeof(p));callback();}
static const char* mode(){const char* p=std::getenv("CS80_JOIN_MODE");return p?p:"valid";}
static void log(const char* text){if(const char* p=std::getenv("CS80_JOIN_LOG")){if(auto* f=std::fopen(p,"a")){std::fprintf(f,"%s\n",text);std::fclose(f);}}}
struct Timer final:Steinberg::Linux::ITimerHandler {
    uint32 refs=1;
    tresult PLUGIN_API queryInterface(const TUID,void** out)override{if(out)*out=nullptr;return kNoInterface;}
    uint32 PLUGIN_API addRef()override{return ++refs;}
    uint32 PLUGIN_API release()override{require(refs>1,"timer fixture base ref");return --refs;}
    void PLUGIN_API onTimer()override{require(std::this_thread::get_id()==uiThread,"timer UI");log("timer");}
};static Timer timer;

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
    uint32 PLUGIN_API release() override {require(std::this_thread::get_id()==uiThread && !processActive,"release on UI after process");if(faultObserved && lifecycleHook)lifecycleHook("failure-interface-release");trace.push_back("interface-release");require(refs>0,"reference underflow");--refs;if(refs==0 && name=="component" && allocationOnFinalRelease)failNextAllocation=true;if(releaseAllocationFailure)throw std::bad_alloc();return refs;}
    tresult PLUGIN_API initialize(FUnknown* host) override {require(host,"mandatory host");if(!pass(name=="controller"?"controller-initialize":"component-initialize"))return kResultFalse;phase=ObserverPhase::Initialized;
        if(name=="component"){lastContext=host;require(host->queryInterface(Steinberg::Linux::IRunLoop::iid,reinterpret_cast<void**>(&pluginLoop))==kResultOk && pluginLoop,"advertised owned runloop");
            FUnknown* identity=nullptr;require(pluginLoop->queryInterface(FUnknown::iid,reinterpret_cast<void**>(&identity))==kResultOk && identity==host,"canonical FUnknown identity across host/loop");identity->release();
            require(pluginLoop->registerTimer(&timer,1)==kResultOk,"register timer");}
        return kResultOk;}
    tresult PLUGIN_API terminate() override {const bool result=pass(name=="controller"?"controller-terminate":"component-terminate");
        if(name=="component" && pluginLoop){if(std::string(mode())!="retained-timer")require(pluginLoop->unregisterTimer(&timer)==kResultOk,"timer unregister");if(std::string(mode())!="retained-host")pluginLoop->release();pluginLoop=nullptr;log("terminate");}++terminated;return !result || failTerminate || terminated>1?kResultFalse:kResultOk;}
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
    tresult PLUGIN_API getParameterInfo(int32 index,ParameterInfo& p) override {++parameterInfoQueries;p={};p.id=fullIds.at(static_cast<std::size_t>(index));p.title[0]='P';p.defaultNormalizedValue=defaultNormalized;return kResultOk;}
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
    uint32 PLUGIN_API release() override {require(std::this_thread::get_id()==uiThread && !processActive,"release on UI after process");if(faultObserved && lifecycleHook)lifecycleHook("failure-interface-release");trace.push_back("factory-release");require(refs>0,"factory reference");--refs;if(refs==0 && hostContext){auto* h=std::exchange(hostContext,nullptr);h->release();}if(refs==0 && std::string(mode())=="factory-release-throw")throw std::runtime_error("inert factory release exception");return refs;}
    tresult PLUGIN_API getFactoryInfo(PFactoryInfo* p) override {*p={};return kResultOk;}
    tresult PLUGIN_API getClassInfo2(int32,PClassInfo2*) override{return kNotImplemented;}
    tresult PLUGIN_API getClassInfoUnicode(int32,PClassInfoW*) override{return kNotImplemented;}
    tresult PLUGIN_API setHostContext(FUnknown* context) override {if(!pass("factory3-host"))return kResultFalse;require(context && !hostContext,"one factory context");hostContext=context;hostContext->addRef();return kResultOk;}
    int32 PLUGIN_API countClasses() override {return classes;}
    tresult PLUGIN_API getClassInfo(int32 index,PClassInfo* p) override {if(!pass("class-info"))return kResultFalse;*p={};const bool ctrl=separate && index==1;std::memset(p->cid,ctrl?7:wrongCid?9:5,16);std::strcpy(p->category,ctrl?kVstComponentControllerClass:kVstAudioEffectClass);return kResultOk;}
    tresult PLUGIN_API createInstance(FIDString cid,FIDString iid,void** out) override {
        *out=nullptr;if(std::string(mode())=="factory-create-allocation")throw std::bad_alloc();TUID componentCid{},controllerCid{};std::memset(componentCid,5,16);std::memset(controllerCid,7,16);
        if(std::memcmp(cid,controllerCid,16)==0) {
            if(!pass("controller-create")){return kResultFalse;}require(std::memcmp(iid,IEditController::iid,16)==0,"controller IID");
            if(aliasController){component.addRef();*out=static_cast<IEditController*>(&component);}else{require(controller.refs==0,"single controller creation");controller.refs=1;*out=static_cast<IEditController*>(&controller);}return kResultOk;
        }
        if(!pass("component-create")){return kResultFalse;}require(std::memcmp(cid,componentCid,16)==0 && std::memcmp(iid,IComponent::iid,16)==0,"frozen component request");
        require(component.refs==0,"single component creation");component.refs=1;*out=static_cast<IComponent*>(&component);return kResultOk;
    }
};
static InitFactory* factory=nullptr;
[[maybe_unused]] static bool entry(void*){return pass("ModuleEntry");}
[[maybe_unused]] static IPluginFactory* factoryExport(){if(!pass("GetPluginFactory"))return nullptr;factory->addRef();return factory;}
[[maybe_unused]] static bool moduleExit(){return pass("ModuleExit");}
struct FakeLoader final : LoaderOps {
    int opens=0,closes=0;
    void* open(const char* path) override {require(std::string(path)=="/sealed/fake.so","exact path");++opens;return pass("loader-open")?this:nullptr;}
    ModuleBindings resolve(void* token) override {require(token==this,"loader token");return pass("loader-resolve")?ModuleBindings{entry,factoryExport,moduleExit}:ModuleBindings{};}
    int close(void* token) override {require(token==this,"same loader owns close");++closes;return pass("loader-close")?0:-1;}
};
[[maybe_unused]] static int count(const char* name){return static_cast<int>(std::count(trace.begin(),trace.end(),name));}
using namespace std::chrono_literals;
template<class F> bool failed(F&& f){try{f();return false;}catch(...){return true;}}

static InitFake realComponent("component",0),realController("controller",0);
static InitFactory realFactory(realComponent,realController);
__attribute__((constructor)) static void loaded(){log("loaded");if(std::string(mode()).starts_with("constructor-thread"))background();}
__attribute__((destructor)) static void unloaded(){if(std::string(mode())=="destructor-overrelease" && lastContext)lastContext->release();log("unloaded");}
extern "C" bool ModuleEntry(void*) {
    log("entry");if((std::string(mode())=="caught-nested" || std::string(mode())=="destroy-active"))misuse();if(std::string(mode())=="setup-false")failure="setup";if(std::string(mode())=="component-init-false")failure="component-initialize";factory=&realFactory;realFactory.factory3Supported=true;
    const packed_filter_workload::Case c{packed_filter_workload::Policy::legacy,48000,packed_filter_workload::Mode::realtime,8,64};
    for(const auto& q:packed_filter_workload::controls(c))fullIds.push_back(q.id);
    realComponent.parameterCount=static_cast<int>(fullIds.size());
    processHook=[](ProcessData& data){log(data.numSamples?"render":"flush");if(std::string(mode())=="caught-wrong-thread"){misuse();return kResultOk;}require(std::this_thread::get_id()!=uiThread,"process worker");if(data.numSamples)for(int i=0;i<data.numSamples;++i){data.outputs[0].channelBuffers32[0][i]=.25f;data.outputs[0].channelBuffers32[1][i]=-.25f;}return kResultOk;};
    return true;
}
extern "C" IPluginFactory* GetPluginFactory(){if(std::string(mode())=="factory-null")return nullptr;realFactory.addRef();return &realFactory;}
extern "C" bool ModuleExit(){
    log("exit");if(std::string(mode())=="unregistered-thread")background();if(std::string(mode())=="exit-throw")throw std::runtime_error("inert exit exception");if(std::string(mode())=="exit-false")return false;
    IHostApplication* application=nullptr;require(lastContext && lastContext->queryInterface(IHostApplication::iid,reinterpret_cast<void**>(&application))==kResultOk,"host lives through exit");String128 name{};require(application->getName(name)==kResultOk,"exit host callback");
    if(std::string(mode())!="retained-timer"){Steinberg::Linux::IRunLoop* exitLoop=nullptr;require(application->queryInterface(Steinberg::Linux::IRunLoop::iid,reinterpret_cast<void**>(&exitLoop))==kResultOk,"exit runloop remains advertised");require(exitLoop->registerTimer(&timer,1)==kResultOk,"exit registration");if(std::string(mode())!="exit-registers")require(exitLoop->unregisterTimer(&timer)==kResultOk,"exit deregistration");exitLoop->release();}
    TUID messageId{};std::memcpy(messageId,IMessage::iid,16);IMessage* message=nullptr;require(application->createInstance(messageId,messageId,reinterpret_cast<void**>(&message))==kResultOk,"exit service");if(std::string(mode())!="retained-service")message->release();application->release();return true;
}
