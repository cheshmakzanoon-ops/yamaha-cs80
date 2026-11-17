#pragma once
// Bounded, versioned request decoding. No module loads and no test hooks.
#include "native_observer_process_child.hpp"
namespace cs80_observer::request {
constexpr std::size_t maxBytes=8u*1024u*1024u;
struct Request {
 cs80_identity::ExpectedIdentity identity;cs80_identity::Caps caps{};
 std::array<char,16> cid{};packed_filter_workload::Case workload{};
 ParameterBindingPolicy binding=ParameterBindingPolicy::ExactNative;AudioInputTopology topology=AudioInputTopology::ZeroAudioInput;ControllerQueryPolicy query=ControllerQueryPolicy::StrictSdk;ControllerStatePolicy state{};std::uint64_t timeoutMs=0;
};
class Reader {
 const std::string& bytes_;std::size_t at_=0;
public:
 explicit Reader(const std::string& b):bytes_(b){require(b.size()<=maxBytes,"request byte cap");}
 std::uint64_t number(std::uint64_t cap=UINT64_MAX){require(bytes_.size()-at_>=8,"truncated request integer");std::uint64_t n=0;for(unsigned i=0;i<8;++i)n|=std::uint64_t(static_cast<unsigned char>(bytes_[at_++]))<<(8*i);require(n<=cap,"request integer cap");return n;}
 std::string text(std::size_t cap=4096){const auto n=number(cap);require(n<=bytes_.size()-at_,"truncated request text");auto s=bytes_.substr(at_,n);at_+=n;require(s.find('\0')==s.npos,"request NUL text");return s;}
 cs80_identity::Root root(){cs80_identity::Root r;r.path=text();r.mode=number(0777);r.uid=number(UINT32_MAX);r.gid=number(UINT32_MAX);return r;}
 cs80_identity::Entry entry(){cs80_identity::Entry e;e.path=text();e.directory=number(1);e.mode=number(0777);e.uid=number(UINT32_MAX);e.gid=number(UINT32_MAX);e.size=number(8ull*1024*1024*1024);e.sha256=text(64);return e;}
 void end(){require(at_==bytes_.size(),"trailing request bytes");}
};
inline Request parse(const std::string& bytes){
 Reader r(bytes);const auto schema=r.text(64);require(schema=="cs80-process-lifetime-request-v1" || schema=="cs80-process-lifetime-request-v2" || schema=="cs80-process-lifetime-request-v3" || schema=="cs80-process-lifetime-request-v4","request schema");require(r.text(32)=="process-lifetime","request lifetime profile");Request q;
 q.identity.arm=r.text(16);require(q.identity.arm=="stock"||q.identity.arm=="candidate","request arm");
 const auto hex=r.text(32);require(hex.size()==32 && hex.find_first_not_of("0123456789abcdef")==hex.npos && hex!=std::string(32,'0'),"request exact nonzero raw CID hex");
 const auto digit=[](char c){return c<='9'?c-'0':c-'a'+10;};for(std::size_t i=0;i<16;++i)q.cid[i]=static_cast<char>(digit(hex[i*2])*16+digit(hex[i*2+1]));
 q.workload.policy=static_cast<packed_filter_workload::Policy>(r.number(2));q.workload.rate=r.number(96000);q.workload.mode=static_cast<packed_filter_workload::Mode>(r.number(1));q.workload.voices=r.number(16);q.workload.block=r.number(1024);packed_filter_workload::validate(q.workload);
 const auto state=r.number(2);require(state==1||state==2,"request controller state policy");q.state=static_cast<ControllerStatePolicy>(state);
 if(schema!="cs80-process-lifetime-request-v1"){q.query=static_cast<ControllerQueryPolicy>(r.number(2));validateControllerQueryPolicy(q.query);}
 if(schema=="cs80-process-lifetime-request-v3" || schema=="cs80-process-lifetime-request-v4"){q.topology=static_cast<AudioInputTopology>(r.number(2));validateAudioInputTopology(q.topology);}
 if(schema=="cs80-process-lifetime-request-v4"){q.binding=static_cast<ParameterBindingPolicy>(r.number(2));validateParameterBindingPolicy(q.binding);}
 q.timeoutMs=r.number(600000);require(q.timeoutMs>0,"request worker deadline");
 q.caps={r.number(16384),r.number(16384),r.number(8ull*1024*1024*1024),r.number(4096)};
 q.identity.bundle=r.root();q.identity.module=r.text();
 auto count=r.number(16384);for(std::uint64_t i=0;i<count;++i)q.identity.bundleEntries.push_back(r.entry());
 count=r.number(1024);for(std::uint64_t i=0;i<count;++i)q.identity.inputRoots.push_back(r.root());
 count=r.number(16384);for(std::uint64_t i=0;i<count;++i){cs80_identity::Input input;input.name=r.text();input.root=r.number(1023);input.file=r.entry();q.identity.inputs.push_back(std::move(input));}
 r.end();cs80_identity::detail::validate(q.identity,q.caps);return q;
}
inline Request read(const std::string& path,const std::string& expectedHash){
 using namespace cs80_identity;require(expectedHash.size()==64&&expectedHash.find_first_not_of("0123456789abcdef")==expectedHash.npos,"request SHA256 argument");detail::absolute(path,{1,1,maxBytes,4096});
 const auto slash=path.rfind('/');require(slash>0,"request owned parent");detail::Fd dir(detail::openRoot(path.substr(0,slash)));
 detail::Fd fd(::openat(dir.n,path.substr(slash+1).c_str(),O_RDONLY|O_NOFOLLOW|O_CLOEXEC|O_NONBLOCK));const auto before=detail::statFd(fd.n);
 require(S_ISREG(before.st_mode)&&before.st_nlink==1&&before.st_uid==::geteuid()&&(before.st_mode&07777)==0600&&before.st_size>0&&static_cast<std::uint64_t>(before.st_size)<=maxBytes,"request regular owned private file");
 std::string bytes(static_cast<std::size_t>(before.st_size),'\0');std::size_t at=0;while(at<bytes.size()){const auto n=::read(fd.n,bytes.data()+at,bytes.size()-at);if(n<0&&errno==EINTR)continue;require(n>0,"request short read");at+=static_cast<std::size_t>(n);}char extra;require(::read(fd.n,&extra,1)==0,"request grew");require(detail::stable(before,detail::statFd(fd.n)),"request changed during read");require(detail::digest(bytes)==expectedHash,"request SHA256 mismatch");return parse(bytes);
}
}
