#pragma once

// MIDI controller response curves (IMPROVEMENT_PLAN 2.4).
//
// INTERFACE LAYER, not a CS-80 circuit: these adapt the response of the
// connected MIDI keyboard to the instrument's touch inputs, the way a player
// would set up a controller. They act on MIDI velocity and pressure before any
// circuit model sees them, in every render policy (including PURE CS-80, since
// they calibrate the controller rather than add an instrument feature). At the
// default settings (curve 0, threshold 0) both maps are the exact identity, so
// default renders are unchanged.

#include <algorithm>
#include <cmath>

namespace cs80
{
// curve in [-1, 1]: 0 linear; positive lifts light playing (soft keyboard),
// negative needs harder playing (heavy keyboard). Exponent 4^-curve: 0.25..4.
inline float applyVelocityCurve(float velocity, float curve) noexcept
{
    velocity = std::clamp(velocity, 0.0f, 1.0f);
    if (curve == 0.0f || velocity <= 0.0f || velocity >= 1.0f) return velocity;
    return std::pow(velocity, std::exp2(-2.0f * std::clamp(curve, -1.0f, 1.0f)));
}

// threshold in [0, 0.5): pressure below it is the controller's resting load and
// reads as zero; the remainder is rescaled to the full range before the curve.
inline float applyPressureCurve(float pressure, float curve, float threshold) noexcept
{
    pressure = std::clamp(pressure, 0.0f, 1.0f);
    if (curve == 0.0f && threshold <= 0.0f) return pressure;
    threshold = std::clamp(threshold, 0.0f, 0.5f);
    const float scaled = threshold > 0.0f ? std::max(0.0f, pressure - threshold) / (1.0f - threshold) : pressure;
    return applyVelocityCurve(scaled, curve);
}
} // namespace cs80
