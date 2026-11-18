#include <bits/stdc++.h>
#include <xmmintrin.h>
#define private public
#include "core/Engine.h"
#undef private
using E=cs80::Engine; namespace s=cs80::source_factory;
using Update=E::SharedReadoutUpdate;
struct Fp { unsigned csr; unsigned short cw,sw; int err; };
Fp status(){asm volatile("":::"memory");Fp x{};asm volatile("stmxcsr %0":"=m"(x.csr)::"memory");asm volatile("fnstcw %0":"=m"(x.cw));asm volatile("fnstsw %0":"=am"(x.sw));x.err=errno;return x;}
void setup(unsigned mode,unsigned seed,unsigned x87seed=0){unsigned short cw=static_cast<unsigned short>(0x37fu|(((mode>>13)&3u)<<10));asm volatile("fnclex\n\tfldcw %0"::"m"(cw):"memory");if(x87seed)std::feraiseexcept(x87seed);_mm_setcsr(mode|seed);errno=119;asm volatile("":::"memory");}
bool samefp(Fp a,Fp b){return a.csr==b.csr&&a.cw==b.cw&&a.sw==b.sw&&a.err==b.err;}
void add(std::vector<std::uint64_t>&b,double x){b.push_back(std::bit_cast<std::uint64_t>(x));}
template<class T,std::size_t N>void add(std::vector<std::uint64_t>&b,const std::array<T,N>&x){for(const auto&y:x)add(b,y);}
std::vector<std::uint64_t> bits(const s::SharedReadout&x){std::vector<std::uint64_t>b;add(b,x.q);add(b,x.qVolts);add(b,x.levelPinVolts);add(b,x.levelGain);add(b,x.qBusVolts);add(b,x.levelBusVolts);add(b,x.masterResVolts);b.push_back(x.saturatedVoices);b.push_back(x.masterResSaturated);return b;}
__attribute__((noinline))bool original(E&e,const s::BothRankDrives&d,s::SharedReadout&r){return s::evaluateShared(e.sourceBoundaries,e.global.resonance,d,r);}
__attribute__((noinline))Update candidate(E&e,const s::BothRankDrives&d){return e.updateSourceSharedReadout(d);}
int main(){const auto saved=status();std::uint64_t samples=0,hits=0,solves=0,invalids=0,clearMisses=0,controlMisses=0,safeZHits=0,policyFallback=0,edgeFallback=0;std::map<int,std::uint64_t>phaseHits;
for(unsigned rounding=0;rounding<4;++rounding)for(unsigned denorm=0;denorm<4;++denorm){
 unsigned mode=0x1f80u|(rounding<<13)|((denorm&1)?0x8000u:0u)|((denorm&2)?0x40u:0u);
 setup(0x1f80u,0);E e; e.global.renderPolicy=cs80::RenderPolicy::sourcePerformance;
 s::ToneControls t0,t1;if(!s::compileTone(0,0,t0)||!s::compileTone(1,0,t1))return 2;e.sourceBoundaries={t0.boundary,t1.boundary};e.global.resonance=.22f;
 s::BothRankDrives d{};for(unsigned r=0;r<2;r++)for(unsigned v=0;v<8;v++)d[r][v]={.01*double(v),.1*double(v),.02*double(r+v)};
 s::SharedReadout ref{};ref.masterResVolts=e.sourceShared.masterResVolts=123.;ref.saturatedVoices=e.sourceShared.saturatedVoices=123;
 for(int n=0;n<640;++n){
  const int phase=n/32,pos=n%32;
  unsigned currentMode=mode,seed=pos==0?0u:0x3fu;
  if(phase==1)for(unsigned r=0;r<2;r++)for(unsigned v=0;v<8;v++)d[r][v].filterEnvelopeZ=.02*double((n+r+v)%97)-.96;
  if(phase==2)seed=0; // a stable result is not permission to omit newly raised flags
  if(phase==3)currentMode=0x1f80u|((unsigned(n)%4)<<13)|((denorm&1)?0x8000u:0u)|((denorm&2)?0x40u:0u);
  if(phase==4)d[(n/8)%2][n%8].pressure=.01*double(n%97);
  if(phase==5)e.global.resonance=float(n%31)/31.f;
  if(phase==6){e.sourceBoundaries[0].level.volts=.1*double(n%31);e.sourceSharedMemo.valid=false;}
  if(phase==7&&pos==0){e.sourceShared={};ref={};e.sourceSharedMemo.valid=false;} // accepted same-frame clear
  if(phase==8)e.global.renderPolicy=cs80::RenderPolicy::isolation;
  if(phase==9)e.global.renderPolicy=cs80::RenderPolicy::legacy;
  if(phase==10)e.global.renderPolicy=cs80::RenderPolicy::sourcePerformance;
  if(phase>=11&&phase<=17){
    const double edge[]={3.0000000000000004,-3.0000000000000004,std::numeric_limits<double>::denorm_min(),-0.,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN(),std::bit_cast<double>(std::uint64_t(0x7ff0000000000001))};
    for(auto&r:d)for(auto&x:r)x.filterEnvelopeZ=.25;
    d[(n/8)%2][n%8].filterEnvelopeZ=edge[phase-11];
  }
  if(phase==18){for(auto&r:d)for(auto&x:r)x.filterEnvelopeZ=.25;d[(n/8)%2][n%8].pressure=pos%3==0?std::numeric_limits<double>::infinity():pos%3==1?-.25:1.25;}
  if(phase==19){for(auto&r:d)for(auto&x:r){x.filterEnvelopeZ=.25;x.velocity=.5;x.pressure=.2;}if(pos==0)e.sourceSharedMemo.valid=false;}
  if(phase==11&&pos==0)seed=0x3fu; // keep the preceding valid cache FP-eligible at the first unsafe Z
  const auto prior=bits(ref);
  const auto prefix=std::size_t(reinterpret_cast<const unsigned char*>(&e.sourceSharedMemo)-reinterpret_cast<const unsigned char*>(&e));
  std::vector<unsigned char> beforeState(prefix);std::memcpy(beforeState.data(),&e,prefix);
  setup(currentMode,seed,pos%7==0?FE_INVALID:0);bool good=original(e,d,ref);auto a=status();
  setup(currentMode,seed,pos%7==0?FE_INVALID:0);auto result=candidate(e,d);auto b=status();
  const auto outStart=std::size_t(reinterpret_cast<const unsigned char*>(&e.sourceShared)-reinterpret_cast<const unsigned char*>(&e));
  const auto *afterState=reinterpret_cast<const unsigned char*>(&e);
  for(std::size_t i=0;i<prefix;++i)if((i<outStart||i>=outStart+sizeof(e.sourceShared))&&beforeState[i]!=afterState[i]){std::fprintf(stderr,"ORIGINAL ENGINE STATE MODIFIED byte=%zu\n",i);return 4;}
  ++samples;hits+=result==Update::retained;solves+=result==Update::solved;invalids+=result==Update::failed;
  if(result==Update::retained)++phaseHits[phase];
  if(phase==2&&result!=Update::retained)++clearMisses;
  if(phase==3&&result!=Update::retained)++controlMisses;
  if(phase==1&&result==Update::retained)++safeZHits;if((phase==8||phase==9)&&result==Update::solved)++policyFallback;if(phase>=11&&phase<=17&&phase!=14&&result!=Update::retained)++edgeFallback;
  if(good!=(result!=Update::failed)||bits(ref)!=bits(e.sourceShared)||!samefp(a,b)||(!good&&bits(ref)!=prior)){
   std::fprintf(stderr,"FAIL round=%u denorm=%u n=%d phase=%d result=%d good=%d csr=%x/%x cw=%x/%x sw=%x/%x errno=%d/%d output=%d\n",rounding,denorm,n,phase,int(result),good,a.csr,b.csr,a.cw,b.cw,a.sw,b.sw,a.err,b.err,bits(ref)==bits(e.sourceShared));return 1;
  }
 }
}
_mm_setcsr(saved.csr);asm volatile("fnclex\n\tfldcw %0"::"m"(saved.cw):"memory");
if(!hits||!solves||!invalids||clearMisses!=512||controlMisses<496||safeZHits<496||policyFallback!=1024||edgeFallback!=3072){std::fprintf(stderr,"ADMISSION COUNTS FAILED\n");return 3;}
std::printf("PASS samples=%llu hits=%llu solves=%llu invalid=%llu clear_misses=%llu control_misses=%llu safe_z_hits=%llu policy_fallback=%llu exceptional_z_fallback=%llu\n",(unsigned long long)samples,(unsigned long long)hits,(unsigned long long)solves,(unsigned long long)invalids,(unsigned long long)clearMisses,(unsigned long long)controlMisses,(unsigned long long)safeZHits,(unsigned long long)policyFallback,(unsigned long long)edgeFallback);for(auto[p,n]:phaseHits)std::printf("phase=%d retained=%llu\n",p,(unsigned long long)n);
}
