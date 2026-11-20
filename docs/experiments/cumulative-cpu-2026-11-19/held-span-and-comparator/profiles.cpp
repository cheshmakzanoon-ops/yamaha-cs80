// High and low rates/times, multiple legal shaping profiles, a restored tiny
// reset countdown, clean/seeded sticky flags, independent directed preparation.
#define main separate_primary_matrix
#include "proof.cpp"
#undef main

template<class P> static P profile(int index) {
    P p;
    switch(index) {
        case 1: p.resetSecondsNominal=1e-6; p.referenceHzNominal=10000.; p.couplingSecondsNominal=.001; break;
        case 2: p.resetSecondsNominal=100e-6; p.referenceHzNominal=100.; p.couplingSecondsNominal=10.; break;
        case 3: p.resetVoltsNominal=.1; p.resetExcessVoltsNominal=.0899; p.triangleSpanVoltsNominal=20.; p.sineGainVoltsInferred=10.; break;
        case 4: p.resetVoltsNominal=20.; p.resetExcessVoltsNominal=0.; p.triangleSpanVoltsNominal=.1; p.sineGainVoltsInferred=.1; break;
        case 5: p.sliceVoltsInferred=-10.; p.pulseHalfSpanVoltsInferred=10.; break;
        case 6: p.sliceVoltsInferred=10.; p.pulseHalfSpanVoltsInferred=.1; break;
        case 7: p.resetSecondsNominal=37e-6; p.referenceHzNominal=432.; p.triangleSpanVoltsNominal=4.7;
                p.shaperOrdinateInferred={0.,.25,.5,.75,1.}; break;
        case 8: p.resetSecondsNominal=21e-6; p.resetVoltsNominal=4.2; p.resetExcessVoltsNominal=.3; p.sliceVoltsInferred=-.04;
                p.shaperOrdinateInferred={0.,.7,.9,.97,1.}; break;
    }
    return p;
}
int main() {
    constexpr unsigned masks[] {0x1f80,0x9fc0,0x1fbf,0x9f80,0x1fc0,0x5f80};
    const auto savedMx=_mm_getcsr();
    unsigned short cw;asm volatile("fnstcw %0":"=m"(cw));
    unsigned long long admit=0,events=0,count=0;
    for (double rate: {8000.,11025.,44100.,48000.,88200.,96000.,192000.,1536000.})
    for (int variant=0;variant<9;++variant) {
        S s;C c;
        initFp(variant%3?0x1f80:0x5f80);
        if (!s.prepare(rate,profile<cs80::OscillatorNominalProfile>(variant))) return 2;
        initFp(variant%3?0x1f80:0x5f80);
        if (!c.prepare(rate,profile<cs80_candidate::OscillatorNominalProfile>(variant))) return 2;
        initFp(0x1f80);
        for (int j=0;j<1536;++j) {
            constexpr double notes[]{440.,2048.,16.,123.456,0x1p-1022,2000.125};
            const double hz=notes[(j/67)%6];
            const bool saw=(j/89)%2,pulse=(j/127)%2;
            const double threshold=((j/61)%3-1)*.3125;
            if (j%137==15) {
                auto q=s.getState(); auto r=c.getState();
                q.resetRemainingSeconds=r.resetRemainingSeconds=std::bit_cast<double>(uint64_t(1+(j%9)));
                s.restoreState(q); c.restoreState(r);
            }
            if (j%101==5) {
                auto q=s.getState(); auto r=c.getState();
                auto bits=j%2?0x7ff0000000000015ull:0x8000000000000001ull;
                q.residual[j%3][(q.residualCursor+2)%16]=r.residual[j%3][(r.residualCursor+2)%16]=std::bit_cast<double>(bits);
                s.restoreState(q);c.restoreState(r);
            }
            auto mx=masks[(j/13)%6]; cs80::WscControls sc{threshold,saw,pulse};cs80_candidate::WscControls cc{threshold,saw,pulse};
            initFp(mx); const bool a=cs80::EngineWscHeldAccess::canBorrow(s,hz,sc);
            const auto x=a?cs80::EngineWscHeldAccess::run(s,hz):s.tick(hz,sc);const auto sf=flags();
            initFp(mx); const bool d=cs80_candidate::EngineWscHeldAccess::canBorrow(c,hz,cc);
            const auto y=d?cs80_candidate::EngineWscHeldAccess::run(c,hz):c.tick(hz,cc);const auto cf=flags();
            ck(a==d,"dispatch",variant,j);ck(out(x,y),"pins",variant,j);ck(sf==cf,"FP/errno",variant,j);
            compare(s,c,variant,j);
            admit+=a;events+=s.getDiagnostics().eventCount; ++count;
        }
    }
    _mm_setcsr(savedMx);asm volatile("fldcw %0"::"m"(cw));
    std::printf("profiles=9 rates=8 pairs=%llu admitted=%llu retained_events=%llu checks=%lld failures=%lld\n",count,admit,events,checks,failures);
    return failures?1:0;
}
