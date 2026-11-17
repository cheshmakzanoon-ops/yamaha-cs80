#pragma once
// Compile-only SDK adapter. Lifecycle orchestration/case execution NOT supplied.
#include "native_observer_support.hpp"
#include <pluginterfaces/base/ipluginbase.h>
#include <pluginterfaces/base/ibstream.h>
#include <pluginterfaces/vst/ivstaudioprocessor.h>
#include <pluginterfaces/vst/ivstcomponent.h>
#include <pluginterfaces/vst/ivsteditcontroller.h>
#include <pluginterfaces/vst/ivstmessage.h>
#include <dlfcn.h>
#include <mutex>
#include <atomic>
#include <array>

namespace cs80_observer {
using namespace Steinberg;
using namespace Steinberg::Vst;

class Stream final : public IBStream {
public:
    StateBuffer state;
    bool failed=false;
    tresult PLUGIN_API queryInterface(const TUID id,void** out) override {
        if(!out) { return kInvalidArgument; }
        *out=nullptr;
        if(std::memcmp(id,IBStream::iid,sizeof(TUID)) && std::memcmp(id,FUnknown::iid,sizeof(TUID)))return kNoInterface;
        *out=this;addRef();return kResultOk;
    }
    // Caller-owned stream valid synchronously through getState/setState only.
    uint32 PLUGIN_API addRef() override {return 1;}
    uint32 PLUGIN_API release() override {return 1;}
    tresult PLUGIN_API read(void* p,int32 n,int32* actual) override {
        if(actual) { *actual=0; }
        if(n<0){failed=true;return kInvalidArgument;}
        try {auto count=state.read(p,static_cast<std::size_t>(n));if(actual)*actual=static_cast<int32>(count);return kResultOk;}catch(...){failed=true;return kInvalidArgument;}
    }
    tresult PLUGIN_API write(void* p,int32 n,int32* actual) override {
        if(actual) { *actual=0; }
        if(n<0){failed=true;return kInvalidArgument;}
        try {state.write(p,static_cast<std::size_t>(n));if(actual)*actual=n;return kResultOk;}catch(...){failed=true;return kInvalidArgument;}
    }
    tresult PLUGIN_API seek(int64 n,int32 mode,int64* actual) override {
        try {state.seek(n,mode);if(actual)*actual=static_cast<int64>(state.position);return kResultOk;}catch(...){failed=true;return kInvalidArgument;}
    }
    tresult PLUGIN_API tell(int64* actual) override {
        if(!actual) { failed=true;return kInvalidArgument; }
        *actual=static_cast<int64>(state.position);return kResultOk;
    }
};

struct HandlerEvent { std::uint32_t kind,id; double value; };
class Handler final : public IComponentHandler {
public:
    // Concurrent callback arrival is serialized; order is observed, not invented.
    mutable std::mutex mutex;
    std::vector<HandlerEvent> events;
    std::atomic<bool> failed{false};
    tresult append(HandlerEvent e) noexcept {
        try {std::lock_guard lock(mutex);if(events.size()>=1000000){failed=true;return kInternalError;}events.push_back(e);return kResultOk;}
        catch(...){failed=true;return kInternalError;}
    }
    tresult PLUGIN_API queryInterface(const TUID id,void** out) override {
        if(!out) { return kInvalidArgument; }
        *out=nullptr;
        if(std::memcmp(id,IComponentHandler::iid,sizeof(TUID)) && std::memcmp(id,FUnknown::iid,sizeof(TUID)))return kNoInterface;
        *out=this;addRef();return kResultOk;
    }
    uint32 PLUGIN_API addRef() override {return 1;}
    uint32 PLUGIN_API release() override {return 1;}
    tresult PLUGIN_API beginEdit(ParamID id) override {return append({0,id,0});}
    tresult PLUGIN_API performEdit(ParamID id,ParamValue v) override {return append({1,id,v});}
    tresult PLUGIN_API endEdit(ParamID id) override {return append({2,id,0});}
    tresult PLUGIN_API restartComponent(int32 flags) override {return append({3,static_cast<std::uint32_t>(flags),0});}
    void encode(Bytes& out) const {std::lock_guard lock(mutex);require(!failed,"handler callback failure");out.u64(events.size());for(const auto& e:events){out.u32(e.kind);out.u32(e.id);out.f64(e.value);}}
};

enum class ObserverPhase { Created, Initialized, Connected, Setup, Activated, Processing };
enum class ControllerKind { Combined, Separate };
// This cache is tied to the live component, not a substitute for the initializer's
// trusted class selection/direct-query and successful phase-transition receipts.
class CreatedIdentity {
    const IComponent* owner;
    std::array<std::uint8_t,16> componentCid{},controllerCid{};
    ControllerKind kind;
    CreatedIdentity(const IComponent& component,ControllerKind selectedKind):owner(&component),kind(selectedKind){}
public:
    static CreatedIdentity capture(IComponent& component,const TUID selectedCid,
                                    ControllerKind kind,ObserverPhase phase) {
        require(phase==ObserverPhase::Created,"controller identity requires Created phase");
        require(kind==ControllerKind::Combined || kind==ControllerKind::Separate,"controller kind required");
        require(selectedCid!=nullptr,"selected component CID required");
        CreatedIdentity result(component,kind);
        std::memcpy(result.componentCid.data(),selectedCid,16);
        require(std::any_of(result.componentCid.begin(),result.componentCid.end(),[](auto b){return b!=0;}),"nonzero component CID required");
        if(kind==ControllerKind::Separate) {
            TUID cid{};require(component.getControllerClassId(cid)==kResultOk,"Created controller class ID");
            std::memcpy(result.controllerCid.data(),cid,16);
            require(std::any_of(result.controllerCid.begin(),result.controllerCid.end(),[](auto b){return b!=0;}),"nonzero controller CID required");
        }
        return result;
    }
    const std::array<std::uint8_t,16>& separateControllerCid() const {
        require(kind==ControllerKind::Separate,"combined controller has no separate CID");
        return controllerCid;
    }
    void encodeFor(const IComponent& component,Bytes& out) const {
        require(owner==&component,"Created identity belongs to another component");
        out.text("native-observer-created-identity-v1");out.raw(componentCid.data(),componentCid.size());
        out.u32(kind==ControllerKind::Separate?1:0);
        // Combined means no separate CID, not sixteen invented zero bytes.
        if(kind==ControllerKind::Separate)out.raw(controllerCid.data(),controllerCid.size());
    }
};
inline void captureFactoryMetadata(IPluginFactory& factory,Bytes& out) {
    out.text("native-observer-factory-v1");
    PFactoryInfo fi{};require(factory.getFactoryInfo(&fi)==kResultOk,"getFactoryInfo");
    out.terminated(fi.vendor);out.terminated(fi.url);out.terminated(fi.email);out.u32(fi.flags);
    const auto classes=factory.countClasses();require(classes>0 && classes<=1024,"factory class count");out.u32(classes);
    for(int32 i=0;i<classes;++i) {PClassInfo c{};require(factory.getClassInfo(i,&c)==kResultOk,"getClassInfo");out.raw(c.cid,sizeof(TUID));out.u32(c.cardinality);out.terminated(c.category);out.terminated(c.name);}
}
struct ConnectedParameterRoster {int32 declaredCount=0;std::vector<ParamID> ids;};
inline void captureConnectedMetadata(IComponent& component,IAudioProcessor& audio,IEditController& controller,
                                      const CreatedIdentity& identity,ObserverPhase phase,Bytes& out,ConnectedParameterRoster* roster=nullptr) {
    require(phase==ObserverPhase::Connected,"metadata requires Connected checkpoint");
    out.text("native-observer-connected-v1");identity.encodeFor(component,out);
    for(auto media:{kAudio,kEvent})for(auto direction:{kInput,kOutput}) {
        const auto count=component.getBusCount(media,direction);require(count>=0 && count<=1024,"bus count");out.u32(media);out.u32(direction);out.u32(count);
        for(int32 i=0;i<count;++i) {BusInfo b{};require(component.getBusInfo(media,direction,i,b)==kResultOk,"bus metadata");
            out.u32(b.mediaType);out.u32(b.direction);out.u32(b.channelCount);out.terminated(b.name);out.u32(b.busType);out.u32(b.flags);
            if(media==kAudio) {SpeakerArrangement arrangement{};require(audio.getBusArrangement(direction,i,arrangement)==kResultOk,"bus arrangement");out.u64(arrangement);}
        }
    }
    const auto n=controller.getParameterCount();require(n>0 && n<=1000000,"parameter count");out.u32(n);
    if(roster){require(roster->ids.empty() && roster->declaredCount==0,"fresh metadata roster required");roster->declaredCount=n;roster->ids.reserve(static_cast<std::size_t>(n));}
    for(int32 i=0;i<n;++i) {ParameterInfo p{};require(controller.getParameterInfo(i,p)==kResultOk,"parameter metadata");
        if(roster){require(p.id!=kNoParamId && std::find(roster->ids.begin(),roster->ids.end(),p.id)==roster->ids.end(),"unique non-sentinel metadata parameter ID");roster->ids.push_back(p.id);}
        const auto value=controller.getParamNormalized(p.id);
        require(p.defaultNormalizedValue>=0.0 && p.defaultNormalizedValue<=1.0 && value>=0.0 && value<=1.0,"normalized parameter range");
        out.u32(p.id);out.terminated(p.title);out.terminated(p.shortTitle);out.terminated(p.units);out.u32(p.stepCount);out.f64(p.defaultNormalizedValue);out.u32(p.unitId);out.u32(p.flags);out.f64(value);}
}
inline void captureSetupMetrics(IAudioProcessor& audio,ObserverPhase phase,Bytes& out) {
    require(phase==ObserverPhase::Setup,"latency/tail require Setup checkpoint");
    out.text("native-observer-setup-metrics-v1");out.u32(audio.getLatencySamples());out.u32(audio.getTailSamples());
}
inline void captureState(IComponent& component,IEditController& controller,Bytes& out) {
    Stream a,b;require(component.getState(&a)==kResultOk && !a.failed,"component full state");require(controller.getState(&b)==kResultOk && !b.failed,"controller full state");
    out.text("native-observer-state-v1");out.blob(a.state.bytes.data(),a.state.bytes.size());out.blob(b.state.bytes.data(),b.state.bytes.size());
}

// Ownership is established by a separately reviewed initializer. Set each flag
// only after successful API return. Fields must not be shared or externally freed.
struct NativeCloseResult {
    const char* operation=nullptr;
    std::int64_t returned=0;
    bool returnedNormally=false,ok=false;
    std::array<char,256> exception{};
};
struct NativeOwnership {
    void* handle=nullptr;bool (*moduleExit)()=nullptr;bool entered=false;
    IPluginFactory* factory=nullptr;IComponent* component=nullptr;IAudioProcessor* audio=nullptr;IEditController* controller=nullptr;
    IConnectionPoint* componentConnection=nullptr;IConnectionPoint* controllerConnection=nullptr;
    bool processing=false,active=false,componentInitialized=false,controllerInitialized=false;
    bool componentConnected=false,controllerConnected=false,handlerAttached=false;
    // Mandatory adjoining gate: activated buses need their own checked
    // deactivation receipts after component deactivation. Not implemented here.
    static constexpr bool busDeactivationGateRequired=true;
    bool attempted=false,accepted=false;
    // Fifteen possible operations, no vector/callback allocation during teardown.
    std::array<NativeCloseResult,15> results{};
    std::size_t resultCount=0;
    NativeOwnership()=default;NativeOwnership(const NativeOwnership&)=delete;NativeOwnership& operator=(const NativeOwnership&)=delete;
    void close() {
        require(!attempted,"native close retry forbidden");attempted=true;bool allOk=true;
        const auto run=[&](const char* name,std::int64_t expected,bool isRelease,auto&& action) noexcept {
            // Exact fixed schedule below has at most fifteen slots.
            auto& r=results[resultCount++];r.operation=name;
            try {r.returned=action();r.returnedNormally=true;r.ok=isRelease || r.returned==expected;}
            catch(const std::exception& e) {const char* p=e.what();if(p) {std::strncpy(r.exception.data(),p,r.exception.size()-1);}}
            catch(...) {std::strncpy(r.exception.data(),"unknown native exception",r.exception.size()-1);}
            allOk=allOk && r.ok;
        };
        if(processing)run("stop",kResultOk,false,[&]() -> std::int64_t {processing=false;return audio?audio->setProcessing(false):kInvalidArgument;});
        if(active)run("deactivate",kResultOk,false,[&]() -> std::int64_t {active=false;return component?component->setActive(false):kInvalidArgument;});
        if(componentConnected)run("component-disconnect",kResultOk,false,[&]() -> std::int64_t {componentConnected=false;return componentConnection && controllerConnection?componentConnection->disconnect(controllerConnection):kInvalidArgument;});
        if(controllerConnected)run("controller-disconnect",kResultOk,false,[&]() -> std::int64_t {controllerConnected=false;return controllerConnection && componentConnection?controllerConnection->disconnect(componentConnection):kInvalidArgument;});
        if(handlerAttached)run("detach-handler",kResultOk,false,[&]() -> std::int64_t {handlerAttached=false;return controller?controller->setComponentHandler(nullptr):kInvalidArgument;});
        if(controllerInitialized)run("controller-terminate",kResultOk,false,[&]() -> std::int64_t {controllerInitialized=false;return controller?controller->terminate():kInvalidArgument;});
        if(componentInitialized)run("component-terminate",kResultOk,false,[&]() -> std::int64_t {componentInitialized=false;return component?component->terminate():kInvalidArgument;});
        const auto release=[&](const char* name,auto*& pointer) {if(pointer)run(name,0,true,[&]() -> std::int64_t {auto* p=std::exchange(pointer,nullptr);return p->release();});};
        release("audio-release",audio);release("controller-connection-release",controllerConnection);release("component-connection-release",componentConnection);
        release("controller-release",controller);release("component-release",component);release("factory-release",factory);
        if(entered)run("ModuleExit",1,false,[&]() -> std::int64_t {entered=false;return moduleExit && moduleExit();});
        if(handle)run("dlclose",0,false,[&]() -> std::int64_t {auto* h=std::exchange(handle,nullptr);return ::dlclose(h);});
        // Evidence lives in fixed slots before acceptance. No post-acceptance copy.
        accepted=allOk;require(allOk,"checked native close failed");
    }
    ~NativeOwnership(){if(!attempted){try{close();}catch(...){}}}
};
} // namespace cs80_observer
