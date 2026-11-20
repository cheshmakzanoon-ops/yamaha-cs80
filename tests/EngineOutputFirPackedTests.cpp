// Compare each Engine FIR output and SSE sticky flags with the unchanged raw
// scalar decimator through ring wraps, resets, and unusual IEEE inputs.
#include "core/EngineOutputFir.h"
#include <bit>
#include <cstdint>
#include <cstdio>
#include <xmmintrin.h>
namespace {
uint32_t rng=0x71234afd;
uint32_t next(){ rng ^= rng<<13; rng^=rng>>17; rng^=rng<<5; return rng; }
float input(int history,int i){
    auto x=next(), mag=x & 0x007fffff;
    switch(history){
        case 0: return std::bit_cast<float>(x & 0x80000000u);
        case 1: return (i % 520) < 275 ? 0.f : std::bit_cast<float>((x & 0x807fffffU) | ((1+(next()%254))<<23));
        case 2: return (i % 271) < 248 ? std::bit_cast<float>(x & 0x80000000u) : std::bit_cast<float>((x & 0x807fffffU) | 0x3e000000u);
        case 3: return (i % 275) == 13 ? std::bit_cast<float>(mag | (x & 0x80000000u)) : std::bit_cast<float>((x & 0x807fffffU) | 0x09000000u);
        case 4: return (i % 300) == 79 ? std::bit_cast<float>((x & 0x80000000u)|0x7f800054u) : std::bit_cast<float>((x & 0x807fffffU) | 0x3e000000u);
        default: return std::bit_cast<float>((x & 0x807fffffU) | ((1+(next()%254))<<23));
    }
}
template<class T> unsigned flags(T& f, unsigned csr, float& t){
    _mm_setcsr(csr);
    t=f.output();
    return _mm_getcsr()&63;
}
}
int main(){
    const auto previous=_mm_getcsr();
    uint64_t checked=0;
    for(int setupRound=0;setupRound!=4;++setupRound)
    for(int factor:{2,4,8})
    for(int mode=0;mode!=4;++mode)
    for(int denorm=0;denorm!=4;++denorm)
    for(int hist=0;hist!=6;++hist){
        cs80::EngineOutputFir trial;
        cs80::OversamplingDecimator oracle;
        _mm_setcsr(0x1f80|setupRound<<13);
        trial.prepare(factor);
        oracle.prepare(factor);
        const unsigned csr=0x1f80 | (mode<<13) | ((denorm&1)<<6) | ((denorm&2)<<14);
        for(int i=0;i<1160;++i){
            if(i==0||i==627){trial.reset();oracle.reset();}
            float x=input(hist,i);
            _mm_setcsr(csr);
            trial.push(x);
            if ((_mm_getcsr()&63)!=0) return 2;
            oracle.push(x);
            if(i%factor!=factor-1)continue;
            float a,b;
            auto fa=flags(oracle,csr,a), fb=flags(trial,csr,b);
            if(std::bit_cast<uint32_t>(a)!=std::bit_cast<uint32_t>(b)||fa!=fb){
                std::fprintf(stderr,"%d %d %d %d %d %d %x %x %x %x\n",setupRound,factor,mode,denorm,hist,i,std::bit_cast<uint32_t>(a),std::bit_cast<uint32_t>(b),fa,fb);
                return 1;
            }
            ++checked;
        }
    }
    _mm_setcsr(previous);
    std::printf("Compared %llu PCM words and sticky flags\n",(unsigned long long)checked);
}
