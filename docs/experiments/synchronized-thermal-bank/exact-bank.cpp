#include "core/ToleranceModel.h"
#include <array>
#include <bit>
#include <cerrno>
#include <cfenv>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <xmmintrin.h>
namespace tc=cs80::tolerance;
constexpr unsigned n=16;
struct Pack {std::array<tc::ThermalDrift,n> d{};std::array<float,n> f{};};
void reset(Pack&a, int sr){for(unsigned i=0;i<n;i++){a.d[i].prepare(sr,int(i));a.f[i]=0;}}
struct Obs{unsigned csr,x87;int err;};
Obs finish(){asm volatile("":::"memory");unsigned short sw;asm volatile("fnstsw %0":"=am"(sw));return {_mm_getcsr(),unsigned(sw)&63u,errno};}
void setup(unsigned mode,unsigned ft,unsigned flags){_mm_setcsr(0x1f80u|mode<<13|(ft&1u?0x8000u:0u)|(ft&2u?0x40u:0u)|flags);asm volatile("fnclex":::"memory");errno=119;asm volatile("":::"memory");}
__attribute__((noinline)) Obs orig(Pack&a){for(unsigned i=0;i<n;i++)a.f[i]=a.d[i].tick();asm volatile(""::"m"(a):"memory");return finish();}
__attribute__((noinline)) Obs cand(Pack&a,tc::SharedThermalClock& clock,unsigned long long& visits){clock.tick([&](auto update){++visits;for(unsigned i=0;i<n;i++)update(a.d[i],a.f[i]);});asm volatile(""::"m"(a):"memory");return finish();}
int main(){auto saved=_mm_getcsr();Pack a,b;tc::SharedThermalClock c;unsigned long long samples=0,visits=0,errors=0,copies=0,pauses=0;int rates[]={8000,44100,48000,96000,192000,1536000,37};
 for(int batch=-1;batch<7;batch++){
  if(batch>=0){reset(a,rates[batch]);reset(b,rates[batch]);c.prepare(b.d[0].controlPeriodSamples());}
  for(int ix=0;ix<26000;ix++){
   if(ix%55==7||ix%179==17){++pauses;continue;}
   if(ix==809||ix==17013){Pack x=a,y=b;tc::SharedThermalClock z=c;a=x;b=y;c=z;++copies;}
   unsigned mode=unsigned((ix/241+batch+1)%4),ft=(ix/1079)%4,flags=(ix%11==0?0u:(ix%7==0?0x3fu:0x20u));
   setup(mode,ft,flags);auto l=orig(a); setup(mode,ft,flags);auto r=cand(b,c,visits);++samples;
   bool ok=l.csr==r.csr&&l.x87==r.x87&&l.err==r.err;
   for(unsigned k=0;k<n;k++){ok&=std::bit_cast<std::uint32_t>(a.f[k])==std::bit_cast<std::uint32_t>(b.f[k]);if(ix%97==0){ok&=std::bit_cast<std::uint64_t>(a.d[k].heldCents())==std::bit_cast<std::uint64_t>(b.d[k].heldCents());ok&=std::bit_cast<std::uint64_t>(a.d[k].warmupComponent())==std::bit_cast<std::uint64_t>(b.d[k].warmupComponent());}}
   if(!ok){if(errors++<5)std::fprintf(stderr,"mismatch batch=%d sample=%d csr=%x/%x x87=%x/%x errno=%d/%d\n",batch,ix,l.csr,r.csr,l.x87,r.x87,l.err,r.err);}
  }
 }
 _mm_setcsr(saved);printf("samples=%llu rank_outputs=%llu visits=%llu skipped=%llu copies=%llu paused=%llu bad=%llu\n",samples,n*samples,visits,samples-visits,copies,pauses,errors);return errors||!(visits>0&&visits<samples)?1:0;}
