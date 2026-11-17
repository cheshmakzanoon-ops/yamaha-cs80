#include "native_identity.hpp"
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
int main(int argc,char**argv){try{check(argc==3,"arguments");std::ifstream in(argv[1]);ExpectedIdentity e;Caps c{};in>>std::quoted(e.arm);e.bundle=root(in);in>>std::quoted(e.module)>>c.entries>>c.files>>c.bytes>>c.pathBytes;std::size_t n=0;in>>n;check(n<10000,"fixture entries");for(std::size_t i=0;i<n;++i)e.bundleEntries.push_back(entry(in));in>>n;check(n<10000,"fixture roots");for(std::size_t i=0;i<n;++i)e.inputRoots.push_back(root(in));in>>n;check(n<10000,"fixture inputs");for(std::size_t k=0;k<n;++k){Input i;in>>std::quoted(i.name)>>i.root;i.file=entry(in);e.inputs.push_back(i);}check(bool(in),"fixture parsing");std::string action=argv[2];
    if(action=="grow"||action=="truncate"){mutatePath=e.bundle.path+"/"+e.module;mutateKind=action;}
    evpFail=action=="evp-fail";
    auto verified=VerifiedIdentity::verify(e,c);
    if(action=="caller-mutate"){e.arm="changed";e.bundleEntries.clear();check(verified->arm()=="stock","caller mutation leaked");}
    if(action=="reverify-content")std::ofstream(verified->modulePath())<<"changed";
    if(action=="reverify-mode")::chmod(verified->modulePath().c_str(),0600);
    if(action=="reverify-extra")std::ofstream(e.bundle.path+"/extra")<<"extra";
    if(action=="reverify-replace"){auto path=verified->modulePath();std::ifstream source(path,std::ios::binary);std::string bytes((std::istreambuf_iterator<char>(source)),{});::rename(path.c_str(),(path+".old").c_str());std::ofstream(path,std::ios::binary)<<bytes;::unlink((path+".old").c_str());}
    auto fresh=verified->reverify();check(verified->sameEvidence(*fresh),"fresh mismatch");
    std::cout<<verified->expectedManifestDigest()<<"\n"<<verified->evidenceDigest()<<"\n";
    return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<"\n";return 1;}}
