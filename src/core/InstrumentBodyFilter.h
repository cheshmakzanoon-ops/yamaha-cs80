#pragma once

// The optional instrument excitation has its own inexpensive, linear spectral
// filter. It follows the rank's effective HP/LP controls, including envelope,
// touch, keyboard tracking and resonance, without clocking the nonlinear
// IG00156 circuit twice. This is an authored voicing filter, not an additional
// claim about the hardware signal path. No-body rendering never enters it.

#include <algorithm>
#include <cmath>

namespace cs80::body
{
class InstrumentBodyFilter
{
public:
    void prepare(double rate) noexcept
    {
        sampleRate = std::isfinite(rate) ? std::clamp(rate, 8000.0, 1536000.0) : 96000.0;
        switchCoefficient = -std::expm1(-1.0 / (0.001 * sampleRate));
        reset();
    }

    void reset() noexcept
    {
        hp = {};
        lp = {};
        countdown = 0;
        initialized = false;
        hpBlend = lpBlend = hpTarget = lpTarget = 0.0;
    }

    float tick(float input, float highPassHz, float lowPassHz,
               float highPassQ, float lowPassQ, bool highPassOn, bool lowPassOn) noexcept
    {
        hpTarget = highPassOn ? 1.0 : 0.0;
        lpTarget = lowPassOn ? 1.0 : 0.0;
        if (countdown == 0)
        {
            hp.set(highPassHz, highPassQ, sampleRate);
            lp.set(lowPassHz, lowPassQ, sampleRate);
            countdown = controlPeriod;
            if (!initialized)
            {
                hpBlend = hpTarget;
                lpBlend = lpTarget;
                initialized = true;
            }
        }
        --countdown;
        return tickFrozen(input);
    }

    // An outgoing body retains the same filter state and last controls as its
    // final sounding sample. Reassignment must not retune its resonances or
    // clear their memories during the short waveform handoff.
    float tickFrozen(float input) noexcept
    {
        hpBlend += switchCoefficient * (hpTarget - hpBlend);
        lpBlend += switchCoefficient * (lpTarget - lpBlend);
        const double source = input;
        const auto high = hp.process(source);
        const double afterHp = source + hpBlend * (high.hp - source);
        const auto low = lp.process(afterHp);
        return static_cast<float>(afterHp + lpBlend * (low.lp - afterHp));
    }

private:
    static constexpr int controlPeriod = 16;
    struct Stage
    {
        double ic1 = 0.0, ic2 = 0.0;
        double a1 = 1.0, a2 = 0.0, a3 = 0.0, damping = 2.0;
        double lastCutoff = -1.0, lastQ = -1.0;

        void set(double cutoff, double q, double rate) noexcept
        {
            cutoff = std::isfinite(cutoff) ? std::clamp(cutoff, 8.0, 0.45 * rate) : 1000.0;
            q = std::isfinite(q) ? std::clamp(q, 0.5, 4.0) : 0.5;
            if (cutoff == lastCutoff && q == lastQ) return;
            lastCutoff = cutoff;
            lastQ = q;
            const double g = std::tan(3.14159265358979323846 * cutoff / rate);
            damping = 1.0 / q;
            a1 = 1.0 / (1.0 + g * (g + damping));
            a2 = g * a1;
            a3 = g * a2;
        }

        struct Output { double hp, lp; };
        Output process(double x) noexcept
        {
            const double v3 = x - ic2;
            const double v1 = a1 * ic1 + a2 * v3;
            const double v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = 2.0 * v1 - ic1;
            ic2 = 2.0 * v2 - ic2;
            return { x - damping * v1 - v2, v2 };
        }
    };

    Stage hp, lp;
    double sampleRate = 96000.0, switchCoefficient = 0.01;
    double hpBlend = 0.0, lpBlend = 0.0, hpTarget = 0.0, lpTarget = 0.0;
    int countdown = 0;
    bool initialized = false;
};
} // namespace cs80::body
