#pragma once

#include "Dsp.h"

#include <cfenv>
#include <cmath>
#include <cstdint>

// Exact branch experiment: only float-zero drift at engine-sized, finite
// phase/offset positions, never a new analog drift law or sine approximation.
// Preserve the original signed-zero result and the caller's sticky inexact
// exception; all boundary, moving and foreign-domain inputs use std::sin.
namespace cs80::drift_opt {

#if defined(CS80_DRIFT_SINE_SOURCE_WITNESS)
struct SourceWorkCounters {
    std::uint64_t fastZeroBranches=0;
    std::uint64_t originalSineCalls=0;
};
extern thread_local SourceWorkCounters sourceWork;
#endif

inline float sine(float drift, float phase, float voiceOffset) {
#if defined(CS80_DRIFT_MUTATE_SOURCE_FAKE_SITE) && defined(CS80_DRIFT_SINE_SOURCE_WITNESS)
    // Negative control: fabricate a "fast" label but still do real sinf.
    if (drift==0.0f) {
        ++sourceWork.fastZeroBranches;
        ++sourceWork.originalSineCalls;
        return drift*std::sin(kTwoPi*(phase+voiceOffset))*0.9f;
    }
#endif
#if !defined(CS80_DRIFT_MUTATE_DISABLE_SOURCE_FAST)
    if (drift==0.0f && phase>=0.0f && phase<1.0f &&
        voiceOffset>=0.0f && voiceOffset<6.0f) {
        const float turn=phase+voiceOffset;
        // Positive engine turns are below seven; float*2 is exact. Stay far
        // from all n*pi crossings (including theta=0) where a libm result's
        // sign can differ from the algebraic half-turn sign after rounding.
        if (turn>=1.0e-4f && turn<7.0f) {
            const int half=static_cast<int>(turn*2.0f);
            const float frac=turn-(float)half*0.5f;
            if (frac>=1.0e-4f && frac<=0.4999f) {
                // For nonzero finite sine of a normal input, original sinf
                // raises FE_INEXACT. This volatile float rounding step raises
                // that same sticky bit under each IEEE rounding direction.
                // No process-global rounding mode or exception is cleared.
                volatile float fpInexact=1.0f;
                fpInexact=fpInexact+0x1p-24f;
                (void)fpInexact;
#if defined(CS80_DRIFT_MUTATE_FAST_CLEAR_FLAGS)
                std::feclearexcept(FE_ALL_EXCEPT);
#endif
#if defined(CS80_DRIFT_SINE_SOURCE_WITNESS)
                ++sourceWork.fastZeroBranches;
#endif
#if defined(CS80_DRIFT_MUTATE_FAST_SIGN)
                const float sineSign=(half & 1)?1.0f:-1.0f;
#else
                const float sineSign=(half & 1)?-1.0f:1.0f;
#endif
                return drift*sineSign*0.9f;
            }
        }
    }
#endif
#if defined(CS80_DRIFT_SINE_SOURCE_WITNESS)
    ++sourceWork.originalSineCalls;
#endif
    return drift*std::sin(kTwoPi*(phase+voiceOffset))*0.9f;
}

} // namespace cs80::drift_opt
