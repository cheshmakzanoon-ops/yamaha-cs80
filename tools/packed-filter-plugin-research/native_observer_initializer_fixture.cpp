// Inert initializer SDK fakes. Loader operations never touch an actual module.
#define INIT_CLASS_IID
#include "native_observer_initializer.hpp"
#include <iostream>
using namespace cs80_observer;
static std::vector<std::string> trace;
static std::string failure;
static bool pass(const char* name){trace.push_back(name);return failure!=name;}
struct InitFake final : IComponent, IAudioProcessor, IEditController, IConnectionPoint {
    std::string name;uint32 refs;int terminated=0;bool throwStop=false,failStop=false,failActive=false,failDisconnect=false,failTerminate=false;
    bool combined=true;bool busActive[2]{false,false};
    ObserverPhase phase=ObserverPhase::Created;int classQueries=0,metricsQueries=0;
    bool badStream=false;double normalized=.25,defaultNormalized=.5;int parameterCount=1;
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
    uint32 PLUGIN_API release() override {trace.push_back("interface-release");require(refs>0,"reference underflow");return --refs;}
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
    tresult PLUGIN_API setState(IBStream*) override {return kResultOk;}
    tresult PLUGIN_API getState(IBStream* s) override {if(!pass("component-state"))return kResultFalse;char raw[]{'A',0,'Z'};int32 n=0;s->write(badStream?nullptr:raw,3,&n);return kResultOk;}
    tresult PLUGIN_API setBusArrangements(SpeakerArrangement*,int32 inputs,SpeakerArrangement* output,int32 outputs) override {require(inputs==0 && outputs==1 && output && *output==SpeakerArr::kStereo,"exact stereo arrangement");return pass("bus-arrangements")?kResultOk:kResultFalse;}
    tresult PLUGIN_API getBusArrangement(BusDirection,int32,SpeakerArrangement& a) override {a=3;return pass("bus-readback")?kResultOk:kResultFalse;}
    tresult PLUGIN_API canProcessSampleSize(int32 size) override {require(size==kSample32,"32-bit samples");return pass("sample-size")?kResultOk:kResultFalse;}
    uint32 PLUGIN_API getLatencySamples() override {++metricsQueries;require(phase==ObserverPhase::Setup,"fake metrics before setup");return 17;}
    tresult PLUGIN_API setupProcessing(ProcessSetup& p) override {require(phase==ObserverPhase::Connected && p.sampleRate==48000 && p.maxSamplesPerBlock==64,"frozen setup");if(!pass("setup"))return kResultFalse;phase=ObserverPhase::Setup;return kResultOk;}
    tresult PLUGIN_API setProcessing(TBool processing) override {if(!pass(processing?"start-processing":"stop-processing"))return kResultFalse;require(phase==(processing?ObserverPhase::Activated:ObserverPhase::Processing),"processing phase");phase=processing?ObserverPhase::Processing:ObserverPhase::Activated;return kResultOk;}
    tresult PLUGIN_API process(ProcessData&) override {return kResultOk;}
    uint32 PLUGIN_API getTailSamples() override {++metricsQueries;require(phase==ObserverPhase::Setup,"fake metrics before setup");return 23;}
    tresult PLUGIN_API setComponentState(IBStream* stream) override {int64 pos=-1;require(stream && stream->tell(&pos)==kResultOk && pos==0,"rewound component state");return pass("controller-state")?kResultOk:kResultFalse;}
    int32 PLUGIN_API getParameterCount() override {require(phase==ObserverPhase::Connected,"fake parameter phase");return parameterCount;}
    tresult PLUGIN_API getParameterInfo(int32,ParameterInfo& p) override {p={};p.id=31;p.title[0]='P';p.defaultNormalizedValue=defaultNormalized;return kResultOk;}
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
    uint32 PLUGIN_API release() override {trace.push_back("host-release");require(refs>1,"host query reference");return --refs;}
    tresult PLUGIN_API getName(String128 name) override {name[0]='F';name[1]=0;return kResultOk;}
    tresult PLUGIN_API createInstance(TUID,TUID,void** p) override {if(p)*p=nullptr;return kNoInterface;}
};
struct InitFactory final : IPluginFactory3 {
    InitFake& component;InitFake& controller;uint32 refs=0;int classes=1;bool wrongCid=false,separate=false,aliasController=false,factory3Supported=false;FUnknown* hostContext=nullptr;
    explicit InitFactory(InitFake& c,InitFake& d):component(c),controller(d){}
    tresult PLUGIN_API queryInterface(const TUID id,void** p) override {if(p)*p=nullptr;if(!pass("factory3-query"))return kInternalError;if(factory3Supported && p && std::memcmp(id,IPluginFactory3::iid,16)==0){*p=static_cast<IPluginFactory3*>(this);addRef();return kResultOk;}return kNoInterface;}
    uint32 PLUGIN_API addRef() override{return ++refs;}
    uint32 PLUGIN_API release() override {trace.push_back("factory-release");require(refs>0,"factory reference");--refs;if(refs==0 && hostContext){auto* h=std::exchange(hostContext,nullptr);h->release();}return refs;}
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
static int count(const char* name){return static_cast<int>(std::count(trace.begin(),trace.end(),name));}
int main(int argc,char** argv) {
    try {
        require(argc==2,"case required");std::string test=argv[1];
        const bool moduleFailure=test.rfind("module:",0)==0;
        const bool separate=test.rfind("separate:",0)==0 || test=="separate-success" || test=="separate-missing-class" || test=="separate-alias";
        const bool instanceFailure=test.rfind("instance:",0)==0 || test.rfind("separate:",0)==0;
        const bool cleanupFailure=test.rfind("cleanup:",0)==0;
        if(moduleFailure){failure=test.substr(7);}if(instanceFailure){failure=test.substr(test.rfind("separate:",0)==0?9:9);}if(cleanupFailure){failure=test.substr(8);}
        InitFake component("component",0),controller("controller",0);InitFactory f(component,controller);factory=&f;Host host;FakeLoader loader;TUID cid{};std::memset(cid,5,16);
        if(test=="factory3-success" || test=="module:factory3-host"){f.factory3Supported=true;}
        if(test=="duplicate-cid"){f.classes=2;}if(test=="missing-cid"){f.wrongCid=true;}
        if(separate){component.combined=false;f.separate=true;f.classes=2;}
        if(test=="separate-missing-class"){f.separate=false;f.classes=1;}if(test=="separate-alias"){f.aliasController=true;}
        InjectedModule module(loader);bool loadFailed=false;try{module.load("/sealed/fake.so",cid,host);}catch(...){loadFailed=true;}
        if(moduleFailure || test=="duplicate-cid" || test=="missing-cid") {
            require(loadFailed && !module.loaded && host.refs==1 && f.refs==0,"module failure cleaned owned references");
            require(count("ModuleExit")==((count("ModuleEntry") && failure!="ModuleEntry")?1:0),"entry success governs exit");
            require(loader.closes==(count("loader-open") && failure!="loader-open"?1:0),"owned handle closes once");return 0;
        }
        require(!loadFailed,"module load should pass");Handler handler;InjectedInstance instance(module,handler);bool startFailed=false;
        try{instance.start({});}catch(...){startFailed=true;}
        if(instanceFailure || test=="separate-missing-class" || test=="separate-alias") {
            require(startFailed && !instance.initializationSucceeded && component.refs==0 && controller.refs==0,"failed instance releases all interfaces");
            require(component.terminated==(count("component-initialize") && failure!="component-initialize"?1:0),"failed initialize not terminated");
            require(controller.terminated==(count("controller-initialize") && failure!="controller-initialize"?1:0),"failed controller initialize not terminated");
            require(count("component-disconnect")==((count("component-connect") && failure!="component-connect")?1:0),"only successful component connection disconnected");
            require(count("controller-disconnect")==((count("controller-connect") && failure!="controller-connect")?1:0),"only successful controller connection disconnected");
            require(count("event-bus-deactivate")==((count("event-bus-activate") && failure!="event-bus-activate")?1:0),"only successful event activation deactivated");
            require(count("audio-bus-deactivate")==((count("audio-bus-activate") && failure!="audio-bus-activate")?1:0),"only successful audio activation deactivated");
        }else {
            require(!startFailed && instance.started && instance.initializationSucceeded,"combined initializer started");
            if(test=="live-module-close") {bool denied=false;try{module.close();}catch(...){denied=true;}require(denied && count("ModuleExit")==0 && loader.closes==0,"cannot unload live instance");}
            bool closeFailed=false;try{instance.close();}catch(...){closeFailed=true;}
            require(closeFailed==cleanupFailure,"cleanup status");require(component.refs==0 && controller.refs==0 && component.terminated==1 && controller.terminated==(separate?1:0),"each initialized base released and terminated once");
            require(count("event-bus-deactivate")==1 && count("audio-bus-deactivate")==1,"both successful bus activations cleaned");
            if(test=="no-retry"){const auto n=trace.size();bool denied=false;try{instance.start({});}catch(...){denied=true;}require(denied && trace.size()==n,"no native retry");}
        }
        bool moduleCloseFailed=false;try{module.close();}catch(...){moduleCloseFailed=true;}
        require(moduleCloseFailed==(test=="live-module-close"),"module close receipt");require(host.refs==1 && f.refs==0 && loader.closes==1 && count("ModuleExit")==1,"module ownership balanced");
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
