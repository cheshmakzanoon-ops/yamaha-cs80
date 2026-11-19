// Test only the compiler/ISA/FP specialization that the product can select.
// No private access, wall clock, host audio tolerance or hardware comparison.
#if defined(__OPTIMIZE__) && !defined(__OPTIMIZE_SIZE__) \
    && defined(__linux__) && defined(__x86_64__) && defined(__GNUC__) && defined(__SSE2_MATH__) \
    && !defined(__clang__) && !defined(__INTEL_COMPILER) && !defined(__INTEL_LLVM_COMPILER) \
    && !defined(__NVCOMPILER) && !defined(__PGI) && !defined(__ibmxl__) && !defined(_MSC_VER) \
    && !defined(__AVX__) && !defined(__FMA__) && !defined(__FMA4__) && !defined(__FP_FAST_FMA) \
    && !defined(__FP_FAST_FMAF) && !defined(__FAST_MATH__) && !defined(__ASSOCIATIVE_MATH__) \
    && !defined(__RECIPROCAL_MATH__) && !defined(__NO_SIGNED_ZEROS__) && !defined(__NO_TRAPPING_MATH__) \
    && !defined(__NO_MATH_ERRNO__) && !defined(__ROUNDING_MATH__) && !defined(__SUPPORT_SNAN__) \
    && (!defined(__FINITE_MATH_ONLY__) || __FINITE_MATH_ONLY__ == 0) \
    && defined(__FLT_EVAL_METHOD__) && __FLT_EVAL_METHOD__ == 0 && !defined(CS80_DISABLE_INLINE_RC_ENVELOPES) \
    && __GNUC__ == 14 && __GNUC_MINOR__ == 2 && !defined(CS80_DISABLE_STEREO_PRA) && !defined(CS80_DISABLE_AVX2_FILTER_BATCH)
#include "core/PraWahCircuit.h"
#include <cstdint>
#include <utility>
#include <cstring>
#include <type_traits>
#include <cstddef>
#include <bit>
#include <cstdio>
#include <cfenv>
#include <cerrno>
#include <emmintrin.h>
#define CS80_STEREO_PRA_BUILD 1
namespace cs80 {
#include "core/EngineStereoPra.cpp.inc"
}
using W=cs80::PraWahCircuit;using P=cs80::EngineStereoPra;
struct Fp{unsigned csr;unsigned short cw,sw;int er;};
Fp fp(){Fp x{_mm_getcsr(),0,0,errno};asm volatile("fnstcw %0;fnstsw %1":"=m"(x.cw),"=m"(x.sw)::"memory");return x;}
bool eq(Fp a,Fp b){return a.csr==b.csr&&a.cw==b.cw&&a.sw==b.sw&&a.er==b.er;}
static uint64_t rng=83727610331; uint64_t raw(){rng^=rng<<13;rng^=rng>>7;rng^=rng<<17;return rng;}
uint64_t bits(double d){return std::bit_cast<uint64_t>(d);}
bool eq(double a,double b){return bits(a)==bits(b);}
__attribute__((noinline,noclone)) W::Nodes clockOne(W& w,float in){return w.process(in);}
__attribute__((noinline,noclone)) P::Out clockPair(W& l,W&r,float x,float y){if (P::common(l,r)&&P::boundedCaps(l,r))return P::run(l,r,x,y);const auto a=clockOne(l,x);const auto b=clockOne(r,y);return {P::V{a.x,b.x},P::V{a.y,b.y}};}
void init(unsigned mx,int e=37){std::fesetenv(FE_DFL_ENV);_mm_setcsr(mx);errno=e;}
bool equal(const W&a,const W&b){return !std::memcmp(&a,&b,sizeof a);}
int main(){
 constexpr uint32_t fspecial[]{0,0x80000000,1,0x80000001,0x007fffff,0x807fffff,0x00800000,0x80800000,0x7f7fffff,0xff7fffff,0x7f800000,0xff800000,0x7fc00055,0xffc00077,0x7f800055,0xff800077};
 constexpr uint64_t special[]{0,0x8000000000000000ULL,1,0x8000000000000001ULL,0xfffffffffffffULL,0x800fffffffffffffULL,0x0010000000000000ULL,0x8010000000000000ULL,0x7fefffffffffffffULL,0xffefffffffffffffULL,0x7ff0000000000000ULL,0xfff0000000000000ULL,0x7ff8000000000055ULL,0xfff8000000000077ULL,0x7ff0000000000055ULL,0xfff0000000000077ULL};
 constexpr int modes=4,scenes=6;unsigned admitted[scenes]{}, tested[scenes]{},mismatched[scenes]{},fpDiff[scenes]{},stateDiff[scenes]{},outDiff[scenes]{};bool emitted[scenes]{};
 for(unsigned ft=0;ft<modes;ft++)for(double rate:{8000.,44100.,96000.,1536000.})for(int style=0;style<scenes;style++){
   init(0x1f80);W a,b,x,y;W::Profile p;
   if(!a.prepare(rate,p,.31)||!b.prepare(rate,p,.31))return 10;
   if(style==5){p.parallelOhms=220000.;if(!b.prepare(rate,p,.31))return 10;}
   x=a;y=b;
   for(int i=0;i<1024;i++){
     init(0x1f80);if((i%19)==0){double pedal=double(raw()%65536)/65535.;a.setPedal(pedal);b.setPedal(pedal);x.setPedal(pedal);y.setPedal(pedal);}
     float l=float(int(raw()%32001)-16000)/16000.f,r=float(int(raw()%32001)-16000)/16000.f;
     if(style>=1){l=std::bit_cast<float>(fspecial[i%16]);r=std::bit_cast<float>(fspecial[(i/16)%16]);}
     if(style==2 || style==3) { // Distinct capacitor energy states, supported/malformed at controlled coordinates.
       a=b;x=a;y=b;
       const auto val=[&](int n){uint64_t v=special[n%16]; if(style==2 && (v&0x7ff0000000000000ULL)==0x7ff0000000000000ULL)v=0;return std::bit_cast<double>(v);};
       const_cast<W::Capacitors&>(a.capacitorState())[(i/8)%4].voltage=const_cast<W::Capacitors&>(x.capacitorState())[(i/8)%4].voltage=val(i);
       const_cast<W::Capacitors&>(b.capacitorState())[(i/3)%4].current=const_cast<W::Capacitors&>(y.capacitorState())[(i/3)%4].current=val(i/16);
       // Additional distinct NaN payloads on the SAME arithmetic operation.
       if(style==3 && (i&1)){const_cast<W::Capacitors&>(a.capacitorState())[0].voltage=const_cast<W::Capacitors&>(x.capacitorState())[0].voltage=val(12+(i%4));const_cast<W::Capacitors&>(a.capacitorState())[0].current=const_cast<W::Capacitors&>(x.capacitorState())[0].current=val(12+((i+1)%4));}
     }

     if(style==4) { // Finite history spanning subnormals through the upper admitted exponent.
       for(auto side : {std::pair{&a,&x},std::pair{&b,&y}}) {
        auto& stock=const_cast<W::Capacitors&>(side.first->capacitorState());
        auto& cand=const_cast<W::Capacitors&>(side.second->capacitorState());
        auto value=[](){const auto rawWord=raw();return std::bit_cast<double>((rawWord&0x800fffffffffffffULL)|((raw()%1536)<<52));};
        for(std::size_t slot=0;slot<4;++slot) stock[slot]=cand[slot]=W::Capacitor{value(),value()};
       }
     }
     admitted[style]+= (P::common(x,y)&&P::boundedCaps(x,y))?1:0;
     const unsigned mx=0x1f80u |((ft&1)?0x0040u:0)|((ft&2)?0x8000u:0)|((i%4)==0?(raw()%64):0);
     init(mx);auto al=clockOne(a,l);auto ar=clockOne(b,r);auto af=fp();
     init(mx);auto got=clockPair(x,y,l,r);auto bf=fp();
     const bool of=eq(al.x,got.x[0])&&eq(al.y,got.y[0])&&eq(ar.x,got.x[1])&&eq(ar.y,got.y[1]);
     const bool st=equal(a,x)&&equal(b,y);const bool fe=eq(af,bf);
     tested[style]++; if(!of||!st||!fe){mismatched[style]++;outDiff[style]+=!of;stateDiff[style]+=!st;fpDiff[style]+=!fe;
       if(!emitted[style]){std::fprintf(stderr,"style %d ft=%u rate=%g i=%d of=%d st=%d fe=%d csr %#x/%#x a.x=%016lx/%016lx b.x=%016lx/%016lx\n",style,ft,rate,i,of,st,fe,af.csr,bf.csr,bits(al.x),bits(got.x[0]),bits(ar.x),bits(got.x[1]));emitted[style]=true;}
       x=a;y=b;
     }
   }
 }
 for(int i=0;i<scenes;i++)printf("style %d tested %u packed %u fallback %u mismatched %u out %u state %u fp %u\n",i,tested[i],admitted[i],tested[i]-admitted[i],mismatched[i],outDiff[i],stateDiff[i],fpDiff[i]);
 for(int i=0;i<scenes;i++)if(mismatched[i])return 1;
 if(admitted[0]!=tested[0]||admitted[1]!=tested[1]||admitted[4]!=tested[4]||admitted[5]!=0)return 2;
 return (admitted[2]<tested[2]&&admitted[3]<tested[3])?0:3;
}
#else
int main(){ return 77; }
#endif
