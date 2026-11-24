#pragma once

#include "InstrumentBody.h"

#include <algorithm>
#include <cmath>

namespace cs80::body
{

// Authored performance scaling for the optional instrument body, not measured
// hardware envelope constants. Capture once at an accepted strike, after
// footage/tuning have established the sounding register. The existing rank
// ADSR remains the amplitude owner; its filter contour remains unchanged.
// Partial blends approach the unchanged circuit articulation continuously.
struct Articulation
{
    float attack = 1.0f;
    float decay = 1.0f;
    float release = 1.0f;
};

inline Articulation captureArticulation(int model, float soundingNote,
                                       float velocity, float bodyMix) noexcept
{
    Articulation result;
    const float mix = std::isfinite(bodyMix) ? std::clamp(bodyMix, 0.0f, 1.0f) : 0.0f;
    if (model == off || model == organ || model < off || model > maxModel || mix == 0.0f)
        return result;

    const float note = std::isfinite(soundingNote) ? std::clamp(soundingNote, 0.0f, 127.0f) : 60.0f;
    const float strike = std::isfinite(velocity) ? std::clamp(velocity, 0.0f, 1.0f) : 0.7f;
    float decaySlope = 0.0f, releaseSlope = 0.0f;
    switch (model)
    {
        // The bass body can retain its aftersound while the upper register
        // remains articulate. Velocity shapes excitation, not string loss.
        case piano:       decaySlope = 0.015f; releaseSlope = 0.010f; break;
        case tine:        decaySlope = 0.010f; releaseSlope = 0.007f; break;
        case reed:        decaySlope = 0.012f; releaseSlope = 0.008f; break;
        case clav:        decaySlope = 0.012f; releaseSlope = 0.010f; break;
        case harpsichord:  decaySlope = 0.010f; releaseSlope = 0.010f; break;
        case nylon:       decaySlope = 0.010f; releaseSlope = 0.008f; break;
        case steel:       decaySlope = 0.009f; releaseSlope = 0.007f; break;
        case bass:        decaySlope = 0.008f; releaseSlope = 0.006f; break;
        case bell:        decaySlope = 0.008f; releaseSlope = 0.003f; break;
        case vibes:       decaySlope = 0.010f; releaseSlope = 0.007f; break;
        case marimba:     decaySlope = 0.015f; releaseSlope = 0.008f; break;
        case celesta:     decaySlope = 0.012f; releaseSlope = 0.008f; break;
        // Soft breath/bow entries grow more gradually. A firm strike speaks
        // sooner without adding pitch instability or a new modulation route.
        case bowed:       result.attack = 1.35f - 0.50f * strike; break;
        case flute:       result.attack = 1.25f - 0.42f * strike; break;
        case clarinet:    result.attack = 1.20f - 0.35f * strike; break;
        case oboe:        result.attack = 1.20f - 0.35f * strike; break;
        case bassoon:     result.attack = 1.28f - 0.40f * strike; break;
        case brass:       result.attack = 1.25f - 0.45f * strike; break;
        default: break;
    }
    result.decay = std::clamp(std::exp2((60.0f - note) * decaySlope), 0.65f, 1.55f);
    result.release = std::clamp(std::exp2((60.0f - note) * releaseSlope), 0.72f, 1.40f);
    result.attack = 1.0f + mix * (result.attack - 1.0f);
    result.decay = 1.0f + mix * (result.decay - 1.0f);
    result.release = 1.0f + mix * (result.release - 1.0f);
    return result;
}

} // namespace cs80::body
