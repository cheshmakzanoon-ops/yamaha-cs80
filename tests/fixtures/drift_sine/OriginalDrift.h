#pragma once

#include <cmath>

// Independently frozen copies of the original Engine.cpp expression at
// lines 2197 and 2215, before changing any Engine source. No optimized
// implementation may include or rewrite this software reference.
namespace cs80::drift_original_20261102 {

inline float sine(float drift, float phase, float voiceOffset) {
    constexpr float kTwoPi = 6.28318530717958647692f;
    return drift * std::sin(kTwoPi * (phase + voiceOffset)) * 0.9f;
}

inline float term(bool sourcePerformance, int driftModel, float drift,
                  float rankDriftCents, float sineValue) {
    constexpr float kDefaultDrift = 0.25f;
    return sourcePerformance && driftModel == 0
               ? drift / kDefaultDrift * rankDriftCents * 0.01f
               : sineValue;
}

} // namespace cs80::drift_original_20261102
