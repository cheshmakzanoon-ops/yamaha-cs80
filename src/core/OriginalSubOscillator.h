#pragma once

#include "Dsp.h"

namespace cs80
{
// Functional original SUB source, not a nonlinear IG00150/NJM4558 model.
// Yamaha owner pp8-10,49; service SUB29/30 and Panel2 switch.
// One shared free-running phase, no key retrigger. Legacy Lfo stays untouched.
class OriginalSubOscillator
{
public:
    static constexpr float kMinRate = 0.7f;
    static constexpr float kMaxBaseRate = 60.0f;
    // PROVISIONAL: retain the existing bounded pressure multiplier, not a fit.
    // VC3/VC4 add currents through120k; IG00150 pin1 feeds an exponential
    // converter, so additive input currents do not imply additive Hz.
    static constexpr float kAftertouchRateFactor = 2.0f;
    static constexpr float kMaxRate = kMaxBaseRate * (1.0f + kAftertouchRateFactor);
    static constexpr float kSawAmplitude = 3.5f / 3.2f; // nominal Vpp ratio only

    static float targetRate(float base, float depth, float pressure) noexcept
    {
        return clampf(base * (1.0f + kAftertouchRateFactor * depth * pressure), kMinRate, kMaxRate);
    }

    void reset() noexcept
    {
        phase = 0.0;
        noise = Rng(0x53554231u);
    }

    float tick(float rate, float sampleRate, int function) noexcept
    {
        // Engine bounds finite rates/sample rates; double phase avoids low-rate
        // float quantisation. Noise advances on every original-mode sample,
        // independent of function/rate; it is a separate stream from legacy DSP.
        phase += (double) rate / sampleRate;
        phase -= std::floor(phase);
        const float white = noise.nextBipolar();
        switch (function)
        {
            case 0: return (float) std::sin(6.28318530717958647692 * phase);
            case 1: return (float) (1.0 - 2.0 * phase) * kSawAmplitude;
            case 2: return (float) (2.0 * phase - 1.0) * kSawAmplitude;
            // IC6 compares the sine with ground. Swing/edge dynamics remain
            // uncalibrated, so use an explicitly normalized ideal square.
            case 3: return phase < 0.5 ? 1.0f : -1.0f;
            case 4: return white; // ideal white source, not measured zener noise
            case 5: return 0.0f; // EXT disconnected until optional input bus exists
            default: return 0.0f;
        }
    }

    double getPhase() const noexcept { return phase; }

private:
    double phase = 0.0;
    Rng noise { 0x53554231u };
};
} // namespace cs80
