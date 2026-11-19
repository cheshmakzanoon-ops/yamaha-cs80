// The direct public tick is the reference. Neither oscillator is reset on a control change.
#include "core/ReconstructedOscillator.h"
#include "core/EngineWscHeld.h"
#include <array>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <limits>

namespace {
using O = cs80::ReconstructedOscillator;
using V = cs80::WscOutputVolts;
using C = cs80::WscControls;
bool same(double a, double b) { return std::bit_cast<std::uint64_t>(a) == std::bit_cast<std::uint64_t>(b); }
bool same(const V& a, const V& b) {
    return same(a.sawPin5,b.sawPin5) && same(a.pulsePin7,b.pulsePin7) && same(a.sinePin2,b.sinePin2);
}
bool same(const O::State& a, const O::State& b) {
    if (!same(a.charge,b.charge) || !same(a.resetRemainingSeconds,b.resetRemainingSeconds)
        || !same(a.inputBaselineVolts,b.inputBaselineVolts) || !same(a.triangleBaselineVolts,b.triangleBaselineVolts)
        || !same(a.heldFrequencyHz,b.heldFrequencyHz) || !same(a.heldThresholdVolts,b.heldThresholdVolts)
        || !same(a.elapsedSeconds,b.elapsedSeconds) || a.sawEnabled!=b.sawEnabled || a.pulseEnabled!=b.pulseEnabled
        || a.residualCursor!=b.residualCursor || a.resetCount!=b.resetCount) return false;
    for (std::size_t j=0;j<a.residual.size();++j)
        for (std::size_t k=0;k<a.residual[j].size();++k)
            if (!same(a.residual[j][k],b.residual[j][k])) return false;
    return true;
}
bool same(const O::Diagnostics& a, const O::Diagnostics& b) {
    if (a.invalidControl!=b.invalidControl || a.frequencyLimited!=b.frequencyLimited
        || a.eventCapacityExceeded!=b.eventCapacityExceeded || a.eventCount!=b.eventCount
        || !same(a.rawPin7Volts,b.rawPin7Volts) || !same(a.coupledInputVolts,b.coupledInputVolts)
        || !same(a.trianglePin3Volts,b.trianglePin3Volts) || !same(a.shaperInputVolts,b.shaperInputVolts) || !same(a.raw,b.raw)) return false;
    // Stale diagnostic slots are deliberately compared, not just the current event count.
    for (std::size_t j=0;j<a.events.size();++j) {
        const auto& x=a.events[j]; const auto& y=b.events[j];
        if (!same(x.secondsFromSampleStart,y.secondsFromSampleStart) || !same(x.absoluteSeconds,y.absoluteSeconds)
            || !same(x.charge,y.charge) || x.kind!=y.kind || x.resetActive!=y.resetActive
            || !same(x.before,y.before) || !same(x.after,y.after)) return false;
    }
    return true;
}
}

int main() {
    unsigned calls=0, borrowed=0, fallback=0;
    for (double rate : {8000.,44100.,96000.,1536000.}) {
        O reference,receiver;
        if (!reference.prepare(rate,{}) || !receiver.prepare(rate,{})) return 1;
        for (unsigned i=0;i<768;++i) {
            const double hz=i<256 ? 440. : i<512 ? 2048. : i<640 ? 4096. : 220.;
            C controls{(i/61)%2 ? -.0 : +.0, (i/37)%2==0, (i/73)%2==0};
            if (i%127==73) controls.pulseThresholdVolts=std::numeric_limits<double>::quiet_NaN();
            if (i%131==29) {reference.restoreState(reference.getState());receiver.restoreState(receiver.getState());}
            const auto wanted=reference.tick(hz,controls);
            bool use=false;
#if CS80_WSC_BORROW_HELD
            use=cs80::EngineWscHeldAccess::canBorrow(receiver,hz,controls);
            const auto got=use ? cs80::EngineWscHeldAccess::run(receiver,hz) : receiver.tick(hz,controls);
#else
            const auto got=receiver.tick(hz,controls);
#endif
            if (use) ++borrowed; else ++fallback;
            if (!same(wanted,got) || !same(reference.getState(),receiver.getState())
                || !same(reference.getDiagnostics(),receiver.getDiagnostics())) {
                std::fprintf(stderr,"WSC endpoint: rate %.0f sample %u differed\n",rate,i);
                return 2;
            }
            ++calls;
        }
    }
#if CS80_WSC_BORROW_HELD
    // A default native test process has standard masks and nearest rounding.
    if (!borrowed || !fallback) return 3;
#endif
    std::printf("WSC endpoint: %u ticks, %u borrowed, %u scalar\n",calls,borrowed,fallback);
    return 0;
}
