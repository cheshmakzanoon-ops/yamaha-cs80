#include "core/BbdChorus.h"
#include "core/PraWahCircuit.h"
#include "fixtures/idle_exact/BbdChorusBefore.h"
#include "fixtures/idle_exact/PraWahCircuitBefore.h"

#include <array>
#include <bit>
#include <cmath>
#include <cfenv>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <string>
#if defined(__SSE__) || defined(_M_X64) || defined(_M_IX86)
#include <xmmintrin.h>
#endif

// Exact software continuation, registered before evaluation in idle-exact-skip.json.
// The frozen oracle is independent of the optimized source and is not hardware evidence.
namespace {
int checks=0, failures=0;
void check(bool ok,const char* what,double rate,int profile=-1)
{
    ++checks;
    if (!ok) { ++failures; std::printf("FAIL: %s rate=%.0f profile=%d\n",what,rate,profile); }
}
bool same(float a,float b) { return std::bit_cast<std::uint32_t>(a)==std::bit_cast<std::uint32_t>(b); }
bool same(double a,double b) { return std::bit_cast<std::uint64_t>(a)==std::bit_cast<std::uint64_t>(b); }
constexpr std::array<double,6> rates {88200,96000,176400,192000,352800,384000};

void bbd(double rate)
{
    cs80::BbdChorus actual;
    cs80::BbdChorusBefore reference;
    actual.prepare((float)rate); reference.prepare((float)rate);
    cs80::Rng inputNoise(0x49444c45u);
    bool cold=true, wake=true, signal=true, tail=true, reentry=true;
    std::uint64_t samples=0;
    const int end=(int)(3.1*rate)+257;
    for (int i=0;i<end;++i)
    {
        const double seconds=i/rate;
        const std::array<float,4> speeds {0.0f,0.55f,17.857f,2.3f};
        const std::array<float,4> depths {0.0f,0.3f,1.0f,0.55f};
        const float speed=speeds[(i/521)%4], depth=depths[(i/173)%4];
        int tabs=0;
        if (i>=(int)rate && i<(int)rate+291) tabs=1+(i-(int)rate)/97;
        else if (seconds>=1.0) tabs=(i/257)%4;
        float left=0.0f, right=0.0f;
        if ((seconds>=1.04 && seconds<1.35) || seconds>=2.6)
        {
            left=0.11f*(float)std::sin(2.0*3.14159265358979323846*317.0*seconds)+0.01f*inputNoise.nextBipolar();
            right=0.07f*(float)std::sin(2.0*3.14159265358979323846*911.0*seconds)+0.01f*inputNoise.nextBipolar();
        }
        else if (i%20000==11) left=right=-0.0f;
        const auto a=actual.processIndependent(left,right,speed,depth,(tabs&1)!=0,(tabs&2)!=0);
        const auto b=reference.processIndependent(left,right,speed,depth,(tabs&1)!=0,(tabs&2)!=0);
        const bool equal=same(a.l,b.l)&&same(a.r,b.r);
        if (!equal && cold&&wake&&signal&&tail&&reentry)
            std::printf("BBD first mismatch rate=%.0f sample=%d tabs=%d actual=%a,%a reference=%a,%a\n",rate,i,tabs,a.l,a.r,b.l,b.r);
        if (seconds<1.0) cold&=equal;
        else if (seconds<1.04) wake&=equal;
        else if (seconds<1.35) signal&=equal;
        else if (seconds<2.6) tail&=equal;
        else reentry&=equal;
        ++samples;
    }
    check(cold,"BBD cold off-state output is bit-identical",rate);
    check(wake,"BBD off-to-on wake preserves phase and noise state bit-for-bit",rate);
    check(signal,"BBD signal and all four tab states are bit-identical",rate);
    check(tail,"BBD nonzero tails and re-entry to zero history are bit-identical",rate);
    check(reentry,"BBD wake from zero packet history is bit-identical",rate);
    actual.reset(); reference.reset();
    bool reset=true;
    for (int i=0;i<8192;++i)
    {
        const float input=i==0?0.1f:0.0f;
        const auto a=actual.process(input,-input,0.8f,0.9f,i%13<9,i%7<3);
        const auto b=reference.process(input,-input,0.8f,0.9f,i%13<9,i%7<3);
        reset&=same(a.l,b.l)&&same(a.r,b.r);
    }
    check(reset,"BBD reset and historical tab adapter remain bit-identical",rate);
    std::printf("BBD exact rate=%.0f compared=%llu samples\n",rate,(unsigned long long)(samples+8192));
}

void bbdDenormalModeTransition()
{
#if defined(__SSE__) || defined(_M_X64) || defined(_M_IX86)
    const unsigned saved=_mm_getcsr();
    // DAZ is an input rule; FTZ is an output rule. A normal input can produce
    // stored subnormal filter samples while DAZ is on and FTZ is off.
    _mm_setcsr((saved|0x0040u)&~0x8000u);
    cs80::BbdChorus actual; cs80::BbdChorusBefore reference;
    actual.prepare(96000); reference.prepare(96000);
    for (int i=0;i<4096;++i)
    {
        actual.processIndependent(2.0e-38f,2.0e-38f,0.55f,0.6f,false,false);
        reference.processIndependent(2.0e-38f,2.0e-38f,0.55f,0.6f,false,false);
    }
    _mm_setcsr(saved&~0x8040u);
    bool exact=true;
    for (int i=0;i<8192;++i)
    {
        // Start nonzero: a zero first sample would conservatively disable a broken
        // unsigned history count by underflow, masking the following unsafe zeros.
        const float input=0.13f*std::sin((i+1)*0.018f);
        const auto a=actual.processIndependent(input,input,0.55f,0.6f,true,false);
        const auto b=reference.processIndependent(input,input,0.55f,0.6f,true,false);
        const bool sameOutput=same(a.l,b.l)&&same(a.r,b.r);
        if (!sameOutput&&exact) std::printf("BBD denormal transition first mismatch sample=%d\n",i);
        exact&=sameOutput;
    }
    _mm_setcsr(saved);
    check(exact,"BBD history classification survives denormal-mode changes exactly",96000);
#endif
}

template<class Profile> Profile profileFor(int index)
{
    Profile p;
    if (index==1)
    {
        p.sourceOhms=0.0; p.dryLoadSiemens=0.001; p.vr1Ohms=0.0; p.vr2Ohms=0.0;
        p.opticalTauSeconds=0.0; p.opticalDecaySeconds=0.0;
    }
    if (index==2)
    {
        p.parallelOhms=10000000.0; p.sourceOhms=100000.0; p.dryLoadSiemens=0.00001;
        p.vr1Ohms=25000.0; p.vr2Ohms=100000.0; p.heelOhms=10000000.0; p.toeOhms=100.0;
        p.opticalTauSeconds=1.0; p.opticalDecaySeconds=5.0;
    }
    return p;
}

template<class A,class B> bool sameNodes(const A& a,const B& b)
{
    return same(a.x,b.x)&&same(a.n,b.n)&&same(a.a,b.a)&&same(a.p,b.p)&&same(a.b,b.b)
        &&same(a.q,b.q)&&same(a.c,b.c)&&same(a.d,b.d)&&same(a.y,b.y);
}
void pra(double rate,int profile)
{
    cs80::PraWahCircuit actual;
    cs80::PraWahCircuitBefore reference;
    check(actual.prepare(rate,profileFor<cs80::PraWahCircuit::Profile>(profile),0.4)
          &&reference.prepare(rate,profileFor<cs80::PraWahCircuitBefore::Profile>(profile),0.4),
          "PRA fixture accepts both identical profiles",rate,profile);
    bool zero=true, optical=true, nonzero=true, tail=true, caps=true;
    const int end=(int)(1.2*rate)+257;
    for (int i=0;i<end;++i)
    {
        const double seconds=i/rate;
        if (i%503==0)
        {
            const double pedal=0.1+0.8*std::abs(std::sin(i*0.013));
            actual.setPedal(pedal); reference.setPedal(pedal);
        }
        if (i%997==0)
        {
            const double ohms=(i/997)%2?2400.0:740000.0;
            actual.setOpticalResistance(ohms); reference.setOpticalResistance(ohms);
        }
        double input=0.0;
        if ((seconds>=0.35 && seconds<0.50)||seconds>=1.0)
            input=0.1*std::sin(2.0*3.14159265358979323846*173.0*seconds)+0.04*std::cos(i*0.023);
        else if (i%12345==1) input=-0.0;
        else if (i%12345==2) input=std::numeric_limits<double>::quiet_NaN();
        else if (i%12345==3) input=std::numeric_limits<double>::infinity();
        const auto a=actual.process(input); const auto b=reference.process(input);
        const bool equal=sameNodes(a,b);
        if (!equal && zero&&nonzero&&tail)
            std::printf("PRA first mismatch rate=%.0f profile=%d sample=%d actual=%a,%a reference=%a,%a\n",rate,profile,i,a.a,a.y,b.a,b.y);
        if (seconds<0.35) zero&=equal;
        else if (seconds<0.50 || seconds>=1.0) nonzero&=equal;
        else tail&=equal;
        optical&=same(actual.opticalResistance(),reference.opticalResistance())
            &&same(actual.targetOpticalResistance(),reference.targetOpticalResistance());
        const auto& ac=actual.capacitorState(); const auto& bc=reference.capacitorState();
        for (std::size_t k=0;k<ac.size();++k)
            caps&=same(ac[k].voltage,bc[k].voltage)&&same(ac[k].current,bc[k].current);
    }
    check(zero,"PRA exact-zero nodes preserve signed-zero and bounded-input semantics",rate,profile);
    check(optical,"PRA optical movement remains bit-identical through zero-state skipping",rate,profile);
    check(nonzero,"PRA first wake sample and nonzero signal nodes are bit-identical",rate,profile);
    check(tail,"PRA charged-capacitor zero-input tails are bit-identical",rate,profile);
    check(caps,"PRA all four capacitor voltages and currents are bit-identical",rate,profile);
    actual.reset(0.7); reference.reset(0.7);
    const auto a=actual.process(0.0); const auto b=reference.process(0.0);
    check(sameNodes(a,b)&&same(actual.opticalResistance(),reference.opticalResistance()),
          "PRA reset preserves exact zero-state semantics",rate,profile);
    std::printf("PRA exact rate=%.0f profile=%d compared=%d samples\n",rate,profile,end+1);
}
template<class A,class B> bool samePraReadouts(const A& actual,const B& before)
{
    bool equal=same(actual.sampleRate(),before.sampleRate())
        &&same(actual.opticalResistance(),before.opticalResistance())
        &&same(actual.targetOpticalResistance(),before.targetOpticalResistance());
    const auto& a=actual.capacitorState(); const auto& b=before.capacitorState();
    for(std::size_t k=0;k<a.size();++k)
        equal&=same(a[k].voltage,b[k].voltage)&&same(a[k].current,b[k].current);
    const auto& p=actual.getProfile(); const auto& q=before.getProfile();
    return equal&&same(p.parallelOhms,q.parallelOhms)&&same(p.vr1Ohms,q.vr1Ohms)
        &&same(p.vr2Ohms,q.vr2Ohms)&&same(p.sourceOhms,q.sourceOhms)
        &&same(p.dryLoadSiemens,q.dryLoadSiemens)&&same(p.heelOhms,q.heelOhms)
        &&same(p.toeOhms,q.toeOhms)&&same(p.opticalTauSeconds,q.opticalTauSeconds)
        &&same(p.opticalDecaySeconds,q.opticalDecaySeconds);
}
template<class A,class B> bool samePraStep(A& actual,B& before,double input)
{
    const auto a=actual.process(input); const auto b=before.process(input);
    return sameNodes(a,b)&&samePraReadouts(actual,before);
}
template<class Profile> Profile signedZeroProfile(double source,double trim)
{
    Profile p;
    p.sourceOhms=source;
    p.vr2Ohms=trim;
    return p;
}

void praSignedZeroProfiles(double rate,int mode)
{
    const std::array<double,3> sources {1000.0,+0.0,-0.0};
    const std::array<double,3> trims {20000.0,+0.0,-0.0};
    int variant=mode*100;
    for(double source:sources)for(double trim:trims)for(double initial:{+0.0,-0.0})
    {
        cs80::PraWahCircuit actual;
        cs80::PraWahCircuitBefore before;
        const auto p=signedZeroProfile<cs80::PraWahCircuit::Profile>(source,trim);
        const auto q=signedZeroProfile<cs80::PraWahCircuitBefore::Profile>(source,trim);
        check(actual.prepare(rate,p,0.4)&&before.prepare(rate,q,0.4),
              "PRA signed-zero boundary profiles remain accepted",rate,variant);
        bool cold=samePraReadouts(actual,before);
        cold&=samePraStep(actual,before,initial);
        actual.setPedal(0.1);before.setPedal(0.1);
        for(int n=0;n<17;++n)
            cold&=samePraStep(actual,before,n%2?+0.0:-0.0);
        check(cold,"PRA signed-zero profile nodes and capacitor states",rate,variant);

        // Copy/assignment must retain cold capacitor signs before a nonzero wake.
        auto copied=actual;auto copiedBefore=before;
        cs80::PraWahCircuit assigned;
        cs80::PraWahCircuitBefore assignedBefore;
        assigned=actual;assignedBefore=before;
        bool wake=samePraReadouts(copied,copiedBefore)&&samePraReadouts(assigned,assignedBefore);
        for(int n=0;n<65;++n)
        {
            const double input=n==0?0.125:(n%2?+0.0:-0.0);
            wake&=samePraStep(actual,before,input);
            wake&=samePraStep(copied,copiedBefore,input);
            wake&=samePraStep(assigned,assignedBefore,input);
        }
        check(wake,"PRA signed-zero profile copy/assignment and nonzero wake",rate,variant);

        actual.reset(0.7);before.reset(0.7);
        bool reset=samePraReadouts(actual,before);
        for(int n=0;n<4;++n)reset&=samePraStep(actual,before,n%2?+0.0:-0.0);
        check(reset,"PRA signed-zero profile reset preserves all readouts",rate,variant);
        const bool accepted=actual.prepare(rate,p,0.9)&&before.prepare(rate,q,0.9);
        bool prepared=accepted&&samePraReadouts(actual,before);
        prepared&=samePraStep(actual,before,initial);
        check(prepared,
              "PRA signed-zero profile reprepare preserves all readouts",rate,variant);
        ++variant;
    }
}

void praSignedZeroEnvironment()
{
    const int rounding=std::fegetround();
    check(std::fesetround(FE_TONEAREST)==0,"PRA signed-zero fixture selects nearest rounding",0);
#if defined(__SSE__) || defined(_M_X64) || defined(_M_IX86)
    const unsigned saved=_mm_getcsr();
    for(int mode=0;mode<2;++mode)
    {
        _mm_setcsr((saved&~0x8040u)|(mode?0x8040u:0u));
        for(double rate:rates)praSignedZeroProfiles(rate,mode);
    }
    _mm_setcsr(saved);
#else
    for(double rate:rates)praSignedZeroProfiles(rate,0);
#endif
    std::fesetround(rounding);
}

} // namespace

int main()
{
#if defined(__SSE__) || defined(_M_X64) || defined(_M_IX86)
    const unsigned old=_mm_getcsr();
    _mm_setcsr(old|0x8040u);
#endif
    volatile float positiveSubnormal=std::bit_cast<float>(std::uint32_t(1));
    volatile float negativeSubnormal=std::bit_cast<float>(std::uint32_t(0x80000001u));
    check(cs80::bbd_detail::hasNonzeroMagnitude(positiveSubnormal)
          &&cs80::bbd_detail::hasNonzeroMagnitude(negativeSubnormal)
          &&!cs80::bbd_detail::hasNonzeroMagnitude(0.0f)
          &&!cs80::bbd_detail::hasNonzeroMagnitude(-0.0f),
          "BBD history predicate classifies stored bits independently of denormal mode",96000);
    bbdDenormalModeTransition();
    for (double rate:rates)
    {
        bbd(rate);
        for (int p=0;p<3;++p) pra(rate,p);
    }
    praSignedZeroEnvironment();
#if defined(__SSE__) || defined(_M_X64) || defined(_M_IX86)
    _mm_setcsr(old);
#endif
    std::printf("Idle exact skips: %d checks, %d failures\n",checks,failures);
    return failures?1:0;
}
