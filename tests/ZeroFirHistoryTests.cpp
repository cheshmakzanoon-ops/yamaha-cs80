#include "core/Dsp.h"
#include <array>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <cfenv>
#if defined(__SSE2__) || defined(_M_X64)
#include <xmmintrin.h>
#define CS80_TEST_MXCSR 1
#else
#define CS80_TEST_MXCSR 0
#endif

namespace
{
int checks=0, failures=0;
void check(bool good,const char* why)
{
    ++checks;
    if(!good){if(failures<12)std::fprintf(stderr,"FAIL %s\n",why);++failures;}
}
bool nonzero(float x)
{
    // The independent chronological history is classified by stored integer
    // bits, including on compilers that fold an unobserved bit test to FP.
    const volatile std::uint32_t bits=std::bit_cast<std::uint32_t>(x);
    return (bits&0x7fffffffu)!=0;
}
using History=std::array<float,cs80::OversamplingDecimator::kMaxTaps>;
float originalFirOutput(const cs80::OversamplingDecimator& filter,const History& history)
{
    const int taps=filter.getTaps(),middle=filter.getCenter();
    double truth=double(filter.getCoefficient(middle))*history[std::size_t(middle)];
    for(int k=0;k<middle;++k)
        truth+=double(filter.getCoefficient(k))*(double(history[std::size_t(k)])
                +double(history[std::size_t(taps-1-k)]));
    return float(truth);
}
void pushHistory(cs80::OversamplingDecimator& filter,History& history,float input)
{
    for(int k=filter.getTaps()-1;k>0;--k)history[std::size_t(k)]=history[std::size_t(k-1)];
    history[0]=input;
    filter.push(input);
}
bool sameFir(float a,float b)
{return std::bit_cast<std::uint32_t>(a)==std::bit_cast<std::uint32_t>(b);}

void signedZeroContinuation(cs80::OversamplingDecimator original,History history)
{
    auto copied=original;
    cs80::OversamplingDecimator assigned;
    assigned=original;
    check(sameFir(copied.output(),originalFirOutput(copied,history))
          &&copied.getNonzeroHistoryCount()==0,"signed-zero FIR copy retains ordered output");
    check(sameFir(assigned.output(),originalFirOutput(assigned,history))
          &&assigned.getNonzeroHistoryCount()==0,"signed-zero FIR assignment retains ordered output");
    for(int n=0;n<17;++n)
    {
        const float input=n==0?0.125f:(n%2?-0.0f:+0.0f);
        pushHistory(original,history,input);
        copied.push(input);assigned.push(input);
        const float truth=originalFirOutput(original,history);
        check(sameFir(original.output(),truth)&&sameFir(copied.output(),truth)
              &&sameFir(assigned.output(),truth),"signed-zero FIR copied histories wake exactly");
    }
    original.reset();
    check(sameFir(original.output(),+0.0f)&&original.getNonzeroHistoryCount()==0,
          "signed-zero FIR reset restores positive-zero output");
    for(int next:{2,4,8,0,-1,3})
    {
        assigned=copied; // Retain charged history before every reprepare.
        assigned.prepare(next);
        const int expected=cs80::OversamplingDecimator::validFactor(next)?next:2;
        check(assigned.getFactor()==expected&&sameFir(assigned.output(),+0.0f)
              &&assigned.getNonzeroHistoryCount()==0,"signed-zero FIR prepare and invalid-factor fallback");
    }
}

void coefficientAlignedSignedZeros()
{
    // Default construction is a defined zero-coefficient filter before prepare.
    cs80::OversamplingDecimator unprepared;
    History unpreparedHistory{};
    for(int k=0;k<unprepared.getTaps();++k)pushHistory(unprepared,unpreparedHistory,-0.0f);
    const float unpreparedTruth=originalFirOutput(unprepared,unpreparedHistory);
    check(std::bit_cast<std::uint32_t>(unprepared.getCoefficient(unprepared.getCenter()))==0u,
          "unprepared FIR center coefficient is positive zero");
    check(std::bit_cast<std::uint32_t>(unpreparedTruth)==0x80000000u,
          "unprepared negative-zero history makes an actual negative-zero oracle");
    check(sameFir(unprepared.output(),unpreparedTruth),"unprepared FIR retains negative-zero ordered output");
    signedZeroContinuation(unprepared,unpreparedHistory);
    for(int factor:{2,4,8})
    {
        cs80::OversamplingDecimator filter;
        filter.prepare(factor);
        const int taps=filter.getTaps();
        check(std::isfinite(filter.getCoefficient(filter.getCenter()))
              &&filter.getCoefficient(filter.getCenter())>0.0f,"prepared FIR center coefficient is positive");
        History history{};
        for(int k=0;k<taps;++k)
            history[std::size_t(k)]=std::signbit(filter.getCoefficient(k))?+0.0f:-0.0f;
        for(int position=0;position<taps;++position)
        {
            filter.reset();
            for(int n=0;n<position;++n)filter.push(+0.0f);
            // Reverse chronological order gives the independent oracle's history
            // at every ring position, including every retained output phase.
            for(int k=taps-1;k>=0;--k)filter.push(history[std::size_t(k)]);
            const float truth=originalFirOutput(filter,history);
            check(std::bit_cast<std::uint32_t>(truth)==0x80000000u,
                  "coefficient-aligned zero history makes every original summand negative zero");
            check(sameFir(filter.output(),truth),"coefficient-aligned signed zeros retain full FIR output bits");
            check(filter.getNonzeroHistoryCount()==0,"signed-zero history retains numeric nonzero count semantics");
            if(position==0||position==taps-1)signedZeroContinuation(filter,history);
        }
    }
}

void signedZeroEnvironments()
{
    const int rounding=std::fegetround();
    check(std::fesetround(FE_TONEAREST)==0,"FIR signed-zero fixtures select nearest rounding");
#if CS80_TEST_MXCSR
    const unsigned saved=_mm_getcsr();
    for(int mode=0;mode<2;++mode)
    {
        _mm_setcsr((saved&~0x8040u)|(mode?0x8040u:0u));
        coefficientAlignedSignedZeros();
    }
    _mm_setcsr(saved);
#else
    coefficientAlignedSignedZeros();
#endif
    std::fesetround(rounding);
}

}
int main()
{
#if CS80_TEST_MXCSR
    const unsigned savedMxcsr=_mm_getcsr();
#endif
    cs80::OversamplingDecimator d;
    cs80::Rng rng(0x5a5a9988u);
    for(int factor:{2,4,8,2,8,4})
    {
        // Reconfiguration must discard a genuinely nonzero prior history.
        d.push(1.0f);
        d.prepare(factor);
        std::array<float,cs80::OversamplingDecimator::kMaxTaps> chronological{};
        const int taps=d.getTaps(),middle=d.getCenter();
        for(int n=0;n<18000;++n)
        {
#if CS80_TEST_MXCSR
            // Change denormal modes while actual subnormal samples remain in
            // the delay memory. Classification must not depend on MXCSR.
            _mm_setcsr((savedMxcsr&~unsigned(0x8040))|((n/193)%2?unsigned(0x8040):0u));
#endif
            if(n==9000){d.reset();chronological.fill(0.f);}
            float x=0.f;
            if(n==2*taps || n==8999)x=1.f;
            else if(n>=4*taps && n<4*taps+3000 && n%17)x=rng.nextBipolar();
            else if(n>=10500 && n<12000)
                x=std::bit_cast<float>(std::uint32_t(1+n%19)|(n%2?0x80000000u:0u));
            else if(n%2)x=-0.f;
            for(int k=taps-1;k>0;--k)chronological[std::size_t(k)]=chronological[std::size_t(k-1)];
            chronological[0]=x;
            d.push(x);
            // Independent chronological indexing; the production implementation
            // uses a duplicated ring and a separate exact history count.
            double truth=double(d.getCoefficient(middle))*chronological[std::size_t(middle)];
            for(int k=0;k<middle;++k)
                truth+=double(d.getCoefficient(k))*(double(chronological[std::size_t(k)])
                        +double(chronological[std::size_t(taps-1-k)]));
            check(std::bit_cast<std::uint32_t>(d.output())==std::bit_cast<std::uint32_t>(float(truth)),
                  "full FIR chronological convolution including signed zeros");
            unsigned count=0;
            for(int k=0;k<taps;++k)count+=nonzero(chronological[std::size_t(k)])?1u:0u;
            check(d.getNonzeroHistoryCount()==count,"exact history count including subnormals");
        }
        check(d.getNonzeroHistoryCount()==0,"all work is elided after the complete history drains");
    }
    signedZeroEnvironments();
#if CS80_TEST_MXCSR
    _mm_setcsr(savedMxcsr);
#endif
    std::printf("Zero FIR history: %d checks, %d failures\n",checks,failures);
    return failures?1:0;
}
