#pragma once
// Linux-only, preparation-only actual-byte identity. Never loads a module.
#include <openssl/evp.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <functional>
#include <limits>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace cs80_identity {
inline void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
struct Caps {std::uint64_t entries,files,bytes,pathBytes;};
struct Entry {
    std::string path; bool directory=false; std::uint32_t mode=0,uid=0,gid=0;
    std::uint64_t size=0; std::string sha256;
};
struct Root {std::string path;std::uint32_t mode=0,uid=0,gid=0;};
struct Input {std::string name;std::size_t root=0;Entry file;};
struct ExpectedIdentity {
    std::string arm; Root bundle;std::string module;
    std::vector<Entry> bundleEntries;std::vector<Root> inputRoots;std::vector<Input> inputs;
};
namespace detail {
struct Fd {int n=-1;explicit Fd(int value):n(value){check(n>=0,"open failed");}~Fd(){if(n>=0)::close(n);}Fd(const Fd&)=delete;Fd& operator=(const Fd&)=delete;};
inline std::uint64_t add(std::uint64_t a,std::uint64_t b,std::uint64_t cap){check(a<=cap&&b<=cap-a,"identity cap exceeded");return a+b;}
inline void u64(std::string& out,std::uint64_t n){for(int i=0;i<8;++i)out.push_back(static_cast<char>(n>>(8*i)));}
inline void text(std::string& out,const std::string& value){u64(out,value.size());out+=value;}
inline void rootEncode(std::string& out,const Root& r){text(out,r.path);u64(out,r.mode);u64(out,r.uid);u64(out,r.gid);}
inline void entryEncode(std::string& out,const Entry& e){text(out,e.path);u64(out,e.directory?1:0);u64(out,e.mode);u64(out,e.uid);u64(out,e.gid);u64(out,e.size);text(out,e.sha256);}
inline bool equal(const Entry&a,const Entry&b){std::string x,y;entryEncode(x,a);entryEncode(y,b);return x==y;}
struct Hash {
    EVP_MD_CTX* ctx=EVP_MD_CTX_new();
    Hash(){check(ctx!=nullptr,"EVP allocation failed");if(EVP_DigestInit_ex(ctx,EVP_sha256(),nullptr)!=1){EVP_MD_CTX_free(ctx);ctx=nullptr;throw std::runtime_error("EVP init failed");}}
    ~Hash(){EVP_MD_CTX_free(ctx);}Hash(const Hash&)=delete;
    void update(const void* p,std::size_t n){check(EVP_DigestUpdate(ctx,p,n)==1,"EVP update failed");}
    std::string finish(){unsigned char bytes[EVP_MAX_MD_SIZE];unsigned int n=0;check(EVP_DigestFinal_ex(ctx,bytes,&n)==1&&n==32,"EVP final failed");static const char hex[]="0123456789abcdef";std::string out;for(unsigned int i=0;i<n;++i){out+=hex[bytes[i]>>4];out+=hex[bytes[i]&15];}return out;}
};
inline std::string digest(const std::string&s){Hash h;h.update(s.data(),s.size());return h.finish();}
inline void relative(const std::string&s,const Caps& c,bool dot=false){
    check(!s.empty()&&s.size()<=c.pathBytes&&s.find('\0')==std::string::npos&&s.find('\\')==std::string::npos,"invalid relative path");
    if(dot&&s==".")return;
    check(s.front()!='/'&&s.back()!='/',"noncanonical relative path");std::size_t p=0;
    while(p<s.size()){auto q=s.find('/',p);if(q==std::string::npos)q=s.size();auto part=s.substr(p,q-p);check(!part.empty()&&part!="."&&part!=".."&&part.size()<=255,"invalid path component");p=q+1;}
}
inline void absolute(const std::string&s,const Caps&c){check(s.size()>1&&s.front()=='/'&&s.size()<=c.pathBytes,"invalid absolute root");relative(s.substr(1),c);}
inline int openRoot(const std::string& path){
    Fd fd(::open("/",O_RDONLY|O_DIRECTORY|O_CLOEXEC));std::size_t p=1;
    while(p<path.size()){auto q=path.find('/',p);if(q==std::string::npos)q=path.size();auto part=path.substr(p,q-p);int next=::openat(fd.n,part.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);check(next>=0,"root ancestor unavailable/symlink");::close(fd.n);fd.n=next;p=q+1;}
    int result=fd.n;fd.n=-1;return result;
}
inline struct stat statFd(int fd){struct stat s{};check(::fstat(fd,&s)==0,"fstat failed");return s;}
inline bool stable(const struct stat&a,const struct stat&b){return a.st_dev==b.st_dev&&a.st_ino==b.st_ino&&a.st_mode==b.st_mode&&a.st_uid==b.st_uid&&a.st_gid==b.st_gid&&a.st_nlink==b.st_nlink&&a.st_size==b.st_size&&a.st_mtim.tv_sec==b.st_mtim.tv_sec&&a.st_mtim.tv_nsec==b.st_mtim.tv_nsec&&a.st_ctim.tv_sec==b.st_ctim.tv_sec&&a.st_ctim.tv_nsec==b.st_ctim.tv_nsec;}
inline Entry entry(const std::string&p,const struct stat&s){check((s.st_mode&07000)==0,"special permission mode");Entry e;e.path=p;e.directory=S_ISDIR(s.st_mode);check(e.directory||S_ISREG(s.st_mode),"special node");e.mode=s.st_mode&0777;e.uid=s.st_uid;e.gid=s.st_gid;check(e.directory||s.st_nlink==1,"hardlinked file");if(!e.directory){check(s.st_size>=0,"negative size");e.size=static_cast<std::uint64_t>(s.st_size);}return e;}
inline void rootCheck(int fd,const Root&r){auto s=statFd(fd);check(S_ISDIR(s.st_mode)&&(s.st_mode&07777)==r.mode&&s.st_uid==r.uid&&s.st_gid==r.gid,"root ownership/mode mismatch");}
inline void observed(std::string& out,const std::string&label,const struct stat&s){text(out,label);u64(out,s.st_dev);u64(out,s.st_ino);}
inline void validateEntry(const Entry&e,const Caps&c){relative(e.path,c,true);check(e.mode<=0777,"invalid mode");if(e.directory)check(e.size==0&&e.sha256.empty(),"directory data");else{check(e.sha256.size()==64&&e.sha256.find_first_not_of("0123456789abcdef")==std::string::npos,"invalid sha256");}}
inline bool nested(const std::string&a,const std::string&b){return a==b||(b.size()>a.size()&&b.compare(0,a.size(),a)==0&&b[a.size()]=='/');}
inline std::string encode(const ExpectedIdentity&e,const Caps&c){
    std::string out;text(out,"cs80-native-expected-v1");text(out,e.arm);rootEncode(out,e.bundle);text(out,e.module);
    u64(out,c.entries);u64(out,c.files);u64(out,c.bytes);u64(out,c.pathBytes);
    u64(out,e.bundleEntries.size());for(const auto&x:e.bundleEntries)entryEncode(out,x);
    u64(out,e.inputRoots.size());for(const auto&r:e.inputRoots)rootEncode(out,r);
    u64(out,e.inputs.size());for(const auto&i:e.inputs){text(out,i.name);u64(out,i.root);entryEncode(out,i.file);}return out;
}
inline void validate(const ExpectedIdentity&e,const Caps&c){
    check(c.entries>0&&c.entries<=1000000&&c.files>0&&c.files<=c.entries&&c.bytes>0&&c.pathBytes>0&&c.pathBytes<=4096,"invalid caps");
    check(!e.arm.empty()&&e.arm.size()<=64&&e.arm.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-")==std::string::npos,"invalid arm");
    check(e.bundleEntries.size()<=c.entries&&e.inputs.size()<=c.entries&&e.inputRoots.size()<=c.entries,"expected count cap");
    absolute(e.bundle.path,c);relative(e.module,c);check(e.bundle.mode<=0777,"root mode");
    std::vector<std::string> roots{e.bundle.path};std::string prev;
    for(const auto&r:e.inputRoots){absolute(r.path,c);check(r.mode<=0777&&r.path>prev,"runtime roots order/mode");for(const auto&p:roots)check(!nested(p,r.path)&&!nested(r.path,p),"overlapping roots");roots.push_back(r.path);prev=r.path;}
    std::uint64_t count=0,files=0,bytes=0;std::set<std::string> dirs;bool module=false;prev.clear();
    for(const auto&x:e.bundleEntries){validateEntry(x,c);check(x.path=="."?count==0:x.path>prev,"duplicate/unsorted bundle entries");if(x.path!=".")prev=x.path;count=add(count,1,c.entries);
        if(x.path==".")check(x.directory&&x.mode==e.bundle.mode&&x.uid==e.bundle.uid&&x.gid==e.bundle.gid,"root entry mismatch");
        else {auto slash=x.path.rfind('/');auto parent=slash==std::string::npos?".":x.path.substr(0,slash);check(dirs.count(parent)==1,"missing directory parent");}
        if(x.directory)dirs.insert(x.path);else{files=add(files,1,c.files);bytes=add(bytes,x.size,c.bytes);}
        if(x.path==e.module){check(!x.directory,"module is not file");module=true;}
    }
    check(!e.bundleEntries.empty()&&e.bundleEntries.front().path=="."&&module,"missing root/module");
    prev.clear();std::set<std::pair<std::size_t,std::string>> paths;std::set<std::size_t> used;
    for(const auto&i:e.inputs){check(!i.name.empty()&&i.name.size()<=c.pathBytes&&i.name.find('\0')==std::string::npos&&i.name>prev,"input label/order");prev=i.name;check(i.root<e.inputRoots.size()&&!i.file.directory&&i.file.path!=".","input root/type");validateEntry(i.file,c);check(paths.emplace(i.root,i.file.path).second,"duplicate input path");used.insert(i.root);count=add(count,1,c.entries);files=add(files,1,c.files);bytes=add(bytes,i.file.size,c.bytes);}
    check(used.size()==e.inputRoots.size(),"unused input root");
}
struct Scan {
    const Caps& caps;std::uint64_t count=0,files=0,bytes=0;
    std::set<std::pair<std::uint64_t,std::uint64_t>> nodes;
    std::string inodes;
    void note(const std::string&label,const struct stat&s){check(nodes.emplace(s.st_dev,s.st_ino).second,"aliased node");observed(inodes,label,s);}
    Entry file(int parent,const std::string&name,const std::string&path,const std::string&label){
        Fd fd(::openat(parent,name.c_str(),O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC));auto before=statFd(fd.n);auto result=entry(path,before);check(!result.directory,"expected regular file");note(label,before);
        count=add(count,1,caps.entries);files=add(files,1,caps.files);bytes=add(bytes,result.size,caps.bytes);
        Hash hash;std::array<char,65536> buffer{};std::uint64_t readBytes=0;
        for(;;){auto n=::read(fd.n,buffer.data(),buffer.size());if(n<0&&errno==EINTR)continue;check(n>=0,"read failed");if(n==0)break;readBytes=add(readBytes,static_cast<std::uint64_t>(n),result.size);hash.update(buffer.data(),static_cast<std::size_t>(n));}
        check(readBytes==result.size&&stable(before,statFd(fd.n)),"file changed during read");result.sha256=hash.finish();return result;
    }
    void tree(int fd,const std::string&path,std::vector<Entry>&out){
        auto before=statFd(fd);auto value=entry(path,before);check(value.directory,"expected directory");count=add(count,1,caps.entries);note("bundle:"+path,before);out.push_back(value);
        int copied=::dup(fd);check(copied>=0,"dup failed");DIR* raw=::fdopendir(copied);if(!raw){::close(copied);throw std::runtime_error("fdopendir failed");}std::unique_ptr<DIR,int(*)(DIR*)> dir(raw,::closedir);
        std::vector<std::string> names;
        for(;;){errno=0;auto*d=::readdir(dir.get());if(!d){check(errno==0,"readdir failed");break;}std::string n=d->d_name;if(n=="."||n=="..")continue;check(names.size()<caps.entries-count,"unexpected entry cap");relative(n,caps);names.push_back(n);}
        std::sort(names.begin(),names.end());
        for(const auto&name:names){std::string child=path=="."?name:path+"/"+name;relative(child,caps);struct stat s{};check(::fstatat(fd,name.c_str(),&s,AT_SYMLINK_NOFOLLOW)==0,"child stat failed");
            if(S_ISDIR(s.st_mode)){Fd sub(::openat(fd,name.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC));check(stable(s,statFd(sub.n)),"directory replaced");tree(sub.n,child,out);}
            else out.push_back(file(fd,name,child,"bundle:"+child));
        }
        check(stable(before,statFd(fd)),"directory changed during scan");
    }
};
inline std::string scan(const ExpectedIdentity&e,const Caps&c){
    Scan s{c,0,0,0,{}, {}};Fd root(openRoot(e.bundle.path));rootCheck(root.n,e.bundle);std::vector<Entry> actual;s.tree(root.n,".",actual);
    std::sort(actual.begin(),actual.end(),[](const Entry&a,const Entry&b){return a.path=="."?b.path!=".":b.path=="."?false:a.path<b.path;});check(actual.size()==e.bundleEntries.size(),"bundle inventory mismatch");for(std::size_t i=0;i<actual.size();++i)check(equal(actual[i],e.bundleEntries[i]),"bundle bytes/metadata mismatch");
    for(std::size_t r=0;r<e.inputRoots.size();++r){Fd fd(openRoot(e.inputRoots[r].path));rootCheck(fd.n,e.inputRoots[r]);s.note("root:"+e.inputRoots[r].path,statFd(fd.n));
        for(const auto&i:e.inputs)if(i.root==r){Fd parent(::dup(fd.n));std::size_t p=0;for(;;){auto q=i.file.path.find('/',p);if(q==std::string::npos)break;auto part=i.file.path.substr(p,q-p);int next=::openat(parent.n,part.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);check(next>=0,"input ancestor symlink/missing");::close(parent.n);parent.n=next;p=q+1;}
            auto actualFile=s.file(parent.n,i.file.path.substr(p),i.file.path,"input:"+i.name);check(equal(actualFile,i.file),"input bytes/metadata mismatch");}
    }
    return s.inodes;
}
} // detail
class VerifiedIdentity final {
    const ExpectedIdentity expected_;const Caps caps_;const std::string expectedBytes_,evidenceBytes_,expectedDigest_,evidenceDigest_;
    VerifiedIdentity(ExpectedIdentity e,Caps c,std::string inodeEvidence):expected_(std::move(e)),caps_(c),expectedBytes_(detail::encode(expected_,caps_)),evidenceBytes_(makeEvidence(expectedBytes_,inodeEvidence)),expectedDigest_(detail::digest(expectedBytes_)),evidenceDigest_(detail::digest(evidenceBytes_)){}
    static std::string makeEvidence(const std::string&e,const std::string&i){std::string out;detail::text(out,"cs80-native-verified-v1");detail::text(out,e);detail::text(out,i);return out;}
public:
    VerifiedIdentity(const VerifiedIdentity&)=delete;VerifiedIdentity&operator=(const VerifiedIdentity&)=delete;
    static std::shared_ptr<const VerifiedIdentity> verify(const ExpectedIdentity& expected,Caps caps){detail::validate(expected,caps);ExpectedIdentity snapshot=expected;auto observed=detail::scan(snapshot,caps);return std::shared_ptr<const VerifiedIdentity>(new VerifiedIdentity(std::move(snapshot),caps,std::move(observed)));}
    std::shared_ptr<const VerifiedIdentity> reverify()const{auto fresh=verify(expected_,caps_);check(fresh->evidenceBytes_==evidenceBytes_,"verified inode identity changed");return fresh;}
    const std::string& arm()const{return expected_.arm;}
    std::string modulePath()const{return expected_.bundle.path+"/"+expected_.module;}
    const std::string& expectedManifestDigest()const{return expectedDigest_;}
    const std::string& evidenceDigest()const{return evidenceDigest_;}
    const std::string& expectedEncoding()const{return expectedBytes_;}
    const std::string& evidenceEncoding()const{return evidenceBytes_;}
    bool sameEvidence(const VerifiedIdentity& other)const{return evidenceBytes_==other.evidenceBytes_;}
};
} // cs80_identity
