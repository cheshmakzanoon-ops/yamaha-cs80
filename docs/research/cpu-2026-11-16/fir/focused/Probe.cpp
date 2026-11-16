#ifdef CANDIDATE
#include "StereoFir.h"
#else
#include "BaselineDsp.h"
#endif
#include <xmmintrin.h>
#include <cfenv>
#include <bit>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <iostream>
#include <memory>

static unsigned long calls=0, accelerated=0;
static std::FILE* output;
static void emit(std::uint32_t x) { if(std::fwrite(&x,4,1,output)!=1) throw std::runtime_error("write failure"); }
static __attribute__((noinline,noclone)) void run(const cs80::OversamplingDecimator& a,const cs80::OversamplingDecimator& b,unsigned mode) {
    std::array<unsigned char,sizeof(a)> beforeA,beforeB;
    std::memcpy(beforeA.data(),&a,sizeof(a));std::memcpy(beforeB.data(),&b,sizeof(b));
    std::feclearexcept(FE_ALL_EXCEPT);_mm_setcsr(mode);
#ifdef CANDIDATE
    auto r=cs80::stereoFirOutput(a,b);auto v=r.samples;accelerated+=r.accelerated;
#else
    std::array<float,2> v{a.output(),b.output()};
#endif
    unsigned after=_mm_getcsr();unsigned short cw,sw;
    asm volatile("fnstcw %0":"=m"(cw));asm volatile("fnstsw %0":"=m"(sw));
    if((after&~0x3fu)!=(mode&~0x3fu)) throw std::runtime_error("control mode changed");
    if(std::memcmp(beforeA.data(),&a,sizeof(a))||std::memcmp(beforeB.data(),&b,sizeof(b)))
        throw std::runtime_error("input state mutated");
    emit(std::bit_cast<std::uint32_t>(v[0]));emit(std::bit_cast<std::uint32_t>(v[1]));
    emit(after);emit(cw);emit(sw);++calls;
}
int main(int argc,char**argv) try {
    if(argc!=2)throw std::runtime_error("output path required");
    output=std::fopen(argv[1],"wb");if(!output)throw std::runtime_error("open failed");
    const unsigned saved=_mm_getcsr();const int savedRound=std::fegetround();
    constexpr std::array<int,4> rounds{FE_TONEAREST,FE_DOWNWARD,FE_UPWARD,FE_TOWARDZERO};
    constexpr std::array<std::array<std::uint32_t,8>,4> patterns{{
      {0x3f800000,0xbf800000,0x3eaaaaab,0xbeaaaaab,0x40000000,0xc0400000,0x3dcccccd,0xbdcccccd},
      {0,0x80000000,1,0x80000001,0x007fffff,0x807fffff,0x00800000,0x80800000},
      {0x7f7fffff,0xff7fffff,0x7f800000,0xff800000,0x3f800000,0xbf800000,0,0x80000000},
      {0x7fc01234,0xffc05678,0x7f801234,0xff805678,0,1,0x3f800000,0xbf800000}}};
    for(int rounding=0;rounding<4;++rounding)for(unsigned denorm: {0u,0x40u,0x8000u,0x8040u})
    for(int factor:{2,4,8})for(unsigned pattern=0;pattern<4;++pattern) {
        cs80::OversamplingDecimator a,b;
        std::fesetround(rounds[(rounding+1)%4]);_mm_setcsr((_mm_getcsr()&~0x8040u)|denorm|0x1f80u);a.prepare(factor);
        std::fesetround(rounds[(rounding+2)%4]);_mm_setcsr((_mm_getcsr()&~0x8040u)|denorm|0x1f80u);b.prepare(factor);
        for(int i=0;i<a.getTaps();++i) {a.push(std::bit_cast<float>(patterns[pattern][i%8]));b.push(std::bit_cast<float>(patterns[pattern][(i*3+1)%8]));}
        for(int j=0;j<17;++j)b.push(std::bit_cast<float>(patterns[pattern][(j+2)%8]));
        std::fesetround(rounds[rounding]);const unsigned control=(_mm_getcsr()&~0x807fu)|denorm|0x1f80u;
        for(int cursor=0;cursor<a.getTaps();++cursor) {
            run(a,b,control|((cursor%2)?0x3fu:0u));
            a.push(std::bit_cast<float>(patterns[pattern][cursor%8]));
            b.push(std::bit_cast<float>(patterns[pattern][(cursor*5+3)%8]));
        }
        auto copy=a;run(copy,b,control);run(a,a,control);
        a.reset();b.reset();run(a,b,control);
        a.prepare(factor==2?4:2);run(a,b,control); // shape mismatch fallback
        b.prepare(factor==2?4:2);run(a,b,control); // reprepare same shape
    }
    std::fesetround(FE_TONEAREST);
    cs80::OversamplingDecimator zeroA,zeroB;
    for(unsigned mask=0;mask<64;++mask)run(zeroA,zeroB,0x1f80u&~(mask<<7));
    std::fesetround(savedRound);_mm_setcsr(saved);
    if(std::fclose(output))throw std::runtime_error("close failed");
    std::cerr<<"PASS calls="<<calls<<" accelerated="<<accelerated<<'\n';return 0;
} catch(const std::exception& ex){std::cerr<<"FAIL "<<ex.what()<<'\n';return 1;}
