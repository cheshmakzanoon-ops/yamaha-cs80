#pragma once
#include "Dsp.h"

#if defined(__GNUC__) && !defined(_WIN32)
#define CS80_OUTPUT_FIR_INTERNAL __attribute__((visibility("hidden")))
#else
#define CS80_OUTPUT_FIR_INTERNAL
#endif
namespace cs80
{
// Engine-owned decimator. Its sole writer and the fast/ordinary choice live
// in the Engine implementation TU. Callers cannot bypass the proof by mutating
// the underlying FIR. This definition/layout is independent of caller FP flags.
class EngineOutputFir final
{
public:
    CS80_OUTPUT_FIR_INTERNAL void prepare(int factor) noexcept;
    CS80_OUTPUT_FIR_INTERNAL void reset() noexcept;
    CS80_OUTPUT_FIR_INTERNAL void push(float x) noexcept;
    CS80_OUTPUT_FIR_INTERNAL float output() const noexcept;
    int getRetainedPhase() const noexcept { return scalar.getRetainedPhase(); }
    int getTaps() const noexcept { return scalar.getTaps(); }
    int getCenter() const noexcept { return scalar.getCenter(); }
    static constexpr int kMaxTaps = OversamplingDecimator::kMaxTaps;

private:
    OversamplingDecimator scalar;
    // Default scalar history and coefficients are all +0. Default copy/assignment
    // preserve the history, cursor and the corresponding certificate together.
    int uniformZeroRun = OversamplingDecimator::tapsFor(2);
    bool safeZeroCoefficients = true, hasNegativeWitness = false;
    std::uint16_t samplesToWidened = 0; // outstanding raw-only exceptional history
    CS80_OUTPUT_FIR_INTERNAL bool qualifies() const noexcept;
    friend struct EngineOutputFirTestAccess;
};
} // namespace cs80
#undef CS80_OUTPUT_FIR_INTERNAL
