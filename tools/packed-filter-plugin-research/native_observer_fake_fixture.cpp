// Inert SDK-interface fakes only. No plugin, synth, dlopen, or real dlclose.
#define INIT_CLASS_IID
#include "native_observer_sdk.hpp"
#include <iostream>
#include <thread>
using namespace cs80_observer;
static std::vector<std::string> trace;
static bool exitOk=true;
static int closeResult=0;
static bool fakeModuleExit(){trace.push_back("exit");return exitOk;}
// Interpose the loader close symbol only in this standalone fake test program.
extern "C" int dlclose(void*) noexcept {trace.push_back("dlclose");return closeResult;}

struct Fake final : IComponent, IAudioProcessor, IEditController, IConnectionPoint {
    std::string name;uint32 refs;int terminated=0;bool throwStop=false,failStop=false,failActive=false,failDisconnect=false,failTerminate=false;
    ObserverPhase phase=ObserverPhase::Created;int classQueries=0,metricsQueries=0;
    bool badStream=false;double normalized=.25,defaultNormalized=.5;int parameterCount=1;
    explicit Fake(std::string n,uint32 r):name(std::move(n)),refs(r){}
    tresult PLUGIN_API queryInterface(const TUID,void** p) override {if(p)*p=nullptr;return kNoInterface;}
    uint32 PLUGIN_API addRef() override {return ++refs;}
    uint32 PLUGIN_API release() override {trace.push_back(name+".release");require(refs>0,"reference underflow");return --refs;}
    tresult PLUGIN_API initialize(FUnknown*) override {phase=ObserverPhase::Initialized;return kResultOk;}
    tresult PLUGIN_API terminate() override {trace.push_back(name+".terminate");++terminated;return failTerminate || terminated>1?kResultFalse:kResultOk;}
    tresult PLUGIN_API getControllerClassId(TUID id) override {++classQueries;if(phase!=ObserverPhase::Created)return kResultFalse;std::memset(id,7,sizeof(TUID));return kResultOk;}
    tresult PLUGIN_API setIoMode(IoMode) override {return kResultOk;}
    int32 PLUGIN_API getBusCount(MediaType media,BusDirection direction) override {require(phase==ObserverPhase::Connected,"fake bus phase");return media==kAudio && direction==kOutput?1:0;}
    tresult PLUGIN_API getBusInfo(MediaType m,BusDirection d,int32,BusInfo& b) override {b={};b.mediaType=m;b.direction=d;b.channelCount=2;b.name[0]='O';return kResultOk;}
    tresult PLUGIN_API getRoutingInfo(RoutingInfo&,RoutingInfo&) override {return kNotImplemented;}
    tresult PLUGIN_API activateBus(MediaType,BusDirection,int32,TBool) override {return kResultOk;}
    tresult PLUGIN_API setActive(TBool) override {trace.push_back(name+".active");return failActive?kResultFalse:kResultOk;}
    tresult PLUGIN_API setState(IBStream*) override {return kResultOk;}
    tresult PLUGIN_API getState(IBStream* s) override {char raw[]{'A',0,'Z'};int32 n=0;s->write(badStream?nullptr:raw,3,&n);return kResultOk;}
    tresult PLUGIN_API setBusArrangements(SpeakerArrangement*,int32,SpeakerArrangement*,int32) override {return kResultOk;}
    tresult PLUGIN_API getBusArrangement(BusDirection,int32,SpeakerArrangement& a) override {a=3;return kResultOk;}
    tresult PLUGIN_API canProcessSampleSize(int32) override {return kResultOk;}
    uint32 PLUGIN_API getLatencySamples() override {++metricsQueries;require(phase==ObserverPhase::Setup,"fake metrics before setup");return 17;}
    tresult PLUGIN_API setupProcessing(ProcessSetup&) override {require(phase==ObserverPhase::Connected,"fake setup phase");phase=ObserverPhase::Setup;return kResultOk;}
    tresult PLUGIN_API setProcessing(TBool) override {trace.push_back(name+".stop");if(throwStop)throw std::runtime_error("fake native stop exception");return failStop?kResultFalse:kResultOk;}
    tresult PLUGIN_API process(ProcessData&) override {return kResultOk;}
    uint32 PLUGIN_API getTailSamples() override {++metricsQueries;require(phase==ObserverPhase::Setup,"fake metrics before setup");return 23;}
    tresult PLUGIN_API setComponentState(IBStream*) override {return kResultOk;}
    int32 PLUGIN_API getParameterCount() override {require(phase==ObserverPhase::Connected,"fake parameter phase");return parameterCount;}
    tresult PLUGIN_API getParameterInfo(int32,ParameterInfo& p) override {p={};p.id=31;p.title[0]='P';p.defaultNormalizedValue=defaultNormalized;return kResultOk;}
    tresult PLUGIN_API getParamStringByValue(ParamID,ParamValue,String128) override {return kNotImplemented;}
    tresult PLUGIN_API getParamValueByString(ParamID,TChar*,ParamValue&) override {return kNotImplemented;}
    ParamValue PLUGIN_API normalizedParamToPlain(ParamID,ParamValue v) override {return v;}
    ParamValue PLUGIN_API plainParamToNormalized(ParamID,ParamValue v) override {return v;}
    ParamValue PLUGIN_API getParamNormalized(ParamID) override {return normalized;}
    tresult PLUGIN_API setParamNormalized(ParamID,ParamValue) override {return kResultOk;}
    tresult PLUGIN_API setComponentHandler(IComponentHandler*) override {trace.push_back(name+".handler");return kResultOk;}
    IPlugView* PLUGIN_API createView(FIDString) override {return nullptr;}
    tresult PLUGIN_API connect(IConnectionPoint*) override {phase=ObserverPhase::Connected;return kResultOk;}
    tresult PLUGIN_API disconnect(IConnectionPoint*) override {trace.push_back(name+".disconnect");return failDisconnect?kResultFalse:kResultOk;}
    tresult PLUGIN_API notify(IMessage*) override {return kResultOk;}
};
struct Factory final : IPluginFactory {
    uint32 refs=1;int classes=1;
    tresult PLUGIN_API queryInterface(const TUID,void** p) override {if(p)*p=nullptr;return kNoInterface;}
    uint32 PLUGIN_API addRef() override {return ++refs;}
    uint32 PLUGIN_API release() override {trace.push_back("factory.release");return --refs;}
    tresult PLUGIN_API getFactoryInfo(PFactoryInfo* p) override {*p={};std::strcpy(p->vendor,"fake");return kResultOk;}
    int32 PLUGIN_API countClasses() override {return classes;}
    tresult PLUGIN_API getClassInfo(int32,PClassInfo* p) override {*p={};std::memset(p->cid,5,sizeof(TUID));std::strcpy(p->name,"fake");std::strcpy(p->category,"Audio Module Class");return kResultOk;}
    tresult PLUGIN_API createInstance(FIDString,FIDString,void**) override {return kNotImplemented;}
};
void install(NativeOwnership& o,Fake& core,Fake& ctrl,Factory& factory,bool combined=false) {
    o.component=&core;o.audio=&core;o.controller=&ctrl;o.factory=&factory;
    o.componentConnection=&core;o.controllerConnection=&ctrl;
    o.processing=o.active=o.componentInitialized=o.handlerAttached=true;
    o.controllerInitialized=!combined;
    o.componentConnected=o.controllerConnected=!combined;
    o.moduleExit=fakeModuleExit;o.entered=true;o.handle=reinterpret_cast<void*>(0x1234);
}
int main(int argc,char** argv) {
    try {
        require(argc==2,"case required");std::string name=argv[1];
        if(name=="stream-roundtrip") {Stream s;char data[]{'a',0,'z'};int32 actual=0;require(s.write(data,3,&actual)==kResultOk && actual==3,"write");require(s.seek(0,IBStream::kIBSeekSet,nullptr)==kResultOk,"seek");char out[9]{};require(s.read(out,9,&actual)==kResultOk && actual==3 && std::memcmp(out,data,3)==0,"read binary");int64 pos=-1;require(s.tell(&pos)==kResultOk && pos==3 && !s.failed,"tell");}
        else if(name=="stream-invalid") {Stream s;int32 n=99;require(s.write(nullptr,1,&n)!=kResultOk && n==0 && s.failed,"latched write error");}
        else if(name=="query-interface") {Stream s;void* p=nullptr;require(s.queryInterface(IBStream::iid,&p)==kResultOk && p==&s,"stream IID");TUID invalid{};p=&s;require(s.queryInterface(invalid,&p)==kNoInterface && !p,"unknown IID");Handler h;require(h.queryInterface(IComponentHandler::iid,&p)==kResultOk && p==&h,"handler IID");}
        else if(name=="handler") {Handler h;require(h.beginEdit(3)==kResultOk && h.performEdit(3,.75)==kResultOk && h.endEdit(3)==kResultOk && h.restartComponent(kLatencyChanged)==kResultOk,"events");Bytes b;h.encode(b);require(h.events.size()==4 && h.events[3].id==kLatencyChanged && b.data.size()==8+4*16,"full callback encoding");}
        else if(name=="handler-nonfinite") {Handler h;h.performEdit(1,std::numeric_limits<double>::infinity());Bytes out;bool failed=false;try{h.encode(out);}catch(...){failed=true;}require(failed,"nonfinite callback cannot become accepted evidence");}
        else if(name=="handler-threads") {Handler h;std::thread a([&]{for(int i=0;i<1000;++i)h.restartComponent(kLatencyChanged);});std::thread b([&]{for(int i=0;i<1000;++i)h.beginEdit(1);});a.join();b.join();Bytes out;h.encode(out);require(h.events.size()==2000,"serialized concurrent callbacks");}
        else if(name=="state") {Fake c("core",1),d("controller",1);Bytes b;captureState(c,d,b);require(b.data.size()==8+24+8+3+8+3,"full streams with lengths");}
        else if(name=="state-invalid") {Fake c("core",1),d("controller",1);c.badStream=true;Bytes b;bool failed=false;try{captureState(c,d,b);}catch(...){failed=true;}require(failed,"successful plugin return cannot hide stream failure");}
        else if(name=="metadata" || name=="metadata-range" || name=="metadata-empty" || name=="metadata-no-class" || name=="metadata-default-range") {
            Fake c("core",1),d("controller",1);Factory f;std::string expected;
            if(name=="metadata-range") {d.normalized=1.1;expected="normalized parameter range";}
            if(name=="metadata-empty") {d.parameterCount=0;expected="parameter count";}
            if(name=="metadata-no-class") {f.classes=0;expected="factory class count";require(d.parameterCount==1,"no-class must not mutate parameters");}
            if(name=="metadata-default-range") {d.defaultNormalized=-.1;expected="normalized parameter range";require(d.parameterCount==1 && d.normalized==.25 && f.classes==1,"default-only mutation");}
            Bytes b;std::string error;try{captureFactoryMetadata(f,b);TUID cid{};std::memset(cid,5,16);auto identity=CreatedIdentity::capture(c,cid,ControllerKind::Separate,ObserverPhase::Created);
                c.initialize(nullptr);d.initialize(nullptr);c.connect(&d);d.connect(&c);
                captureConnectedMetadata(c,c,d,identity,ObserverPhase::Connected,b);
                ProcessSetup setup{};c.setupProcessing(setup);captureSetupMetrics(c,ObserverPhase::Setup,b);
            }catch(const std::exception& e){error=e.what();}
            require(error==expected,"exact metadata rejection stage");
            if(expected.empty())require(b.data.size()>80,"metadata captured");
        }
        else if(name=="phase-correct-sequence") {
            Fake c("core",1),d("controller",1);Factory f;Bytes b;TUID cid{};std::memset(cid,5,16);
            captureFactoryMetadata(f,b);auto identity=CreatedIdentity::capture(c,cid,ControllerKind::Separate,ObserverPhase::Created);
            require(c.classQueries==1 && c.metricsQueries==0,"Created identity only");
            c.initialize(nullptr);d.initialize(nullptr);c.connect(&d);d.connect(&c);
            captureConnectedMetadata(c,c,d,identity,ObserverPhase::Connected,b);
            require(c.classQueries==1 && c.metricsQueries==0,"Connected metadata does not query late identity/early metrics");
            ProcessSetup setup{};c.setupProcessing(setup);captureSetupMetrics(c,ObserverPhase::Setup,b);
            require(c.metricsQueries==2,"Setup getter pair");
        }
        else if(name=="phase-late-identity" || name=="phase-late-identity-declared") {
            Fake c("core",1);TUID cid{};std::memset(cid,5,16);c.phase=ObserverPhase::Connected;bool failed=false;
            try{auto id=CreatedIdentity::capture(c,cid,ControllerKind::Separate,name=="phase-late-identity"?ObserverPhase::Created:ObserverPhase::Connected);(void)id;}catch(...){failed=true;}
            require(failed && c.classQueries==(name=="phase-late-identity"?1:0),"late identity rejected at native/helper gate");
        }
        else if(name=="phase-early-metrics" || name=="phase-early-metrics-declared") {
            Fake c("core",1);Bytes b;bool failed=false;
            try{captureSetupMetrics(c,name=="phase-early-metrics"?ObserverPhase::Setup:ObserverPhase::Connected,b);}catch(...){failed=true;}
            require(failed && c.metricsQueries==(name=="phase-early-metrics"?1:0),"early metrics rejected at native/helper gate");
        }
        else if(name=="phase-combined-cache") {
            Fake c("combined",1);TUID cid{};std::memset(cid,5,16);auto identity=CreatedIdentity::capture(c,cid,ControllerKind::Combined,ObserverPhase::Created);
            require(c.classQueries==0,"combined cache has no separate controller CID");c.initialize(nullptr);c.phase=ObserverPhase::Connected;Bytes b;
            captureConnectedMetadata(c,c,c,identity,ObserverPhase::Connected,b);require(c.classQueries==0 && c.metricsQueries==0,"combined metadata phase");
        }
        else if(name=="identity-other-component") {
            Fake c("core",1),other("other",1),d("controller",1);TUID cid{};std::memset(cid,5,16);auto identity=CreatedIdentity::capture(c,cid,ControllerKind::Separate,ObserverPhase::Created);
            other.phase=d.phase=ObserverPhase::Connected;Bytes b;bool failed=false;try{captureConnectedMetadata(other,other,d,identity,ObserverPhase::Connected,b);}catch(...){failed=true;}require(failed,"cache cannot bind another live component");
        }
        else if(name=="partial") {Fake c("core",1);Factory f;NativeOwnership o;o.component=&c;o.componentInitialized=true;o.factory=&f;o.entered=true;o.moduleExit=fakeModuleExit;o.handle=reinterpret_cast<void*>(0x1234);o.close();require(o.accepted && c.refs==0 && f.refs==0 && trace==std::vector<std::string>({"core.terminate","core.release","factory.release","exit","dlclose"}),"partial initialization close");}
        else if(name=="partial-uninitialized") {Fake c("core",1);NativeOwnership o;o.component=&c;o.close();require(o.accepted && c.refs==0 && c.terminated==0 && trace==std::vector<std::string>({"core.release"}),"uninitialized component release without terminate");}
        else if(name=="combined") {Fake combined("combined",5);Factory f;NativeOwnership o;install(o,combined,combined,f,true);o.close();require(o.accepted && combined.refs==0 && combined.terminated==1,"combined interface ownership");}
        else if(name=="combined-double-init") {Fake combined("combined",5);Factory f;NativeOwnership o;install(o,combined,combined,f,true);o.controllerInitialized=true;bool failed=false;try{o.close();}catch(...){failed=true;}require(failed && !o.accepted && combined.terminated==2 && combined.refs==0,"bad combined ownership rejected");}
        else {Fake c("core",3),d("controller",2);Factory f;NativeOwnership o;install(o,c,d,f);
            if(name=="stop-exception")c.throwStop=true;else if(name=="stop-failure")c.failStop=true;else if(name=="deactivate-failure")c.failActive=true;else if(name=="disconnect-failure")d.failDisconnect=true;else if(name=="terminate-failure")d.failTerminate=true;else if(name=="exit-failure")exitOk=false;else if(name=="dlclose-failure")closeResult=-1;else require(name=="full-close" || name=="retry","unknown case");
            bool failed=false;try{o.close();}catch(...){failed=true;}
            require(failed==(name!="full-close" && name!="retry"),"close result");require(o.accepted==!failed && o.resultCount==15 && c.refs==0 && d.refs==0 && f.refs==0 && trace.back()=="dlclose","all ownership cleanup despite failure");
            if(name=="stop-exception")require(!o.results[0].returnedNormally && std::string(o.results[0].exception.data())=="fake native stop exception","exception receipt");
            if(name=="full-close")require(o.results[7].returned==2 && o.results[8].returned==1 && o.results[11].returned==0,"raw positive release counts accepted");
            if(name=="full-close")require(trace==std::vector<std::string>({"core.stop","core.active","core.disconnect","controller.disconnect","controller.handler","controller.terminate","core.terminate","core.release","controller.release","core.release","controller.release","core.release","factory.release","exit","dlclose"}),"exact close order");
            if(name=="retry") {auto count=trace.size();bool rejected=false;try{o.close();}catch(...){rejected=true;}require(rejected && trace.size()==count,"close cannot retry");}
        }
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
