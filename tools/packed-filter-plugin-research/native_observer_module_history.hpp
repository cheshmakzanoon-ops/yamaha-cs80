#pragma once
#include "native_sealed_loader.hpp"
#include "native_observer_history.hpp"
#include "native_observer_bounded_host.hpp"
#include "native_observer_runloop.hpp"
namespace cs80_observer {
namespace module_history_detail {
using ThreadStamp=std::pair<std::uint64_t,std::uint64_t>;
inline std::vector<ThreadStamp> threads(){
    DIR* directory=::opendir("/proc/self/task");require(directory,"task inventory unreadable");struct Close{DIR* d;~Close(){::closedir(d);}} close{directory};
    std::vector<std::uint64_t> ids;
    for(;;){errno=0;auto* entry=::readdir(directory);if(!entry){require(errno==0,"task inventory read error");break;}std::string_view name(entry->d_name);if(name=="." || name=="..")continue;
        require(ids.size()<4096 && name.size()<=20,"bounded task count/name");ids.push_back(cs80_provider::number(name,10));}
    require(!ids.empty(),"empty task inventory");std::sort(ids.begin(),ids.end());require(std::adjacent_find(ids.begin(),ids.end())==ids.end(),"duplicate task ID");
    std::vector<ThreadStamp> result;
    for(auto id:ids){const auto path="/proc/self/task/"+std::to_string(id)+"/stat";cs80_identity::detail::Fd fd(::open(path.c_str(),O_RDONLY|O_CLOEXEC));std::string text;std::array<char,1024> buffer{};
        for(;;){const auto n=::read(fd.n,buffer.data(),buffer.size());if(n<0 && errno==EINTR)continue;require(n>=0,"task stat read error");if(!n)break;require(text.size()+static_cast<std::size_t>(n)<=8192,"task stat size bound");text.append(buffer.data(),static_cast<std::size_t>(n));}
        const auto open=text.find('('),end=text.rfind(')');require(text.ends_with('\n') && open>0 && end!=text.npos && end>open && end+2<text.size() && text[end+1]==' ',"task stat structure/truncation");
        require(cs80_provider::number(std::string_view(text).substr(0,open-1),10)==id,"task stat PID mismatch");
        std::size_t cursor=end+2;std::uint64_t start=0;
        for(int field=3;field<=22;++field){const auto stop=text.find(' ',cursor);require(stop!=text.npos && stop>cursor,"task stat field missing");if(field==3)require(stop-cursor==1,"task state field");if(field==22)start=cs80_provider::number(std::string_view(text).substr(cursor,stop-cursor),10);cursor=stop+1;}
        require(start>0,"task start identity required");result.emplace_back(id,start);
    }
    return result;
}
}
struct ModuleHistoryFailure {const char* attempted="none";uint32 hostReferences=0;std::size_t registrations=0;bool providerAbsent=false;unsigned opens=0,closes=0;InitializerCalls<128> module;ControllerQueryObservation query;};
// Private advertised views share one FUnknown identity; Dispatcher never escapes.
class ProcessLifetimeChild;
class ModuleHistoryOwner final {
    friend class ProcessLifetimeChild;
    static inline std::atomic<bool> processTerminal_{false};static inline std::atomic<bool> activeOwner_{false};
    class Host final:public IHostApplication,public Steinberg::Linux::IRunLoop {
        std::atomic<uint32> refs_{1};std::atomic<bool> valid_{true};
        bounded_host::Context services_;runloop::Dispatcher* loop_=new runloop::Dispatcher;
    public:
        Host()=default;Host(const Host&)=delete;
        ~Host(){if(refs_!=1 || loop_->registrations())std::terminate();loop_->release();}
        tresult PLUGIN_API queryInterface(const TUID id,void** out)override{
            if(!out)return kInvalidArgument;
            *out=nullptr;if(!id)return kInvalidArgument;
            if(FUnknownPrivate::iidEqual(id,FUnknown::iid)||FUnknownPrivate::iidEqual(id,IHostApplication::iid))*out=static_cast<IHostApplication*>(this);
            else if(FUnknownPrivate::iidEqual(id,Steinberg::Linux::IRunLoop::iid))*out=static_cast<Steinberg::Linux::IRunLoop*>(this);
            else return kNoInterface;
            if(!addRef()){*out=nullptr;return kInternalError;}return kResultOk;
        }
        uint32 PLUGIN_API addRef()override{auto n=refs_.load();do{if(n==std::numeric_limits<uint32>::max()){valid_=false;return 0;}}while(!refs_.compare_exchange_weak(n,n+1));return n+1;}
        uint32 PLUGIN_API release()override{auto n=refs_.load();do{if(n<=1){valid_=false;return 1;}}while(!refs_.compare_exchange_weak(n,n-1));return n-1;}
        tresult PLUGIN_API getName(String128 name)override{return services_.application().getName(name);}
        tresult PLUGIN_API createInstance(TUID cid,TUID iid,void** out)override{return services_.application().createInstance(cid,iid,out);}
        tresult PLUGIN_API registerEventHandler(Steinberg::Linux::IEventHandler* h,Steinberg::Linux::FileDescriptor fd)override{return loop_->registerEventHandler(h,fd);}
        tresult PLUGIN_API unregisterEventHandler(Steinberg::Linux::IEventHandler* h)override{return loop_->unregisterEventHandler(h);}
        tresult PLUGIN_API registerTimer(Steinberg::Linux::ITimerHandler* h,Steinberg::Linux::TimerInterval interval)override{return loop_->registerTimer(h,interval);}
        tresult PLUGIN_API unregisterTimer(Steinberg::Linux::ITimerHandler* h)override{return loop_->unregisterTimer(h);}
        void pump(std::uint64_t deadline){require(valid_ && services_.healthy() && loop_->pump({deadline,64,8}).ok,"owned host pump failed");}
        bool unloadGate(){return valid_ && refs_==2 && services_.finish() && loop_->healthy() && loop_->registrations()==0 && loop_->shutdown();}
        bool finalGate()const{return valid_ && refs_==1 && services_.finish() && loop_->healthy() && loop_->registrations()==0;}
        uint32 refs()const noexcept{return refs_;}
        std::size_t registrations()const noexcept{return loop_->registrations();}
    };
    struct Graph;
    class Loader final:public LoaderOps {
        SealedLoader sealed_;Host& host_;InjectedModule* module_=nullptr;const bool* historyClosed_=nullptr;const std::vector<module_history_detail::ThreadStamp>* baseline_=nullptr;std::vector<module_history_detail::ThreadStamp>* after_=nullptr;bool refused_=false;
    public:
        Loader(std::shared_ptr<const cs80_identity::PinnedModule> pin,Host& host,ModuleLifetimeProfile profile):sealed_(std::move(pin),SealedCleanupPolicy::ExplicitOnly,profile),host_(host){}
        void bind(InjectedModule& module,const bool& historyClosed,const std::vector<module_history_detail::ThreadStamp>& baseline,std::vector<module_history_detail::ThreadStamp>& after){require(!module_,"single private module binding");module_=&module;historyClosed_=&historyClosed;baseline_=&baseline;after_=&after;}
        void* open(const char* path)override{return sealed_.open(path);}
        ModuleBindings resolve(void* handle)override{return sealed_.resolve(handle);}
        int close(void* handle)override{
            // Module.close records factory releases and ModuleExit before calling us.
            refused_=true;
            bool exitSeen=false,exitOk=false,releasesOk=true,factorySeen=false,factory3Seen=false,factory3Needed=false;
            require(module_,"bound module required");
            for(std::size_t i=0;i<module_->calls.count;++i){const auto& call=module_->calls.rows[i];
                if(std::strcmp(call.name,"factory3-host")==0)factory3Needed=true;
                if(std::strcmp(call.name,"factory-release")==0)factorySeen=true;
                if(std::strcmp(call.name,"factory3-release")==0)factory3Seen=true;
                if(std::strcmp(call.name,"ModuleExit")==0){exitSeen=true;exitOk=call.normal && call.ok && !call.error;}
                if(std::strcmp(call.name,"factory3-release")==0 || std::strcmp(call.name,"factory-release")==0)releasesOk=releasesOk && call.normal && call.ok && !call.error;
            }
            require(after_,"owned post-exit task inventory");*after_=module_history_detail::threads();
            if(processTerminal_ || !historyClosed_ || !*historyClosed_ || !factorySeen || (factory3Needed && !factory3Seen) || !exitSeen || !exitOk || !releasesOk || !baseline_ || *after_!=*baseline_ || !host_.unloadGate()){refused_=true;throw std::runtime_error("unsafe module unload refused");}
            const auto status=sealed_.close(handle);refused_=false;return status;
        }
        bool admitted()const{return !refused_ && sealed_.lifecycleClosedCleanly();}
        bool refused()const{return refused_;}
        const SealedLoader& evidence()const{return sealed_;}
    };
    struct Graph {
        const std::shared_ptr<const cs80_identity::PinnedModule> pin;
        bool historyClosed=false;std::vector<module_history_detail::ThreadStamp> baseline,after;Host host;Loader loader;InjectedModule module;Handler handler;std::unique_ptr<HistoryOwner> history;
        explicit Graph(std::shared_ptr<const cs80_identity::PinnedModule> p,ModuleLifetimeProfile profile):pin(std::move(p)),loader(pin,host,profile),module(loader){loader.bind(module,historyClosed,baseline,after);}
    };
    static std::unique_ptr<Graph> allocate(std::shared_ptr<const cs80_identity::PinnedModule> pin,ModuleLifetimeProfile profile){require(!processTerminal_,"child process terminal after quarantine");bool expected=false;require(activeOwner_.compare_exchange_strong(expected,true),"one active module history graph per child");try{require(!processTerminal_,"terminal child cannot allocate graph");return std::make_unique<Graph>(std::move(pin),profile);}catch(...){activeOwner_=false;throw;}}
    const std::thread::id ui_=std::this_thread::get_id();std::unique_ptr<Graph> graph_;
    std::atomic<bool> violation_{false};bool running_=false;
    bool ownsActive_=true;bool attempted_=false,admitted_=false,quarantined_=false,providerAbsent_=false;std::exception_ptr error_;
    ModuleHistoryFailure failure_;const char* attemptedStep_="not-started";
    uint32 lastHostRefs_=0;std::size_t lastRegistrations_=0;
    void receiptAccess()const{require(std::this_thread::get_id()==ui_ && !running_,"module history UI idle receipt");}
    void check(){require(!violation_ && !processTerminal_,"module history terminal violation");}
    void quarantine()noexcept{processTerminal_=true;if(graph_){providerAbsent_=graph_->loader.evidence().providerClosedCleanly();lastHostRefs_=graph_->host.refs();lastRegistrations_=graph_->host.registrations();failure_.attempted=attemptedStep_;failure_.hostReferences=lastHostRefs_;failure_.registrations=lastRegistrations_;failure_.providerAbsent=providerAbsent_;failure_.opens=graph_->loader.evidence().nativeOpenCalls();failure_.closes=graph_->loader.evidence().nativeCloseCalls();failure_.module=graph_->module.calls;if(graph_->history)failure_.query=graph_->history->controllerQueryObservation();graph_.release();}quarantined_=true;}
    ModuleHistoryOwner(std::shared_ptr<const cs80_identity::PinnedModule> pin,ModuleLifetimeProfile profile):graph_(allocate(std::move(pin),profile)){}
public:
    explicit ModuleHistoryOwner(std::shared_ptr<const cs80_identity::PinnedModule> pin):ModuleHistoryOwner(std::move(pin),ModuleLifetimeProfile::StrictUnmap){}
    ModuleHistoryOwner(const ModuleHistoryOwner&)=delete;ModuleHistoryOwner& operator=(const ModuleHistoryOwner&)=delete;
    void run(const TUID cid,packed_filter_workload::Case c,ControllerStatePolicy policy,worker::WaitPolicy wait,ControllerQueryPolicy queryPolicy=ControllerQueryPolicy::StrictSdk){
        if(std::this_thread::get_id()!=ui_){violation_=true;processTerminal_=true;throw std::runtime_error("module history wrong thread");}
        if(running_ || attempted_){violation_=true;processTerminal_=true;throw std::runtime_error("module history reentry/retry");}
        running_=true;struct Reset{bool& running;~Reset(){running=false;}} reset{running_};attempted_=true;
        try{check();require(bool(graph_),"owned graph required");validateControllerQueryPolicy(queryPolicy);
            attemptedStep_="thread-baseline";graph_->baseline=module_history_detail::threads();
            attemptedStep_="module-load";graph_->module.load(graph_->pin->identity()->modulePath().c_str(),cid,static_cast<IHostApplication&>(graph_->host));check();
            attemptedStep_="history-construction";graph_->history=std::make_unique<HistoryOwner>(graph_->module,graph_->handler,c,policy,queryPolicy);
            const auto deadline=static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(wait.deadline.time_since_epoch()).count());
            attemptedStep_="instance-history";graph_->history->run(wait,[&]{check();graph_->host.pump(deadline);check();},[&]{check();});check();require(graph_->history->admitted(),"instance history not admitted");graph_->historyClosed=true;
            attemptedStep_="module-close";graph_->module.close();check();require(graph_->module.closedCleanly && graph_->loader.admitted() && graph_->host.finalGate(),"module/provider/host close not admitted");admitted_=true;ownsActive_=false;activeOwner_=false;
        }catch(...){error_=std::current_exception();
            // Module may already have consumed its single close attempt. No retries.
            // Preserve entire ownership graph on ANY failure; destructor must not unload.
            quarantine();std::rethrow_exception(error_);
        }
    }
    const ModuleHistoryFailure& failureEvidence()const{receiptAccess();return failure_;}
    const SealedProviderEvidence& providerEvidence()const{receiptAccess();require(admitted_ && graph_,"closed provider evidence");return graph_->loader.evidence().providerEvidence();}
    bool admitted()const{receiptAccess();return admitted_ && !violation_ && !processTerminal_;}
    bool quarantined()const{receiptAccess();return quarantined_;}
    bool providerAbsenceConfirmed()const{receiptAccess();return providerAbsent_ || (admitted_ && graph_ && graph_->loader.evidence().providerClosedCleanly());}
    uint32 retainedHostReferences()const{receiptAccess();return lastHostRefs_;}
    std::size_t retainedRegistrations()const{receiptAccess();return lastRegistrations_;}
    std::exception_ptr error()const{receiptAccess();return error_;}
    const auto& threadBaseline()const{receiptAccess();require(admitted_ && graph_,"closed thread baseline");return graph_->baseline;}
    const auto& postExitThreads()const{receiptAccess();require(admitted_ && graph_,"closed post-exit thread evidence");return graph_->after;}
    const HistoryObservation& history()const{receiptAccess();require(admitted_ && graph_,"successful closed history only");return graph_->history->observation();}
    ~ModuleHistoryOwner(){if(std::this_thread::get_id()!=ui_ || running_)std::terminate();if(attempted_ && !admitted_ && graph_)quarantine();if(!attempted_ && ownsActive_){graph_.reset();ownsActive_=false;activeOwner_=false;}}
};
}
