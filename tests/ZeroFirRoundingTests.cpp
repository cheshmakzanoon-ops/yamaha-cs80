// Prospective extension 156d82e9f3ee20089bb952ba853e0f0c779b903f.
// Software floating-point continuation; no hardware measurement or new FIR law.
#include "core/Dsp.h"
#include <array>
#include <bit>
#include <cfenv>
#include <cmath>
#include <cstdint>
#include <cstdio>
#if defined(__SSE__) || defined(_M_X64) || defined(_M_IX86)
#include <xmmintrin.h>
#define CS80_FIR_TEST_MXCSR 1
#else
#define CS80_FIR_TEST_MXCSR 0
#endif
#if defined(_MSC_VER)
#define CS80_FIR_NOINLINE __declspec(noinline)
#elif defined(__GNUC__) || defined(__clang__)
#define CS80_FIR_NOINLINE __attribute__((noinline))
#else
#define CS80_FIR_NOINLINE
#endif
#if defined(__clang__)
#pragma STDC FENV_ACCESS ON
#elif defined(_MSC_VER)
#pragma fenv_access(on)
#endif

namespace {
using Fir=cs80::OversamplingDecimator;
using History=std::array<float,Fir::kMaxTaps>;
int checks=0,failures=0,independentMxcsrCases=0;
std::uint64_t downwardWitnesses=0;
void check(bool good,const char* label) {
    ++checks;
    if(!good){if(failures<24)std::fprintf(stderr,"FAIL %s\n",label);++failures;}
}
bool same(float a,float b) {
    return std::bit_cast<std::uint32_t>(a)==std::bit_cast<std::uint32_t>(b);
}
struct Environment {
    std::fenv_t standard{};
    int direction=0,flags=0;
#if CS80_FIR_TEST_MXCSR
    unsigned mxcsr=0;
#endif
    Environment() {
        std::fegetenv(&standard);
        direction=std::fegetround();flags=std::fetestexcept(FE_ALL_EXCEPT);
#if CS80_FIR_TEST_MXCSR
        mxcsr=_mm_getcsr();
#endif
    }
    void restore() const {
        std::fesetenv(&standard);
#if CS80_FIR_TEST_MXCSR
        _mm_setcsr(mxcsr);
#endif
    }
};
struct RestoreEnvironment {
    Environment saved;
    ~RestoreEnvironment(){saved.restore();}
};
CS80_FIR_NOINLINE float orderedOutput(const Fir& filter,const History& history) {
    // Original center-first summation, independently indexed chronological data.
    const int center=filter.getCenter(),taps=filter.getTaps();
    double y=double(filter.getCoefficient(center))*history[std::size_t(center)];
    for(int i=0;i<center;++i)
        y+=double(filter.getCoefficient(i))*(double(history[std::size_t(i)])
             +double(history[std::size_t(taps-1-i)]));
    return float(y);
}
CS80_FIR_NOINLINE float actualOutput(const Fir& filter){return filter.output();}
CS80_FIR_NOINLINE bool actualDownwardZero() {
    volatile double positive=0.0,negative=-0.0;
    volatile double sum=positive+negative;
    return std::signbit(sum);
}
float observe(const Fir& filter,const History& history,const char* label) {
    const Environment before;
    const float expected=orderedOutput(filter,history);
    const Environment afterOracle;
    before.restore();
    const float actual=actualOutput(filter);
    const Environment after;
    check(same(actual,expected),label);
    check(after.direction==before.direction,"FIR output preserves standard rounding control");
    check((after.flags&before.flags)==before.flags,"FIR output preserves existing standard sticky flags");
    check(after.flags==afterOracle.flags,"FIR output matches original exception flags");
#if CS80_FIR_TEST_MXCSR
    check((after.mxcsr&~0x3fu)==(before.mxcsr&~0x3fu),"FIR output preserves all MXCSR controls");
    check((after.mxcsr&before.mxcsr&0x3fu)==(before.mxcsr&0x3fu),"FIR output preserves existing MXCSR sticky flags");
    check((after.mxcsr&0x3fu)==(afterOracle.mxcsr&0x3fu),"FIR output matches original MXCSR exception flags");
#endif
    return expected;
}
void push(Fir& filter,History& history,float value) {
    for(int k=filter.getTaps()-1;k>0;--k)history[std::size_t(k)]=history[std::size_t(k-1)];
    history[0]=value;filter.push(value);
}
History zeros(const Fir& filter,int pattern) {
    History result{};
    for(int k=0;k<filter.getTaps();++k) {
        if(pattern==1)result[std::size_t(k)]=-0.0f;
        else if(pattern==2)result[std::size_t(k)]=std::signbit(filter.getCoefficient(k))?+0.0f:-0.0f;
        else if(pattern==3)result[std::size_t(k)]=(k%3)?-0.0f:+0.0f;
    }
    if(pattern!=2)result[std::size_t(filter.getCenter())]=+0.0f;
    return result;
}
void load(Fir& filter,const History& history,int position) {
    filter.reset();
    for(int k=0;k<position;++k)filter.push(+0.0f);
    for(int k=filter.getTaps()-1;k>=0;--k)filter.push(history[std::size_t(k)]);
}
void continuation(Fir filter,History history) {
    auto copy=filter;Fir assigned;assigned=filter;
    observe(copy,history,"rounding-mode FIR copy preserves exact zero history");
    observe(assigned,history,"rounding-mode FIR assignment preserves exact zero history");
    const int length=filter.getTaps()+17;
    for(int n=0;n<length;++n) {
        const float value=n==0?0.125f:n==1?std::bit_cast<float>(std::uint32_t(1))
            :n==2?std::bit_cast<float>(std::uint32_t(0x80000001u))
            :n==3?std::bit_cast<float>(std::uint32_t(0x007fffffu))
            :(n%2?-0.0f:+0.0f);
        push(filter,history,value);copy.push(value);assigned.push(value);
        observe(filter,history,"rounding-mode FIR nonzero and subnormal continuation");
        observe(copy,history,"rounding-mode FIR copied continuation");
        observe(assigned,history,"rounding-mode FIR assigned continuation");
    }
    filter.reset();history.fill(+0.0f);
    observe(filter,history,"rounding-mode FIR reset uses original ordered zero sign");
    for(int next:{2,4,8,0,-1,3}) {
        assigned=copy;assigned.prepare(next);
        check(assigned.getFactor()==(Fir::validFactor(next)?next:2),"rounding-mode FIR reprepare keeps factor contract");
        observe(assigned,history,"rounding-mode FIR reprepare uses original ordered zero sign");
    }
}
void zeroFixtures(bool downward,const char* outputLabel) {
    for(int factor:{0,2,4,8}) {
        Fir filter;if(factor!=0)filter.prepare(factor);
        bool negativeCoefficient=false;
        for(int k=0;k<filter.getTaps();++k)
            negativeCoefficient|=filter.getCoefficient(k)<0.0f;
        if(factor!=0)check(negativeCoefficient,"prepared FIR contains negative coefficient for downward witness");
        for(int pattern=0;pattern<4;++pattern) {
            const History history=zeros(filter,pattern);
            for(int position=0;position<filter.getTaps();++position) {
                load(filter,history,position);
                const float truth=observe(filter,history,outputLabel);
                if(downward&&((factor==0&&pattern==1)||(factor!=0&&pattern==0))) {
                    ++downwardWitnesses;
                    check(std::bit_cast<std::uint32_t>(truth)==0x80000000u,
                          "original downward FIR counterexample is negative zero");
                }
            }
            if(pattern==1)continuation(filter,history);
        }
    }
}
template<class Function> void denormalEnvironments(Function function) {
#if CS80_FIR_TEST_MXCSR
    const unsigned entry=_mm_getcsr();
    for(unsigned flags:std::array<unsigned,4>{0u,0x40u,0x8000u,0x8040u}) {
        _mm_setcsr((entry&~0x8040u)|flags);
        function();
    }
    _mm_setcsr(entry);
#else
    function();
#endif
}
void sameHistoryTransitions() {
    Fir filter;filter.prepare(2);const History history=zeros(filter,1);load(filter,history,0);
    for(int mode:std::array<int,8>{FE_TONEAREST,FE_DOWNWARD,FE_TONEAREST,FE_UPWARD,
                                 FE_TOWARDZERO,FE_DOWNWARD,FE_UPWARD,FE_TONEAREST}) {
        const int result=std::fesetround(mode);
        check(result==0,"same-history rounding direction is supported");
        if(result==0)observe(filter,history,"same-history FIR rounding transition is exact");
    }
}
}
int main() {
    const RestoreEnvironment restore;
    for(int mode:std::array<int,4>{FE_TONEAREST,FE_DOWNWARD,FE_UPWARD,FE_TOWARDZERO}) {
        const int result=std::fesetround(mode);
        check(result==0,"all four standard FIR rounding modes are supported");
        if(result!=0){std::fprintf(stderr,"UNAVAILABLE standard rounding %d\n",mode);continue;}
        check(actualDownwardZero()==(mode==FE_DOWNWARD),"rounding test compiler honors standard directed zero addition");
        denormalEnvironments([&]{zeroFixtures(mode==FE_DOWNWARD,
            "all-rounding FIR ordered convolution including positive-center signed zeros");});
    }
    std::fesetround(FE_TONEAREST);
    denormalEnvironments([]{sameHistoryTransitions();});
#if CS80_FIR_TEST_MXCSR
    for(unsigned bits:std::array<unsigned,4>{0u,0x2000u,0x4000u,0x6000u}) {
        std::fesetround(FE_TONEAREST);
        _mm_setcsr((_mm_getcsr()&~0x6000u)|bits);
        const int standard=std::fegetround();
        if(bits!=0&&standard==FE_TONEAREST)++independentMxcsrCases;
        std::printf("MXCSR-only direction 0x%04x: standard query %d, arithmetic negative zero %d\n",
                    bits,standard,int(actualDownwardZero()));
        const bool downward=actualDownwardZero();
#if defined(__x86_64__) || defined(_M_X64)
        check(downward==(bits==0x2000u),"x86-64 rounding driver observes actual SSE direction");
#endif
        denormalEnvironments([&]{zeroFixtures(downward,
            "MXCSR-only FIR ordered convolution including positive-center signed zeros");});
    }
#endif
    check(downwardWitnesses>0,"rounding gate exercises original negative-zero counterexample");
    std::printf("Zero FIR rounding: %d checks, %d failures, %llu downward witnesses, %d independent MXCSR directions\n",
                checks,failures,static_cast<unsigned long long>(downwardWitnesses),independentMxcsrCases);
    return failures?1:0;
}
