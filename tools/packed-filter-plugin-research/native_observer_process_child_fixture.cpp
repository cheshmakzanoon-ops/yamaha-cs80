#define main strict_module_fixture_main
#define __wrap_open strict_wrap_open
#define __wrap_dlclose strict_wrap_dlclose
#include "native_observer_module_history_fixture.cpp"
#undef main
#undef __wrap_open
#undef __wrap_dlclose
#include "native_observer_process_child.hpp"
#include <sys/mman.h>
extern "C" int __wrap_dlclose(void* h){
 if(mode=="native-close-failure"){++closes;return -1;}
 const int result=strict_wrap_dlclose(h);
 if(mode=="identity-drift" && result==0){const int fd=__real_open(::getenv("CS80_TEST_MODULE"),O_WRONLY|O_APPEND);if(fd>=0){const char byte='x';::write(fd,&byte,1);::close(fd);}}
 return result;
}
extern "C" int __wrap_open(const char* path,int flags,...){
 if(closes && std::string_view(path)=="/proc/self/maps" && mode.starts_with("maps-")){
  std::ifstream input(path);std::string text((std::istreambuf_iterator<char>(input)),{});const std::string target=::getenv("CS80_TEST_MODULE");
  if(mode=="maps-malformed")text="not a maps row\n";
  else{const auto occurrence=text.find(target);require(occurrence!=text.npos,"actual retained map fixture");const auto begin=text.rfind('\n',occurrence);const auto start=begin==text.npos?0:begin+1;const auto end=text.find('\n',occurrence);
   if(mode=="maps-partial")text.erase(start,end-start+1);
   else if(mode=="maps-permission"){const auto perms=text.find(' ',start)+1;text[perms]=text[perms]=='r'?'-':'r';}
   else if(mode=="maps-path-mixed"){std::size_t pos=0;while((pos=text.find(target,pos))!=text.npos){text.replace(pos,target.size(),"/unrelated-file");pos+=15;}}
  }
  int fds[2];require(::pipe(fds)==0,"fixture maps pipe");
  // Snapshot is larger than pipe capacity: anonymous regular memfd avoids producer deadlock.
  ::close(fds[0]);::close(fds[1]);const int fd=::memfd_create("inert-map-snapshot",MFD_CLOEXEC);require(fd>=0,"fixture maps fd");require(::write(fd,text.data(),text.size())==static_cast<ssize_t>(text.size()),"fixture maps bytes");::lseek(fd,0,SEEK_SET);return fd;
 }
 if(flags&O_CREAT){va_list args;va_start(args,flags);int permissions=va_arg(args,int);va_end(args);return strict_wrap_open(path,flags,permissions);}return strict_wrap_open(path,flags);
}
extern "C" ssize_t __real_write(int,const void*,size_t);
extern "C" ssize_t __wrap_write(int fd,const void* p,size_t n){if(mode=="output-write"){struct stat s{};if(::fstat(fd,&s)==0 && S_ISREG(s.st_mode) && (::fcntl(fd,F_GETFL)&O_ACCMODE)==O_WRONLY){errno=ENOSPC;return -1;}}return __real_write(fd,p,n);}
extern "C" int __real_fsync(int);
extern "C" int __wrap_fsync(int fd){if(mode=="output-flush"){errno=EIO;return -1;}return __real_fsync(fd);}
extern "C" int __real_close(int);
extern "C" int __wrap_close(int fd){bool target=false;if(mode=="output-close"){struct stat s{};target=::fstat(fd,&s)==0 && S_ISREG(s.st_mode) && (::fcntl(fd,F_GETFL)&O_ACCMODE)==O_WRONLY;}const auto status=__real_close(fd);if(target){errno=EIO;return -1;}return status;}
int main(int argc,char** argv){try{
 require(argc==5,"process child fixture arguments");std::ifstream in(argv[1]);ExpectedIdentity expected;expected.arm="stock";expected.bundle=root(in);in>>std::quoted(expected.module);std::size_t n;in>>n;for(std::size_t i=0;i<n;++i)expected.bundleEntries.push_back(entry(in));require(bool(in),"manifest");
 auto identity=VerifiedIdentity::verify(expected,{64,64,8*1024*1024,4096});auto pin=PinnedModule::pin(identity);mode=argv[2];::setenv("CS80_TEST_MODULE",identity->modulePath().c_str(),1);::setenv("CS80_JOIN_MODE",mode.c_str(),1);
 TUID cid{};std::memset(cid,5,16);ProcessLifetimeChild::run(pin,cid,{packed_filter_workload::Policy::legacy,48000,packed_filter_workload::Mode::realtime,8,64},ControllerStatePolicy::Supported,{worker::Clock::now()+std::chrono::seconds(5),100000,std::chrono::milliseconds(1)},argv[3],argv[4]);
 }catch(...){return 2;}}
