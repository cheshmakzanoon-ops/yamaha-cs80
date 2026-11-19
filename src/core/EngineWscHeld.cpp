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
#include "WscActiveSegments.inc"
}
}
#endif
