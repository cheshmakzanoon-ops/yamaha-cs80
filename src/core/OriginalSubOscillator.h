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
        circuitPrimed = false;
    }

    float tick(float rate, float sampleRate, int function, float external = 0.0f) noexcept
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
            case 5: return external; // independent of phase/base speed/AT speed
            default: return 0.0f;
        }
    }

    // ---- Circuit engine (Source Performance / Isolation) --------------------------
    // tick()/targetRate() above stay the legacy-policy path, bit for bit.
    //
    // IG00150 speed converter. SUB29 sums VC3 (SPEED) and VC4 (pressure x AT SPEED)
    // currents through equal 120 k resistors into pin 1, which the IC guide (PDF29)
    // shows feeding an exponential converter: touch adds octaves, not hertz.
    // DECLARED INFERENCE: the service p52 200 Hz IG00150 calibration point is the rate
    // at full SPEED plus full touch, so full touch adds log2(200/60) octaves at every
    // base speed. Truth model: reference/Ig00150Sub.h; ws4-circuits.md Step 1.
    static constexpr double kConverterAnchorHz = 200.0;

    static double touchOctaves() noexcept
    {
        return std::log2(kConverterAnchorHz / (double) kMaxBaseRate);
    }
    static double circuitTargetRate(double baseHz, double depth, double pressure) noexcept
    {
        const double base = std::clamp(baseHz, (double) kMinRate, (double) kMaxBaseRate);
        const double touch = std::clamp(depth, 0.0, 1.0) * std::clamp(pressure, 0.0, 1.0);
        return base * std::exp2(touch * touchOctaves());
    }

    // The existing 10 ms original-mode smoothing, applied where an RC would act: on the
    // pin-1 control node, i.e. in log frequency. First call after reset starts at target.
    double circuitRate(double targetHz, double smoothing) noexcept
    {
        const double target = std::log(targetHz);
        if (! circuitPrimed)
        {
            circuitLogRate = target;
            circuitPrimed = true;
        }
        circuitLogRate += (target - circuitLogRate) * smoothing;
        return std::exp(circuitLogRate);
    }

    // Same functions, amplitudes, phase and noise stream as tick(), but the saw resets and
    // the comparator square are band-limited with a 4-sample integrated cubic B-spline
    // step (in-band images fall roughly as sinc^4). Sine, noise and EXT are unchanged.
    float tickCircuit(double rate, double sampleRate, int function, float external = 0.0f) noexcept
    {
        const double dt = rate / sampleRate;
        phase += dt;
        phase -= std::floor(phase);
        const float white = noise.nextBipolar();
        switch (function)
        {
            case 0: return (float) std::sin(6.28318530717958647692 * phase);
            case 1: return (float) ((1.0 - 2.0 * phase + 2.0 * stepResidual(phase, dt)) * kSawAmplitude);
            case 2: return (float) ((2.0 * phase - 1.0 - 2.0 * stepResidual(phase, dt)) * kSawAmplitude);
            case 3:
            {
                const double half = phase < 0.5 ? phase + 0.5 : phase - 0.5;
                return (float) ((phase < 0.5 ? 1.0 : -1.0)
                                + 2.0 * stepResidual(phase, dt) - 2.0 * stepResidual(half, dt));
            }
            case 4: return white;
            case 5: return external;
            default: return 0.0f;
        }
    }

    // Band-limited minus ideal unit step, for a step at phase 0 (wrap). t is the time to
    // the step in samples; the kernel is the cubic B-spline on [-2, 2], whose integral
    // S(t) gives S(t) - 1 = -S(-t) after the step.
    static double stepResidual(double phase, double dt) noexcept
    {
        if (! (dt > 0.0))
            return 0.0;
        const auto integral = [](double t) // S(t) for t in [-2, 0]
        {
            if (t <= -1.0)
            {
                const double u = 2.0 + t;
                return u * u * u * u / 24.0;
            }
            return 0.5 + t * (2.0 / 3.0) - t * t * t / 3.0 - t * t * t * t / 8.0;
        };
        const double after = phase / dt;
        if (after < 2.0)
            return -integral(-after);
        const double before = (phase - 1.0) / dt;
        if (before >= -2.0)
            return integral(before);
        return 0.0;
    }

    double getPhase() const noexcept { return phase; }

private:
    double phase = 0.0;
    Rng noise { 0x53554231u };
    double circuitLogRate = 0.0;
    bool circuitPrimed = false;
};
} // namespace cs80
