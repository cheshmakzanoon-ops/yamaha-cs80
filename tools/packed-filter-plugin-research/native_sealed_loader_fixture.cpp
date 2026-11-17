#define INIT_CLASS_IID
#include "native_sealed_loader.hpp"
#include <iomanip>
#include <cstdarg>
#include <iostream>
using namespace cs80_observer;
using namespace cs80_identity;
static std::string action,resourcePath;static int mapsCalls=0;
extern "C" int __real_open(const char*,int,...);
extern "C" int __wrap_open(const char* path,int flags,...){
 if(std::string(path)=="/proc/self/maps"){++mapsCalls;if(action=="maps-pre-unreadable" && mapsCalls==1){errno=EACCES;return -1;}
 if((action=="maps-post-malformed" && mapsCalls==2)||(action=="maps-close-truncated" && mapsCalls==3)){int fds[2];if(::pipe(fds))return -1;const char* text=action=="maps-post-malformed"?"bad maps\n":"truncated";::write(fds[1],text,std::strlen(text));::close(fds[1]);return fds[0];}}
 if(flags&O_CREAT){va_list args;va_start(args,flags);int mode=va_arg(args,int);va_end(args);return __real_open(path,flags,mode);}return __real_open(path,flags);
}
static int infoCalls=0,symbolCalls=0,closeCalls=0;static void* failedCloseHandle=nullptr;
extern "C" int __real_dlinfo(void*,int,void*);
extern "C" int __wrap_dlinfo(void* h,int request,void* out){++infoCalls;if(action=="post-open-proof" && infoCalls==2)return -1;return __real_dlinfo(h,request,out);}
extern "C" void* __real_dlsym(void*,const char*);
extern "C" void* __wrap_dlsym(void* h,const char* name){++symbolCalls;if(action=="post-resolve-reverify" && symbolCalls==4)std::ofstream(resourcePath)<<"changed";if(action=="post-resolve-provider" && symbolCalls==4)return __real_dlsym(RTLD_DEFAULT,"puts");return __real_dlsym(h,name);}
extern "C" int __real_dlclose(void*);
extern "C" int __wrap_dlclose(void* h){++closeCalls;if(action=="close-failure"){failedCloseHandle=h;return -1;}return __real_dlclose(h);}
Root root(std::istream& in){Root r;in>>std::quoted(r.path)>>r.mode>>r.uid>>r.gid;return r;}
Entry entry(std::istream& in){Entry e;in>>std::quoted(e.path)>>e.directory>>e.mode>>e.uid>>e.gid>>e.size>>std::quoted(e.sha256);return e;}
int main(int argc,char** argv){try{require(argc==3,"arguments");action=argv[2];std::ifstream in(argv[1]);ExpectedIdentity expected;expected.arm="stock";expected.bundle=root(in);in>>std::quoted(expected.module);std::size_t n;in>>n;for(std::size_t i=0;i<n;++i)expected.bundleEntries.push_back(entry(in));require(bool(in),"manifest");
 resourcePath=expected.bundle.path+"/resource";const auto path=expected.bundle.path+"/"+expected.module;void* outside=nullptr;
 if(action=="preloaded" || action=="stale-path"){outside=::dlopen(path.c_str(),RTLD_NOW|RTLD_LOCAL);require(outside,"preload inert fixture");if(action=="stale-path"){auto copy=path+".copy";std::filesystem::copy_file(path,copy);std::filesystem::rename(copy,path);}}
 auto identity=VerifiedIdentity::verify(expected,{64,64,1024*1024,4096});auto pin=PinnedModule::pin(identity);SealedLoader loader(pin);bool failed=false;void* handle=nullptr;
 if(action=="constructor-mutation")::setenv("CS80_SEALED_MUTATE",(expected.bundle.path+"/resource").c_str(),1);
 try{handle=loader.open(path.c_str());auto exports=loader.resolve(handle);require(exports.entry && exports.factory && exports.exit,"resolved inert exports");
     if(action=="outside-reference"){outside=::dlopen(path.c_str(),RTLD_NOW|RTLD_LOCAL);require(outside,"extra reference");}
     if(action=="before-close-mutation")std::ofstream(expected.bundle.path+"/resource")<<"changed";
     loader.close(handle);
 }catch(...){failed=true;}
 ::unsetenv("CS80_SEALED_MUTATE");
 const bool good=action=="valid" || action=="reload";
 if(good)require(!loader.providerEvidence().beforeMaps.empty() && !loader.providerEvidence().loadedMaps.empty() && !loader.providerEvidence().afterMaps.empty() && !loader.providerEvidence().beforeObjects.empty() && !loader.providerEvidence().afterObjects.empty(),"complete retained provider evidence");
 require(failed!=good,"expected joined acceptance/rejection");require(loader.providerClosedCleanly()==good,"private joined result");
 if(!good)require(loader.terminalFailure(),"terminal failure cannot be erased");
 if(action=="preloaded" || action=="stale-path" || action=="maps-pre-unreadable")require(loader.nativeOpenCalls()==0 && loader.nativeCloseCalls()==0,"stale/preexisting rejected before load");
 else require(loader.nativeOpenCalls()==1 && loader.nativeCloseCalls()==1,"exact once native ownership cleanup");
 if(action=="reload"){SealedLoader fresh(pin);auto h=fresh.open(path.c_str());fresh.resolve(h);fresh.close(h);require(fresh.providerClosedCleanly() && fresh.exportFileOffsets()==loader.exportFileOffsets(),"fresh ASLR-independent reload proof");}
 if(outside)require(__real_dlclose(outside)==0,"outside reference cleanup");
 if(failedCloseHandle)require(__real_dlclose(failedCloseHandle)==0,"fixture injected close failure cleanup");
 return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
