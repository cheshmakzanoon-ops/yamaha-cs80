#include "cold_cutoff-v2c.h"

#include <bit>
#include <cerrno>
#include <cfenv>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <xmmintrin.h>

namespace sf = cs80::source_factory;
using Read = sf::CutoffReadout;
using Drives = sf::RankDrives;
using T = sf::ToneControls;

[[gnu::noipa]] bool scalar(const T& tone, const Drives& d, Read& out) {
    return sf::evaluateCutoff(tone,d,out);
}
[[gnu::noipa]] bool research(const T& tone, const Drives& d, Read& out, bool& fast) {
    return cold_cutoff::evaluate(tone,d,out,&fast);
}
std::uint64_t bits(double x) { return std::bit_cast<std::uint64_t>(x); }
bool same(const Read& a, const Read& b) {
    if (a.saturatedVoices != b.saturatedVoices) return false;
    for (int s=0; s<2; ++s) {
        if (bits(a.loadedBusVolts[s]) != bits(b.loadedBusVolts[s])) return false;
        for (int v=0; v<sf::voiceCount; ++v) {
            if (bits(a.pinVolts[v][s]) != bits(b.pinVolts[v][s]) ||
                bits(a.effectiveCv[v][s]) != bits(b.effectiveCv[v][s])) return false;
        }
    }
    return true;
}
struct End { unsigned mx; unsigned short status, mode; int err; bool accepted; };
End observe(bool accepted) {
    End e{.mx=_mm_getcsr(),.status=0,.mode=0,.err=errno,.accepted=accepted};
    asm volatile("fnstsw %0; fnstcw %1" : "=m"(e.status),"=m"(e.mode) :: "memory");
    return e;
}
void start(int rounding, unsigned opts) {
    std::fesetenv(FE_DFL_ENV);
    std::fesetround(rounding);
    _mm_setcsr(_mm_getcsr() | opts);
    errno=117;
}
bool same(End a, End b) {
    return a.mx==b.mx && a.status==b.status && a.mode==b.mode && a.err==b.err && a.accepted==b.accepted;
}

#ifndef EXPECTED_OFF
#define EXPECTED_OFF 0
#endif

int main() {
    unsigned hits=0, trials=0;
    for (int rank=0; rank<2; ++rank) for (int selector=0; selector<11; ++selector)
    for (auto hp : {sf::Grade::A,sf::Grade::B,sf::Grade::C})
    for (auto lp : {sf::Grade::A,sf::Grade::B,sf::Grade::C}) {
        T ordinary;
        if (!sf::compileTone(rank,selector,ordinary,hp,lp)) return 2;
        for (int mutation=0; mutation<4; ++mutation) {
            auto tone=ordinary;
            if (mutation==1) tone.provenance.version=0;
            if (mutation==2) tone.initialBrillianceVolts=std::numeric_limits<double>::infinity();
            if (mutation==3) tone.cutoff[1].effectiveCvOffset=std::numeric_limits<double>::quiet_NaN();
            for (int mode : {FE_TONEAREST,FE_DOWNWARD,FE_UPWARD,FE_TOWARDZERO})
            for (unsigned opts : {0u,0x40u,0x8000u,0x8040u,0x20u,0x3fu})
            for (int scene=0; scene<22; ++scene) {
                Drives d{};
                const auto i=static_cast<std::size_t>((selector+rank+scene)%sf::voiceCount);
                if (scene==1) d[i].pressure = -0.;
                if (scene==2) d[i].filterEnvelopeZ=std::numeric_limits<double>::denorm_min();
                if (scene==3) d[i].velocity=std::numeric_limits<double>::min();
                if (scene==4) d[i].filterEnvelopeZ=-.25;
                if (scene==5) d[i].pressure=.5;
                if (scene==6) d[i].velocity=1.5;
                if (scene==7) d[i].velocity=std::bit_cast<double>(std::uint64_t{0x7ff0000000000001});
                if (scene==8) d[i].filterEnvelopeZ=std::numeric_limits<double>::infinity();
                if (scene==9) d[i].pressure=std::numeric_limits<double>::quiet_NaN();
                if (scene==10) d.fill({-.12,.6,.4});
                if (scene==11) d.fill({-3.,-0.,0.});
                if (scene==12) d.fill({std::numeric_limits<double>::denorm_min(), -0., 0.});
                if (scene==13) d.fill({-3.25,1.12,-.04});
                if (scene==14) d.fill({0.,std::bit_cast<double>(std::uint64_t{0x7ff0000000000001}),0.});
                if (scene==15) d.fill({0.,std::numeric_limits<double>::quiet_NaN(),0.});
                if (scene==16) d.fill({0.,0.,std::numeric_limits<double>::infinity()});
                if (scene==17) d.fill({3e304, 1e299, -1e200});
                if (scene==18) d.fill({-0.,-0.,-0.});
                if (scene==19) d.fill({-3., .8, 0.});
                if (scene==20) { d.fill({-3., .8, 0.}); d[7].velocity = std::nextafter(.8,1.0); }
                if (scene==21) { d.fill({-3., .8, 0.}); d[7].filterEnvelopeZ = std::nextafter(-3.,0.0); }
                Read a,b; a.loadedBusVolts=b.loadedBusVolts={42.,-17.}; a.saturatedVoices=b.saturatedVoices=27;
                bool fast=false;
                start(mode,opts); const auto x=observe(scalar(tone,d,a));
                start(mode,opts); const auto y=observe(research(tone,d,b,fast));
                if (!same(x,y) || !same(a,b) || fast!=((scene==0 || (scene>=10 && scene<20)) && mode==FE_TONEAREST && !EXPECTED_OFF)) {
                    std::fprintf(stderr,"mismatch: rank=%d selector=%d hp=%d lp=%d mutation=%d mode=%d opts=%u scene=%d fast=%d xmx=%x ymx=%x\n",
                                 rank,selector,int(hp),int(lp),mutation,mode,opts,scene,fast,x.mx,y.mx);
                    return 3;
                }
                ++trials; hits+=fast;
            }
        }
    }
    std::printf("exact paired return/readout/MXCSR/x87/errno: %u; fast-path admission: %u\n",trials,hits);
}
