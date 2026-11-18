#include <bits/stdc++.h>
#include <xmmintrin.h>
#include <cerrno>
unsigned long long audit_cache_hits=0;
#define private public
#include "core/Ig00156Filter.h"
#undef private
using S=cs80::Ig00156Stage;
std::FILE* file;std::FILE* indexFile;std::uint64_t eventNo=0;unsigned constructorNo=0;
void setup(unsigned csr,unsigned seed=0){asm volatile("fninit":::"memory");if(seed)asm volatile("fldz; fldz; fdivp; fstp %%st(0)":::"memory");_mm_setcsr(csr);errno=101+int(eventNo%13);asm volatile("":::"memory");}
__attribute__((noinline)) void set(S&s,float fc,float q,float rate,bool uw=false){s.set(fc,q,rate,uw);asm volatile(""::"m"(s):"memory");}
__attribute__((noinline)) S::Output process(S&s,float in){auto o=s.process(in);asm volatile(""::"m"(s):"memory");return o;}
void dump(const char*label,const S&s,S::Output o={}){const unsigned csr=_mm_getcsr();unsigned short sw,cw;asm volatile("fnstsw %0":"=am"(sw));asm volatile("fnstcw %0":"=m"(cw));int err=errno;std::vector<std::uint64_t> v;v.push_back(eventNo);
for(double x:{s.net.sourceOhms,s.net.groundOhms,s.net.inputFarads,s.net.feedbackOhms,s.net.feedbackFarads,s.net.integratorFarads,s.net.feedforwardOhms})v.push_back(std::bit_cast<std::uint64_t>(x));
for(auto m:std::array<double S::*,28>{&S::gs,&S::gg,&S::gf,&S::gTotal,&S::inputScale,&S::feedforwardHz,&S::kHalf,&S::kHalfCoupling,&S::gmPart,&S::ffPart,&S::qSiemensValue,&S::kiGs,&S::xFromFb,&S::xFromIn,&S::xFromU,&S::xFromBp,&S::inFromX,&S::invA11,&S::zIn,&S::zFb,&S::zBp,&S::zLp,&S::vIn,&S::vFb,&S::vBp,&S::vLp,&S::slope1,&S::slope2})v.push_back(std::bit_cast<std::uint64_t>(s.*m));
for(float x:{s.lastCutoff,s.lastQ,s.lastRate})v.push_back(std::bit_cast<std::uint32_t>(x));v.push_back(s.lastUnwarped);v.push_back(s.cutoffCoefficientsValid);for(float x:{o.lp,o.hp,o.bp})v.push_back(std::bit_cast<std::uint32_t>(x));v.push_back(csr);v.push_back(sw);v.push_back(cw);v.push_back(static_cast<unsigned>(err));if(v.size()!=48)std::abort();if(std::fwrite(v.data(),sizeof(v[0]),v.size(),file)!=v.size())std::abort();if(indexFile)std::fprintf(indexFile,"%llu,%u,%s\n",(unsigned long long)eventNo,constructorNo,label);++eventNo;}
void runSet(S&s,const char*label,float fc,float q,float sr,bool uw,unsigned csr){setup(csr,(eventNo%3)==0);set(s,fc,q,sr,uw);dump(label,s);}
void runProcess(S&s,const char*label,float in,unsigned csr){setup(csr,(eventNo%3)==0);auto o=process(s,in);dump(label,s,o);}
const std::array<cs80::Ig00156Network,3> nets{cs80::kIg00156LowPassNetwork,cs80::kIg00156HighPassNetwork,{91000.,39000.,.82e-6,1.1e6,220e-12,1.8e-9,5.1e6}};
void trajectories(){for(const auto&n:nets)for(float sr:{8000.f,96000.f,1536000.f,3000.f})for(unsigned rm:{0u,0x2000u,0x4000u,0x6000u})for(unsigned den:{0u,0x40u,0x8000u,0x8040u})for(unsigned sticky:{0u,0x20u,0x3fu}){++constructorNo;unsigned ctrl=0x1f80|rm|den,alt=ctrl^0x4000;float a=sr*2,b=sr*3;setup(ctrl|sticky);S s(n);dump("constructor",s);
runSet(s,"low full",1000,2,sr,false,ctrl|sticky);runProcess(s,"process prelude",.1f,ctrl|sticky);
runSet(s,"first saturated full",a,2,sr,false,ctrl|0x20);runSet(s,"saturated hit",b,2,sr,false,ctrl|0x20);
runSet(s,"same raw early return changed controls",b,2,sr,false,alt);runSet(s,"changed raw new controls",a,2,sr,false,alt|0x20);runSet(s,"new-controls hit",b,2,sr,false,alt|0x20);
runSet(s,"Q-only changed controls",b,3,sr,false,ctrl|0x20);runSet(s,"raw after Q-only",a,3,sr,false,ctrl|0x20);runSet(s,"hit after new full",b,3,sr,false,ctrl|sticky);
setup(ctrl|sticky);s.reset();dump("reset",s);runSet(s,"raw after reset",a,3,sr,false,ctrl|0x20);
setup(ctrl|sticky);S copy=s;dump("copy",copy);runSet(copy,"copied raw hit",b,3,sr,false,ctrl|0x20);setup(ctrl|sticky);s=copy;dump("assigned copy",s);
auto mem=s.getMemory();mem.halfIntegrators=std::bit_cast<double>(std::bit_cast<std::uint64_t>(mem.halfIntegrators)^1);setup(ctrl|sticky);s.restoreMemory(mem);dump("restore perturbed halfstep",s);runSet(s,"full after restore",a,3,sr,false,ctrl|0x20);runSet(s,"hit after restore full",b,3,sr,false,ctrl|0x20);
runSet(s,"clamped Q full",a,10,sr,false,ctrl|sticky);runSet(s,"same clamped Q hit",b,11,sr,false,ctrl|0x20);
runSet(s,"unwarped exit",a,9,sr,true,ctrl|0x20);runSet(s,"warped reentry",b,9,sr,false,ctrl|0x20);runSet(s,"post-reentry hit",a,9,sr,false,ctrl|0x20);
runProcess(s,"process following hit",-.2f,ctrl|sticky);auto z=s.getState();auto v=s.getNodes();setup(ctrl|sticky);s.setState(z,v);dump("setState",s);auto h=s.halfSteps();h.integrators*=.5;h.couplings*=2;setup(ctrl|sticky);s.reReference(h);dump("reReference",s);runSet(s,"after reference",b,9,sr,false,ctrl|0x20);
runSet(s,"cleared sticky hit attempt",a,9,sr,false,ctrl);runSet(s,"all flags seeded",b,9,sr,false,ctrl|0x3f);runProcess(s,"final process",.037f,ctrl|sticky);
}}
void exceptions(){const std::array<std::uint32_t,14> fbits{0u,0x80000000u,1u,0x80000001u,0x007fffffu,0x00800000u,0xbf800000u,0x7149f2cau,0x7f800000u,0xff800000u,0x7fc00001u,0xffc00001u,0x7f800001u,0xff800001u};const std::array<std::uint32_t,11> rbits{0u,0x80000000u,1u,0xbf800000u,0x45f9ffffu,0x45fa0000u,0x49bb8000u,0x49bb8001u,0x7f800000u,0x7fc00001u,0x7f800001u};
for(const auto&n:nets)for(unsigned rm:{0u,0x2000u,0x4000u,0x6000u})for(unsigned den:{0u,0x40u,0x8000u,0x8040u})for(unsigned sticky:{0u,0x20u,0x3fu})for(int field=0;field<3;++field){auto bits=field==2?std::vector<std::uint32_t>(rbits.begin(),rbits.end()):std::vector<std::uint32_t>(fbits.begin(),fbits.end());for(auto raw:bits){++constructorNo;unsigned csr=0x1f80|rm|den|sticky;setup(csr);S s(n);dump("exception constructor",s);runSet(s,"exception prelude",100000,2,96000,false,csr|0x20);float x=std::bit_cast<float>(raw),fc=field==0?x:100001.f,q=field==1?x:2.f,sr=field==2?x:96000.f;runSet(s,"exception changed",fc,q,sr,false,csr);runSet(s,"exception identical return",fc,q,sr,false,csr);runSet(s,"exception altered raw",field==0?100002.f:fc,q,sr,false,csr);runProcess(s,"exception process",.1f,csr);runSet(s,"exception recovery",100003,2,96000,false,csr|0x20);runSet(s,"post recovery hit",100004,2,96000,false,csr|0x20);}}
}
int main(int argc,char**argv){if(argc<2)return 2;file=std::fopen(argv[1],"wb");if(!file)return 3;if(argc>2)indexFile=std::fopen(argv[2],"w");auto old=_mm_getcsr();trajectories();exceptions();std::fclose(file);if(indexFile)std::fclose(indexFile);_mm_setcsr(old);std::fprintf(stderr,"events=%llu constructors=%u explicit_hit_witnesses=%llu sizeof=%zu\n",(unsigned long long)eventNo,constructorNo,audit_cache_hits,sizeof(S));}
