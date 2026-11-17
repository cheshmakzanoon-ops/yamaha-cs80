#include "core/PackedPanResearch.h"
#include <immintrin.h>
#include <array>
#include <cfenv>
#include <cerrno>
#include <cstdio>
#include <cstdint>
#include <limits>
__attribute__((noinline)) void base(float a,float b,std::array<float,4>&o){cs80::constantPowerPan(a,o[0],o[1]);cs80::constantPowerPan(b,o[2],o[3]);}
__attribute__((noinline)) bool cand(float a,float b,std::array<float,4>&o){return cs80::research::panPair(a,b,o[0],o[1],o[2],o[3]);}
unsigned short status(){unsigned short x;asm volatile("fnstsw %0":"=am"(x));return x;}
uint64_t rng=2347289;uint32_t next(){rng^=rng<<13;rng^=rng>>7;rng^=rng<<17;return uint32_t(rng);}
int main(){unsigned original=_mm_getcsr(),count=0,packed=0;std::array<uint32_t,16> fixed{0,0x80000000,0x3f800000,0xbf800000,0x3f7fffff,0xbf7fffff,0x3f800001,0xbf800001,1,0x80000001,0x7f800000,0xff800000,0x7fc00001,0x7f800001,0x7f7fffff,0xff7fffff};
for(int mode:{FE_TONEAREST,FE_DOWNWARD,FE_UPWARD,FE_TOWARDZERO})for(unsigned denorm:{0u,0x40u,0x8000u,0x8040u}){std::fesetround(mode);_mm_setcsr((_mm_getcsr()&~0x8040u)|denorm);
for(unsigned i=0;i<8192;++i){float a=std::bit_cast<float>(i<256?fixed[i%16]:next()),b=std::bit_cast<float>(i<256?fixed[i/16]:next());std::array<float,4>x{},y{};
std::feclearexcept(FE_ALL_EXCEPT);std::feraiseexcept(i%3==0?FE_INVALID:(i%3==1?FE_INEXACT|FE_UNDERFLOW:0));errno=17;base(a,b,x);unsigned xm=_mm_getcsr();auto xf=status();int xe=errno;
std::feclearexcept(FE_ALL_EXCEPT);std::feraiseexcept(i%3==0?FE_INVALID:(i%3==1?FE_INEXACT|FE_UNDERFLOW:0));errno=17;bool used=cand(a,b,y);unsigned ym=_mm_getcsr();auto yf=status();int ye=errno;
if(std::bit_cast<std::array<uint32_t,4>>(x)!=std::bit_cast<std::array<uint32_t,4>>(y)||xm!=ym||xf!=yf||xe!=ye){std::printf("FAIL %u round%d denorm%u packed%d flags%u/%u\n",i,mode,denorm,used,xm,ym);return 1;}++count;packed+=used;
}}
std::fesetround(FE_TONEAREST);_mm_setcsr(original);
#if CS80_IG00156_EXACT_BATCH_BUILD
if(packed==0)return 4;
#else
if(packed!=0)return 5;
#endif
std::printf("PASS %u pairs, packed%u scalar%u; bits and FP/errno equal\n",count,packed,count-packed);}
