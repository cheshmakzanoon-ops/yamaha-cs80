#include "native_identity_pin.hpp"
#include <fstream>
#include <iomanip>
#include <iostream>
using namespace cs80_identity;
static std::string mutatePath,mutateKind;static bool evpFail=false;
extern "C" ssize_t __real_read(int,void*,size_t);
extern "C" ssize_t __wrap_read(int fd,void*p,size_t n){auto result=__real_read(fd,p,n);struct stat a{},b{};if(!mutatePath.empty()&&result>0&&::fstat(fd,&a)==0&&::stat(mutatePath.c_str(),&b)==0&&a.st_dev==b.st_dev&&a.st_ino==b.st_ino){auto path=std::exchange(mutatePath,{});if(mutateKind=="grow")std::ofstream(path,std::ios::app)<<"x";else ::truncate(path.c_str(),0);}return result;}
extern "C" int __real_EVP_DigestUpdate(EVP_MD_CTX*,const void*,size_t);
extern "C" int __wrap_EVP_DigestUpdate(EVP_MD_CTX*c,const void*p,size_t n){return evpFail?0:__real_EVP_DigestUpdate(c,p,n);}
static Root root(std::istream&in){Root r;in>>std::quoted(r.path)>>r.mode>>r.uid>>r.gid;return r;}
static Entry entry(std::istream&in){Entry e;in>>std::quoted(e.path)>>e.directory>>e.mode>>e.uid>>e.gid>>e.size>>std::quoted(e.sha256);return e;}
static std::string secondReverifyMutation;
extern "C" ssize_t __real_pread(int,void*,size_t,off_t);
extern "C" ssize_t __wrap_pread(int fd,void*p,size_t n,off_t offset){auto result=__real_pread(fd,p,n,offset);if(result>0&&!secondReverifyMutation.empty()){auto path=std::exchange(secondReverifyMutation,{});std::ofstream(path)<<"wrong runtime";}return result;}
static int countFds(){DIR*d=::opendir("/proc/self/fd");check(d,"fd count unavailable");int n=0;while(::readdir(d))++n;::closedir(d);return n;}
int main(int argc,char**argv){try{check(argc==3,"arguments");std::ifstream in(argv[1]);ExpectedIdentity e;Caps c{};in>>std::quoted(e.arm);e.bundle=root(in);in>>std::quoted(e.module)>>c.entries>>c.files>>c.bytes>>c.pathBytes;std::size_t n=0;in>>n;check(n<10000,"fixture entries");for(std::size_t i=0;i<n;++i)e.bundleEntries.push_back(entry(in));in>>n;check(n<10000,"fixture roots");for(std::size_t i=0;i<n;++i)e.inputRoots.push_back(root(in));in>>n;check(n<10000,"fixture inputs");for(std::size_t k=0;k<n;++k){Input i;in>>std::quoted(i.name)>>i.root;i.file=entry(in);e.inputs.push_back(i);}check(bool(in),"fixture parsing");std::string action=argv[2];
    auto verified=VerifiedIdentity::verify(e,c);const int before=countFds();
    if(action=="stamp") {auto&s=verified->moduleStamp();std::cout<<s.device<<" "<<s.inode<<" "<<s.size<<" "<<s.mode<<" "<<s.uid<<" "<<s.gid<<" "<<s.sha256<<"\n";return 0;}
    auto path=verified->modulePath();
    if(action=="replace") {std::ifstream source(path,std::ios::binary);std::string bytes((std::istreambuf_iterator<char>(source)),{});::rename(path.c_str(),(path+".old").c_str());std::ofstream(path,std::ios::binary)<<bytes;::unlink((path+".old").c_str());}
    if(action=="symlink") {::rename(path.c_str(),(path+".old").c_str());::symlink((path+".old").c_str(),path.c_str());}
    if(action=="intermediate") {auto dir=e.bundle.path+"/data";::rename(dir.c_str(),(dir+".old").c_str());::symlink((dir+".old").c_str(),dir.c_str());}
    if(action=="different")std::ofstream(path,std::ios::binary)<<std::string(verified->moduleStamp().size,'x');
    if(action=="second-fail")secondReverifyMutation=e.inputRoots[0].path+"/"+e.inputs[0].file.path;
    if(action=="pin-evp")evpFail=true;
    try{
        auto pinned=PinnedModule::pin(verified);
        if(action=="lifetime")verified.reset();
        if(action=="held-change")std::ofstream(path,std::ios::binary)<<std::string(pinned->stamp().size,'z');
        if(action=="held-mode")::chmod(path.c_str(),0600);
        if(action=="held-grow")std::ofstream(path,std::ios::app)<<"x";
        if(action=="held-truncate")::truncate(path.c_str(),0);
        if(action=="held-evp")evpFail=true;
        if(action=="held-writable"){detail::Fd writable(::open(path.c_str(),O_RDWR));check(::dup2(writable.n,pinned->borrowedFd())>=0,"fixture dup failed");}
        if(action=="held-unlink")::unlink(path.c_str());
        if(action=="offset"){check(::lseek(pinned->borrowedFd(),3,SEEK_SET)==3,"seek failed");pinned->recheckHeld();check(::lseek(pinned->borrowedFd(),0,SEEK_CUR)==3,"recheck changed borrowed offset");}
        pinned->recheckHeld();check(pinned->identity()->arm()=="stock","identity lifetime lost");
        pinned.reset();check(countFds()==before,"success descriptor leak");return 0;
    }catch(const std::exception&){check(countFds()==before,"failure descriptor leak");throw;}
}catch(const std::exception&e){std::cerr<<e.what()<<"\n";return 1;}}
