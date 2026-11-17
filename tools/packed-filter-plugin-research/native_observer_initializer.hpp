#pragma once
// Bounded injected initialization only. NO dlopen/dlsym implementation or CLI.
#include "native_observer_sdk.hpp"
#include "native_observer_state.hpp"
#include <pluginterfaces/vst/ivsthostapplication.h>
#include <optional>
#include <filesystem>

namespace cs80_observer {
// Strict SDK behavior remains default. The opt-in applies ONLY to the absent
// combined IEditController query in the pinned JUCE implementation.
enum class ControllerQueryPolicy : std::uint32_t { StrictSdk=1, PinnedJuceFalseNull=2 };
struct ControllerQueryObservation {ControllerQueryPolicy policy=ControllerQueryPolicy::StrictSdk;bool attempted=false,normal=false,pointerPresent=false;std::int32_t result=kInternalError;};
inline void validateControllerQueryPolicy(ControllerQueryPolicy p){require(p==ControllerQueryPolicy::StrictSdk || p==ControllerQueryPolicy::PinnedJuceFalseNull,"unknown controller query policy");}
struct InitializerCall { const char* name=nullptr;std::int64_t returned=0;bool normal=false,ok=false;std::exception_ptr error; };
template<std::size_t N> struct InitializerCalls {
    std::array<InitializerCall,N> rows{};std::size_t count=0;
    template<class F> bool run(const char* name,std::int64_t expected,F&& f,bool anyReturn=false) noexcept {
        // The bounded call schedules and factory cap below fit their fixed arrays.
        if(count==N)return false;
        auto& r=rows[count++];r.name=name;
        try {r.returned=f();r.normal=true;r.ok=anyReturn || r.returned==expected;}
        catch(...) {r.error=std::current_exception();}
        return r.ok;
    }
};
struct ModuleBindings { bool (*entry)(void*)=nullptr;IPluginFactory* (*factory)()=nullptr;bool (*exit)()=nullptr; };
struct LoaderOps {
    virtual ~LoaderOps()=default;
    virtual void* open(const char* exactPath)=0;
    virtual ModuleBindings resolve(void* ownHandle)=0;
    virtual int close(void* ownHandle)=0;
};
// Host context and loader are borrowed from the caller and must outlive this
// session. A queried IHostApplication reference is separately owned until close.
class InjectedModule {
    LoaderOps& loader;
    void* handle=nullptr;ModuleBindings bindings{};IPluginFactory* pluginFactory=nullptr;
    IPluginFactory3* factory3=nullptr;IHostApplication* host=nullptr;
    bool loadAttempted=false,entered=false,closeAttempted=false,prematureClose=false;
    unsigned liveInstances=0;
    std::array<char,16> selected{};
    std::array<PClassInfo,32> inventory{};int32 inventoryCount=0;
public:
    InitializerCalls<128> calls;
    std::exception_ptr primaryError;
    bool loaded=false,loadSucceeded=false,closedCleanly=false;
    explicit InjectedModule(LoaderOps& ops):loader(ops){}
    InjectedModule(const InjectedModule&)=delete;InjectedModule& operator=(const InjectedModule&)=delete;
    void load(const char* exactPath,const TUID selectedCid,FUnknown& context) {
        require(!loadAttempted && !closeAttempted,"module initialization retry forbidden");loadAttempted=true;
        try {
            require(exactPath && *exactPath && selectedCid,"module path/CID required");
            const std::filesystem::path path(exactPath);require(path.is_absolute() && path.lexically_normal().string()==exactPath,"canonical absolute module path required");
            std::memcpy(selected.data(),selectedCid,16);
            require(std::any_of(selected.begin(),selected.end(),[](auto c){return c!=0;}),"nonzero selected CID");
            require(calls.run("host-query",kResultOk,[&]{return context.queryInterface(IHostApplication::iid,reinterpret_cast<void**>(&host));}) && host,"mandatory IHostApplication");
            require(calls.run("loader-open",1,[&]{handle=loader.open(exactPath);return handle?1:0;}),"loader open failed");
            require(calls.run("loader-resolve",1,[&]{bindings=loader.resolve(handle);return bindings.entry && bindings.factory && bindings.exit?1:0;}),"module exports missing");
            require(calls.run("ModuleEntry",1,[&]{return bindings.entry(handle)?1:0;}),"ModuleEntry failed");entered=true;
            require(calls.run("GetPluginFactory",1,[&]{pluginFactory=bindings.factory();return pluginFactory?1:0;}),"factory missing");
            tresult query=kInternalError;
            const bool queryReturned=calls.run("factory3-query",0,[&]{query=pluginFactory->queryInterface(IPluginFactory3::iid,reinterpret_cast<void**>(&factory3));return query;},true);
            const bool queryValid=queryReturned && ((query==kNoInterface && !factory3) || (query==kResultOk && factory3));
            calls.rows[calls.count-1].ok=queryValid;require(queryValid,"factory3 query failed");
            if(factory3)require(calls.run("factory3-host",kResultOk,[&]{return factory3->setHostContext(host);}),"factory host context failed");
            int32 count=0;require(calls.run("class-count",1,[&]{count=pluginFactory->countClasses();return count;},true) && count>0 && count<=32,"bounded factory class count");
            inventoryCount=count;int matches=0;
            for(int32 i=0;i<count;++i) {PClassInfo info{};require(calls.run("class-info",kResultOk,[&]{return pluginFactory->getClassInfo(i,&info);}),"factory class info failed");
                require(std::memchr(info.category,0,sizeof(info.category)),"terminated class category");inventory[static_cast<std::size_t>(i)]=info;
                if(std::memcmp(info.cid,selected.data(),16)==0) {require(std::memchr(info.category,0,sizeof(info.category)) && std::strcmp(info.category,kVstAudioEffectClass)==0,"selected class is not audio effect");++matches;}}
            require(matches==1,"exact unique selected component CID required");loaded=true;loadSucceeded=true;
        }catch(...) {primaryError=std::current_exception();try{close();}catch(...){}std::rethrow_exception(primaryError);}
    }
    IPluginFactory& factory(){require(loaded && !closeAttempted,"module is not live");return *pluginFactory;}
    FUnknown& context(){require(loaded && !closeAttempted,"host context is not live");return *host;}
    const char* cid()const{return selected.data();}
    bool hasControllerClass(const std::array<std::uint8_t,16>& cid)const {
        int matches=0;for(int32 i=0;i<inventoryCount;++i){const auto& c=inventory[static_cast<std::size_t>(i)];if(std::memcmp(c.cid,cid.data(),16)==0 && std::strcmp(c.category,kVstComponentControllerClass)==0)++matches;}
        return matches==1;
    }
    void retainInstance(){require(loaded && !closeAttempted && !prematureClose && liveInstances==0,"one live instance per module required");++liveInstances;}
    void releaseInstance() noexcept {if(liveInstances)--liveInstances;}

    void close() {
        require(!closeAttempted,"module close retry forbidden");
        if(liveInstances){prematureClose=true;require(false,"cannot unload module with live instance");}
        closeAttempted=true;loaded=false;bool ok=!prematureClose;
        if(factory3){auto* p=std::exchange(factory3,nullptr);ok=calls.run("factory3-release",0,[&]{return p->release();},true)&&ok;}
        if(pluginFactory){auto* p=std::exchange(pluginFactory,nullptr);ok=calls.run("factory-release",0,[&]{return p->release();},true)&&ok;}
        if(entered){entered=false;ok=calls.run("ModuleExit",1,[&]{return bindings.exit()?1:0;})&&ok;}
        if(handle){auto* h=std::exchange(handle,nullptr);ok=calls.run("loader-close",0,[&]{return loader.close(h);})&&ok;}
        if(host){auto* p=std::exchange(host,nullptr);ok=calls.run("host-release",0,[&]{return p->release();},true)&&ok;}
        closedCleanly=ok;require(ok,"module cleanup failed");
    }
    ~InjectedModule(){if(!closeAttempted && liveInstances==0){try{close();}catch(...){}}}
};

struct SynthSetup { double rate=48000;int32 mode=kRealtime,block=64; };
// This subset supports the exact stereo synth topology (zero audio inputs,
// one stereo output, one event input, no event output). Other layouts fail.
class ProcessingInstanceOwner;
class InjectedInstance {
    friend class ProcessingInstanceOwner;
public:
    enum class Phase { Fresh,Connected,Processing,Suspended,Closed,Failed };
private:
    InjectedModule& module;Handler& handler;
    NativeOwnership own;
    FUnknown* componentIdentity=nullptr;FUnknown* controllerIdentity=nullptr;
    struct Bus {MediaType media;BusDirection direction;int32 index;bool active=false;};
    std::array<Bus,2> buses{{{kAudio,kOutput,0,false},{kEvent,kInput,0,false}}};
    bool startAttempted=false,closeAttempted=false,moduleRetained=false;
    std::optional<CreatedIdentity> identity;
    Phase phase_=Phase::Fresh;SynthSetup setup_{};unsigned suspensions_=0,resumes_=0;
    bool operation_=false,metadataAttempted_=false;
    bool deferFailureCleanup_=false; // private owned-worker boundary; synchronous default unchanged
    StateObservation saved_;StateRestoreReceipt restored_;Bytes connectedMetadata_,setupMetrics_;ConnectedParameterRoster connectedRoster_;
    void fail(std::exception_ptr error) {
        if(!primaryError)primaryError=error;
        phase_=Phase::Failed;
        if(!operation_ && !closeAttempted && !deferFailureCleanup_){try{close();}catch(...){}}
        std::rethrow_exception(primaryError);
    }
    static void validateSetup(const SynthSetup& config) {
        require((config.rate==44100 || config.rate==48000 || config.rate==96000)
          && (config.mode==kRealtime || config.mode==kOffline)
          && (config.block==64 || config.block==256 || config.block==1024),"frozen setup grid required");
    }
    void phaseGuard(Phase expected) {
        require(phase_==expected && !closeAttempted && !primaryError && !operation_,"instance phase/operation guard");
    }
    void activateAndStart() {
        for(auto& b:buses){require(calls.run(b.media==kAudio?"audio-bus-activate":"event-bus-activate",kResultOk,[&]{return own.component->activateBus(b.media,b.direction,b.index,true);}),"bus activate failed");b.active=true;}
        require(calls.run("activate",kResultOk,[&]{return own.component->setActive(true);}),"component activate failed");own.active=true;
        require(calls.run("start-processing",kResultOk,[&]{return own.audio->setProcessing(true);}),"processing start failed");own.processing=true;
        phase_=Phase::Processing;started=true;
    }
    bool stopAndDeactivate() noexcept {
        bool ok=true;started=false;
        if(own.processing){own.processing=false;ok=calls.run("stop-processing",kResultOk,[&]{return own.audio->setProcessing(false);})&&ok;}
        if(own.active){own.active=false;ok=calls.run("deactivate",kResultOk,[&]{return own.component->setActive(false);})&&ok;}
        for(auto i=buses.rbegin();i!=buses.rend();++i)if(i->active){i->active=false;ok=calls.run(i->media==kAudio?"audio-bus-deactivate":"event-bus-deactivate",kResultOk,[&]{return own.component->activateBus(i->media,i->direction,i->index,false);})&&ok;}
        return ok;
    }
public:
    // Worst admitted schedule: separate initialization31 + optional metadata1 and
    // setup metrics1 + two suspend/resume pairs16 + final close4 =53. Partial
    // failure cannot add another lifecycle attempt; at most two temporary
    // identity releases add two cleanup rows. State/native ownership have their
    // own bounded receipts. Keep explicit headroom without dynamic growth.
    static constexpr std::size_t maximumScheduledCalls=55;
    static_assert(maximumScheduledCalls<=96);
    InitializerCalls<96> calls;
    std::exception_ptr primaryError;
    bool started=false,initializationSucceeded=false,closedCleanly=false;
    private:
    const ControllerQueryPolicy queryPolicy_;ControllerQueryObservation queryObservation_;
public:
    ControllerQueryObservation controllerQueryObservation()const noexcept{return queryObservation_;}
    explicit InjectedInstance(InjectedModule& m,Handler& h,ControllerQueryPolicy p=ControllerQueryPolicy::StrictSdk):module(m),handler(h),queryPolicy_(p){validateControllerQueryPolicy(p);queryObservation_.policy=p;}
    InjectedInstance(const InjectedInstance&)=delete;InjectedInstance& operator=(const InjectedInstance&)=delete;
    void start(SynthSetup config) {
        try {phaseGuard(Phase::Fresh);validateSetup(config);initializeConnected();prepareAndStart(config);}
        catch(...){fail(std::current_exception());}
    }
    void initializeConnected() {
        try {
            phaseGuard(Phase::Fresh);require(!startAttempted,"instance initialization retry forbidden");startAttempted=true;
            module.retainInstance();moduleRetained=true;
            auto& factory=module.factory();
            require(calls.run("component-create",kResultOk,[&]{return factory.createInstance(module.cid(),IComponent::iid,reinterpret_cast<void**>(&own.component));}) && own.component,"component create failed");
            tresult controllerQuery=kInternalError;queryObservation_.attempted=true;
            const bool queryReturned=calls.run("combined-query",0,[&]{controllerQuery=own.component->queryInterface(IEditController::iid,reinterpret_cast<void**>(&own.controller));return controllerQuery;},true);
            queryObservation_.normal=queryReturned;queryObservation_.result=controllerQuery;queryObservation_.pointerPresent=own.controller!=nullptr;
            const bool absent=controllerQuery==kNoInterface || (queryPolicy_==ControllerQueryPolicy::PinnedJuceFalseNull && controllerQuery==kResultFalse);
            const bool queryValid=queryReturned && ((controllerQuery==kResultOk && own.controller) || (absent && !own.controller));
            calls.rows[calls.count-1].ok=queryValid;require(queryValid,"controller query failed");
            const bool combined=controllerQuery==kResultOk;
            identity=CreatedIdentity::capture(*own.component,module.cid(),combined?ControllerKind::Combined:ControllerKind::Separate,ObserverPhase::Created);
            require(calls.run("component-initialize",kResultOk,[&]{return own.component->initialize(&module.context());}),"component initialize failed");own.componentInitialized=true;
            require(calls.run("audio-query",kResultOk,[&]{return own.component->queryInterface(IAudioProcessor::iid,reinterpret_cast<void**>(&own.audio));}) && own.audio,"audio interface missing");
            if(!combined) {
                const auto& cid=identity->separateControllerCid();require(module.hasControllerClass(cid),"cached separate controller CID absent/ambiguous in factory inventory");
                require(calls.run("controller-create",kResultOk,[&]{return factory.createInstance(reinterpret_cast<const char*>(cid.data()),IEditController::iid,reinterpret_cast<void**>(&own.controller));}) && own.controller,"controller create failed");
                require(calls.run("component-identity",kResultOk,[&]{return own.component->queryInterface(FUnknown::iid,reinterpret_cast<void**>(&componentIdentity));}) && componentIdentity,"component identity unavailable");
                require(calls.run("controller-identity",kResultOk,[&]{return own.controller->queryInterface(FUnknown::iid,reinterpret_cast<void**>(&controllerIdentity));}) && controllerIdentity,"controller identity unavailable");
                const bool distinct=componentIdentity!=controllerIdentity;
                auto* a=std::exchange(componentIdentity,nullptr);require(calls.run("component-identity-release",0,[&]{return a->release();},true),"component identity release failed");
                auto* b=std::exchange(controllerIdentity,nullptr);require(calls.run("controller-identity-release",0,[&]{return b->release();},true),"controller identity release failed");
                require(distinct,"factory-created controller aliases component");
                require(calls.run("controller-initialize",kResultOk,[&]{return own.controller->initialize(&module.context());}),"controller initialize failed");own.controllerInitialized=true;
            }
            require(calls.run("handler-attach",kResultOk,[&]{return own.controller->setComponentHandler(&handler);}),"handler attach failed");own.handlerAttached=true;
            if(!combined) {
                require(calls.run("component-connection-query",kResultOk,[&]{return own.component->queryInterface(IConnectionPoint::iid,reinterpret_cast<void**>(&own.componentConnection));}) && own.componentConnection,"component connection unavailable");
                require(calls.run("controller-connection-query",kResultOk,[&]{return own.controller->queryInterface(IConnectionPoint::iid,reinterpret_cast<void**>(&own.controllerConnection));}) && own.controllerConnection,"controller connection unavailable");
                require(calls.run("component-connect",kResultOk,[&]{return own.componentConnection->connect(own.controllerConnection);}),"component connect failed");own.componentConnected=true;
                require(calls.run("controller-connect",kResultOk,[&]{return own.controllerConnection->connect(own.componentConnection);}),"controller connect failed");own.controllerConnected=true;
            }
            Stream initial;require(calls.run("component-state",kResultOk,[&]{return own.component->getState(&initial);}) && !initial.failed,"initial component state failed");
            require(initial.seek(0,IBStream::kIBSeekSet,nullptr)==kResultOk,"initial state rewind");
            require(calls.run("controller-state",kResultOk,[&]{return own.controller->setComponentState(&initial);}) && !initial.failed,"initial controller state failed");
            phase_=Phase::Connected;
        }catch(...){fail(std::current_exception());}
    }
    void prepareAndStart(SynthSetup config,bool captureMetrics=false) {
        try {
            phaseGuard(Phase::Connected);validateSetup(config);setup_=config;
            for(auto media:{kAudio,kEvent})for(auto direction:{kInput,kOutput}) {
                int32 count=-1;const int32 wanted=(media==kAudio && direction==kOutput) || (media==kEvent && direction==kInput)?1:0;
                require(calls.run("bus-count",0,[&]{count=own.component->getBusCount(media,direction);return count;},true) && count==wanted,"frozen synth bus topology");
            }
            for(const auto& b:buses) {
                BusInfo info{};require(calls.run(b.media==kAudio?"audio-bus-info":"event-bus-info",kResultOk,[&]{return own.component->getBusInfo(b.media,b.direction,b.index,info);})
                    && info.mediaType==b.media && info.direction==b.direction && (b.media==kAudio?info.channelCount==2:info.channelCount>0 && info.channelCount<=16),"frozen bus metadata differs");
            }
            SpeakerArrangement stereo=SpeakerArr::kStereo;
            require(calls.run("bus-arrangements",kResultOk,[&]{return own.audio->setBusArrangements(nullptr,0,&stereo,1);}),"stereo arrangement failed");
            SpeakerArrangement observed=0;require(calls.run("bus-arrangement-readback",kResultOk,[&]{return own.audio->getBusArrangement(kOutput,0,observed);}) && observed==stereo,"arrangement readback differs");
            require(calls.run("sample-size",kResultOk,[&]{return own.audio->canProcessSampleSize(kSample32);}),"sample format unsupported");
            ProcessSetup setup{config.mode,kSample32,config.block,config.rate};
            require(calls.run("setup",kResultOk,[&]{return own.audio->setupProcessing(setup);}),"processing setup failed");
            if(captureMetrics)require(calls.run("setup-metrics",1,[&]{cs80_observer::captureSetupMetrics(*own.audio,ObserverPhase::Setup,setupMetrics_);return 1;}),"setup metrics failed");
            activateAndStart();initializationSucceeded=true;
        }catch(...){fail(std::current_exception());}
    }
    void captureConnected() {
        try {phaseGuard(Phase::Connected);require(!metadataAttempted_,"connected metadata retry forbidden");metadataAttempted_=true;
            require(calls.run("connected-metadata",1,[&]{cs80_observer::captureConnectedMetadata(*own.component,*own.audio,*own.controller,*identity,ObserverPhase::Connected,connectedMetadata_,&connectedRoster_);return 1;}),"connected metadata failed");
        }catch(...){fail(std::current_exception());}
    }
    void saveState(ControllerStatePolicy policy) {
        try {phaseGuard(Phase::Processing);require(suspensions_==0 && !saved_.attempted,"save only once in initial history");
            observeStates(*own.component,*own.controller,policy,saved_);
        }catch(...){fail(std::current_exception());}
    }
    void suspend() {
        try {phaseGuard(Phase::Processing);require(suspensions_<2 && suspensions_==resumes_,"bounded suspension history");
            require(saved_.accepted,"initial history state must be saved before suspension");
            ++suspensions_;require(stopAndDeactivate(),"suspension failed");phase_=Phase::Suspended;
        }catch(...){fail(std::current_exception());}
    }
    void restoreSavedState() {
        try {phaseGuard(Phase::Suspended);require(suspensions_==1 && resumes_==0 && !restored_.attempted,"restore only once in first suspension");
            restoreStates(*own.component,*own.controller,saved_,restored_);
        }catch(...){fail(std::current_exception());}
    }
    void resume() {
        try {phaseGuard(Phase::Suspended);require(suspensions_==resumes_+1 && suspensions_<=2 && restored_.accepted,"resume requires first exact restore and bounded history");
            activateAndStart();++resumes_;
        }catch(...){fail(std::current_exception());}
    }
    template<class Function> void withProcessingOperation(Function&& function) {
        try {phaseGuard(Phase::Processing);operation_=true;
            struct Reset {bool& active;~Reset(){active=false;}} reset{operation_};
            static_assert(std::is_void_v<std::invoke_result_t<Function,IAudioProcessor&,const SynthSetup&>>,"processing operation callbacks return void");
            std::forward<Function>(function)(*own.audio,static_cast<const SynthSetup&>(setup_));
            require(phase_==Phase::Processing && !closeAttempted && !primaryError,"processing operation became terminal");
        }catch(...){fail(std::current_exception());throw;}
    }
    IAudioProcessor& processingAudio(){try{phaseGuard(Phase::Processing);return *own.audio;}catch(...){fail(std::current_exception());throw;}}
    const SynthSetup& processingSetup(){try{phaseGuard(Phase::Processing);return setup_;}catch(...){fail(std::current_exception());throw;}}
    IComponent& connectedComponent(){try{phaseGuard(Phase::Connected);return *own.component;}catch(...){fail(std::current_exception());throw;}}
    IEditController& connectedController(){try{phaseGuard(Phase::Connected);return *own.controller;}catch(...){fail(std::current_exception());throw;}}
    Phase phase()const noexcept{return phase_;}
    unsigned suspensionCount()const noexcept{return suspensions_;}
    unsigned resumeCount()const noexcept{return resumes_;}
    const StateObservation& savedStateReceipt()const noexcept{return saved_;}
    const StateRestoreReceipt& restoreReceipt()const noexcept{return restored_;}
    const ConnectedParameterRoster& connectedParameterRoster()const noexcept{return connectedRoster_;}
    const Bytes& connectedMetadataReceipt()const noexcept{return connectedMetadata_;}
    const Bytes& setupMetricsReceipt()const noexcept{return setupMetrics_;}
    void close() {
        try {
        require(!operation_,"cannot close inside active processing operation");
        require(!closeAttempted,"instance close retry forbidden");closeAttempted=true;started=false;bool ok=true;
        if(phase_!=Phase::Failed)phase_=Phase::Closed;
        ok=stopAndDeactivate()&&ok;
        if(componentIdentity){auto* p=std::exchange(componentIdentity,nullptr);ok=calls.run("component-identity-release",0,[&]{return p->release();},true)&&ok;}
        if(controllerIdentity){auto* p=std::exchange(controllerIdentity,nullptr);ok=calls.run("controller-identity-release",0,[&]{return p->release();},true)&&ok;}
        try{own.close();}catch(...){ok=false;}
        if(moduleRetained){module.releaseInstance();moduleRetained=false;}
        closedCleanly=ok;require(ok,"instance cleanup failed");
        }catch(...){phase_=Phase::Failed;if(!primaryError)primaryError=std::current_exception();throw;}
    }
    const NativeOwnership& ownershipReceipt()const{return own;}
    const CreatedIdentity& createdIdentity()const{require(identity.has_value(),"no Created identity");return *identity;}
    ~InjectedInstance(){if(!closeAttempted){try{close();}catch(...){}}}
};
} // namespace cs80_observer
