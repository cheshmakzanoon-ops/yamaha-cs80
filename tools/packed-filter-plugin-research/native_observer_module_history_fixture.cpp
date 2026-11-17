#define INIT_CLASS_IID
#include "native_observer_module_history.hpp"
#include <iomanip>
#include <cstdarg>
#include <iostream>
using namespace cs80_observer;using namespace cs80_identity;
static std::string mode;static int infos=0,symbols=0,closes=0;static ModuleHistoryOwner* activeOwner=nullptr;static std::unique_ptr<ModuleHistoryOwner>* activeOwned=nullptr;static std::atomic<bool> misuseSeen=false;
extern "C" int __real_dlinfo(void*,int,void*);
extern "C" int __wrap_dlinfo(void* h,int request,void* out){++infos;if((mode=="constructor-thread-postopen" && infos==2)||(mode=="constructor-thread-internalproof" && infos==1))return -1;return __real_dlinfo(h,request,out);}
extern "C" void* __real_dlsym(void*,const char*);
extern "C" void* __wrap_dlsym(void* h,const char* name){++symbols;if(mode=="constructor-thread-resolve" && symbols==1)return nullptr;return __real_dlsym(h,name);}
extern "C" int __real_dlclose(void*);
extern "C" int __wrap_dlclose(void* h){++closes;return __real_dlclose(h);}
extern "C" void cs80_test_misuse(){if(mode=="destroy-active"){activeOwned->reset();return;}TUID cid{};std::memset(cid,5,16);try{activeOwner->run(cid,{packed_filter_workload::Policy::legacy,48000,packed_filter_workload::Mode::realtime,8,64},ControllerStatePolicy::Supported,{worker::Clock::now()+std::chrono::seconds(1),100,std::chrono::milliseconds(1)});}catch(...){misuseSeen=true;}}
static int taskReads=0;
extern "C" int __real_open(const char*,int,...);
extern "C" int __wrap_open(const char* path,int flags,...){
 if(std::string_view(path).starts_with("/proc/self/task/")){++taskReads;
    if(mode=="task-baseline-unreadable" || (mode=="task-postexit-missing" && taskReads==2)){errno=ENOENT;return -1;}
    if(mode=="task-postexit-malformed" && taskReads==2){int fds[2];if(::pipe(fds))return -1;const char text[]="truncated task stat";::write(fds[1],text,sizeof(text)-1);::close(fds[1]);return fds[0];}}
 if(flags&O_CREAT){va_list args;va_start(args,flags);int permissions=va_arg(args,int);va_end(args);return __real_open(path,flags,permissions);}return __real_open(path,flags);
}
Root root(std::istream& in){Root r;in>>std::quoted(r.path)>>r.mode>>r.uid>>r.gid;return r;}
Entry entry(std::istream& in){Entry e;in>>std::quoted(e.path)>>e.directory>>e.mode>>e.uid>>e.gid>>e.size>>std::quoted(e.sha256);return e;}
int main(int argc,char** argv){try{
 require(argc==3,"arguments");std::ifstream in(argv[1]);ExpectedIdentity expected;expected.arm="stock";expected.bundle=root(in);in>>std::quoted(expected.module);std::size_t n;in>>n;for(std::size_t i=0;i<n;++i)expected.bundleEntries.push_back(entry(in));require(bool(in),"manifest");
 auto identity=VerifiedIdentity::verify(expected,{64,64,8*1024*1024,4096});auto pin=PinnedModule::pin(identity);const std::string test=argv[2];mode=test;::setenv("CS80_JOIN_MODE",test.c_str(),1);
 if(test=="destroy-active")std::set_terminate([]{std::cerr<<"active-owner-destructor blocked\n";std::_Exit(29);});
 auto owned=std::make_unique<ModuleHistoryOwner>(pin);auto& owner=*owned;activeOwned=&owned;activeOwner=&owner;if(test=="overlap-refused"){bool denied=false;try{ModuleHistoryOwner second(pin);}catch(...){denied=true;}require(denied,"second active graph rejected before load");}TUID cid{};std::memset(cid,5,16);const packed_filter_workload::Case c{packed_filter_workload::Policy::legacy,48000,packed_filter_workload::Mode::realtime,8,64};bool failed=false;
 try{owner.run(cid,c,ControllerStatePolicy::Supported,{worker::Clock::now()+std::chrono::seconds(5),100000,std::chrono::milliseconds(1)});}catch(const std::exception& e){failed=true;std::cerr<<"observed: "<<e.what()<<'\n';}
 const bool good=test=="valid" || test=="retry-after-success" || test=="overlap-refused";require(failed!=good && owner.admitted()==good,"actual module history outcome");
 if(good){require(!owner.quarantined() && owner.history().renderPresent[3] && !owner.history().canonicalPayload.data.empty(),"closed source A-D actual module");
 if(test=="retry-after-success"){bool denied=false;try{owner.run(cid,c,ControllerStatePolicy::Supported,{worker::Clock::now()+std::chrono::seconds(1),10,std::chrono::milliseconds(1)});}catch(...){denied=true;}require(denied && !owner.admitted(),"successful owner retry cannot remain admitted");}return 0;}
 if(test.starts_with("constructor-thread") || test=="unregistered-thread" || test=="caught-nested" || test=="caught-wrong-thread" || test.starts_with("task-"))require(closes==0,"unsafe lifecycle must never reach actual dlclose");
 if(test.starts_with("caught-"))require(misuseSeen,"caught native callback misuse reached");
 require(owner.failureEvidence().opens==(test=="task-baseline-unreadable"?0u:1u) && (test=="task-baseline-unreadable" || owner.failureEvidence().module.count>0),"bounded terminal diagnostics retained");
 require(owner.quarantined() && owner.error(),"visible terminal quarantine");bool denied=false;try{ModuleHistoryOwner second(pin);}catch(...){denied=true;}require(denied,"caught quarantine cannot create new owner");
 denied=false;try{owner.run(cid,c,ControllerStatePolicy::Supported,{worker::Clock::now()+std::chrono::seconds(1),10,std::chrono::milliseconds(1)});}catch(...){denied=true;}require(denied,"quarantined owner cannot resume");
 if(test=="destructor-overrelease")require(owner.providerAbsenceConfirmed(),"report post-unload host failure truthfully");
 if(test=="retained-timer" || test=="exit-registers")require(owner.retainedRegistrations()>0,"actual retained callback diagnosed");
 if(test=="retained-host")require(owner.retainedHostReferences()>1,"actual retained host diagnosed");
 std::cerr<<"QUARANTINED: child must exit; no continuation\n";std::_Exit(23);
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
