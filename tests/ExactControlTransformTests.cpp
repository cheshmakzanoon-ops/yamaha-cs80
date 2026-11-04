#include "core/ExactControlTransforms.h"
#include <array>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <type_traits>

namespace
{
int checks = 0, failures = 0;
template<class T> void equal(T a, T b, const char* name)
{
    using U = std::conditional_t<sizeof(T)==4, std::uint32_t, std::uint64_t>;
    ++checks;
    if(std::bit_cast<U>(a)!=std::bit_cast<U>(b))
    { if(failures<12)std::fprintf(stderr,"FAIL %s: %.17g / %.17g\n",name,double(a),double(b)); ++failures; }
}
}
int main()
{
    cs80::MidiPitchMemo pitch;
    for(float reference : {0.f,400.f,440.f,442.f,480.f})
        for(int k=-1024;k<=2048;++k)
            for(int repeat=0;repeat<3;++repeat)
            {
                const float p=float(k)*.125f;
                equal(pitch(p,reference),reference*std::pow(2.f,(p-69.f)/12.f),"MIDI pitch");
            }
    // The registered zero reference includes both IEEE signs. A held pitch
    // must not reuse +0 for -0, or -0 for +0, across transitions and repeats.
    for(int k=-1024;k<=2048;++k)
        for(float reference : {+0.0f,+0.0f,-0.0f,-0.0f,+0.0f,-0.0f,+0.0f,+0.0f})
        {
            const float p=float(k)*.125f;
            equal(pitch(p,reference),reference*std::pow(2.f,(p-69.f)/12.f),"MIDI reference signed zero");
        }
    cs80::RingFrequencyMemo ring;
    cs80::SubTargetRateMemo sub;
    constexpr std::array<float,5> levels{0.f,.00001f,.1f,.7f,1.f};
    for(float base : {0.f,.7f,6.6666665f,60.f,200.f})
        for(float depth:levels) for(float envelope:levels) for(int repeat=0;repeat<3;++repeat)
        {
            const double control=double(cs80::clampf(depth,0.f,1.f))*double(cs80::clampf(envelope,0.f,1.f))*5.;
            const double octaves=std::log2(200./(1./.150))/(10.-5.);
            equal(ring(base,depth,envelope),float(double(std::max(0.f,base))*std::exp2(control*octaves)),"ring converter");
            const double b=std::clamp(double(base),double(cs80::OriginalSubOscillator::kMinRate),60.);
            const double touch=std::clamp(double(depth),0.,1.)*std::clamp(double(envelope),0.,1.);
            equal(sub(base,depth,envelope),b*std::exp2(touch*std::log2(200./60.)),"SUB converter");
        }
    cs80::ExpMemo<float> dc;
    for(float rate : {44100.f,48000.f,88200.f,96000.f,176400.f,192000.f,384000.f})
        for(int repeat=0;repeat<19;++repeat)
        {
            const float x=-cs80::kTwoPi*10.f*(1.f/rate);
            equal(dc(x),std::exp(x),"DC rate");
        }
    // Independent time-domain recurrence, including changes while the log node
    // is still moving, exact settled arguments, a reset and rate transitions.
    for(double rate:{88200.,96000.,192000.,384000.})
    {
        cs80::OriginalSubOscillator osc;
        double z=0.;bool primed=false;
        const double smoothing=-std::expm1(-1./(.01*rate));
        for(int n=0;n<32000;++n)
        {
            if(n==17321){osc.reset();primed=false;}
            const double hz=n<8000?.7:n<12000?60.:n<17000?200.:n<24000?3.25:3.25+double(n-24000)*.0003;
            const double target=std::log(hz);
            if(!primed){z=target;primed=true;}
            z+=(target-z)*smoothing;
            equal(osc.circuitRate(hz,smoothing),std::exp(z),"SUB continuous log node");
        }
    }
    std::printf("Exact transforms: %d checks, %d failures\n",checks,failures);
    return failures?1:0;
}
