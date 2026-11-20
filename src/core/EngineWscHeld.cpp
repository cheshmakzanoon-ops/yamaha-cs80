#include "EngineWscHeld.h"

#if CS80_WSC_BORROW_HELD
namespace cs80 {
WscOutputVolts ReconstructedOscillator::tickBorrowedHeld(double f) noexcept {
    diagnostics.eventCount=0;
    const double inverseTauSamples=basis->inverseTauSamples,weightedArea=basis->weightedArea,weightedMoment=basis->weightedMoment;
    static_assert(std::is_trivially_destructible_v<Nodes>);
    static_assert(sizeof(WscOutputVolts)==3*sizeof(double) && sizeof(Nodes)==20*sizeof(double));
    union Slot {
        Nodes value;
        Slot() noexcept {}
        void set(const Nodes& all) noexcept { std::construct_at(&value,all); }
    } before,right;
    // The saved end is read-only until the ordinary final commit in the canonical loop.
    // A segment's start is a second view of the same right edge; it is last read before
    // calculating its replacement. No inactive member or stale segment view is read.
    struct RightEdge {
        const Nodes* node;
        Slot* scratch;
        operator const Nodes&() const noexcept { return *node; }
        RightEdge& operator=(const Nodes& n) noexcept { scratch->set(n); node=&scratch->value; return *this; }
    } after{&endNodes,&right};
    // These are operands of this segment's original crossing search, not
    // physical history. No ratio, endpoint or exponential crosses a segment.
    struct PreparedSpan {
        double pinSlope, inputAmplitude, triangleAmplitude;
        double secondsOverTau, decayFactor, inputEnd;
    };
    const auto prepareSpan = [&](const Nodes& n,double seconds) noexcept {
        const double tau=profile.couplingSecondsNominal;
        const double pinSlope=state.resetRemainingSeconds>0.?0.:-converterGain*state.heldFrequencyHz;
        const double asymptote=pinSlope*tau,b=n.u-asymptote;
        const double ratio=seconds/tau,e=decay(ratio);
        return PreparedSpan{pinSlope,b,n.triangleGain*b,ratio,e,asymptote+b*e};
    };
#define CS80_WSC_REUSE_HELD_SPAN 1
    const auto nextHeldEvent = [&](const Nodes& n,double limit,bool& event,const PreparedSpan& prepared) noexcept {
#include "WscNextBranchEvent.inc"
    };
    const auto advanceHeld = [&](double seconds,const Nodes& start,const PreparedSpan& prepared) noexcept {
#include "WscAdvanceSegment.inc"
    };
// This emitted path already requires the BorrowedHeld masked/nearest gate.
#define CS80_WSC_BORROWED_EXPLICIT_TERMINAL 1
#include "WscActiveSegments.inc"
#undef CS80_WSC_BORROWED_EXPLICIT_TERMINAL
#undef CS80_WSC_REUSE_HELD_SPAN
}
}
#endif
