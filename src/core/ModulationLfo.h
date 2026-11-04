#pragma once

#include "Dsp.h"

#include <cstdint>

namespace cs80
{

// Modern-layer LFO waveforms (IMPROVEMENT_PLAN Phase 6). These three LFOs are a software
// extension, not CS-80 circuitry: the instrument's own modulation sources are the SUB
// oscillator, the ring modulator envelope and the PWM oscillators. PURE CS-80 already
// zeroes their depth; the waveform choice is a further modern-only control.
//
// Sine is the historical waveform: it keeps the exact float phase law and sin() call of
// Lfo::tick(..., 0, ...) so a default render stays byte-identical. The other shapes are
// phase-aligned with that sine: every one is zero (or at its period mean) at phase 0 and
// reaches its positive extreme in the first half period.
enum class LfoWave : int
{
    sine = 0,
    triangle,
    sawUp,
    sawDown,
    square,
    sampleHold,
    smoothRandom,
};
inline constexpr int kLfoWaveCount = 7;

inline const char* lfoWaveName(int wave) noexcept
{
    static constexpr const char* names[kLfoWaveCount] = {
        "Sine", "Triangle", "Saw up", "Saw down", "Square", "S&H", "Smooth random" };
    return wave >= 0 && wave < kLfoWaveCount ? names[wave] : "Sine";
}

// Analytic shape for a phase in [0, 1). The random shapes need history and are handled by
// ModulationLfo itself; here they return 0.
inline float lfoShape(int wave, float phase) noexcept
{
    switch ((LfoWave) wave)
    {
        case LfoWave::sine: return std::sin(kTwoPi * phase);
        case LfoWave::triangle:
        {
            float p = phase + 0.25f;
            p -= std::floor(p);
            return 1.0f - 4.0f * std::abs(p - 0.5f);
        }
        case LfoWave::sawUp:
        {
            float p = phase + 0.5f;
            p -= std::floor(p);
            return 2.0f * p - 1.0f;
        }
        case LfoWave::sawDown:
        {
            float p = phase + 0.5f;
            p -= std::floor(p);
            return 1.0f - 2.0f * p;
        }
        case LfoWave::square: return phase < 0.5f ? 1.0f : -1.0f;
        default: return 0.0f;
    }
}

class ModulationLfo
{
public:
    explicit ModulationLfo(std::uint32_t seed = 0x6c8e9cf5u) noexcept : seed(seed), rng(seed) {}

    // Same signature as Lfo::reset. The private generator is re-seeded so random shapes
    // repeat after a reset and never consume the engine's shared modulation stream.
    void reset(float p = 0.0f) noexcept
    {
        phase = p;
        shapePhase = (double) p;
        rng = Rng(seed);
        started = false;
        held = from = to = 0.0f;
    }

    // Readouts may be unused by the engine, but both phase clocks and the
    // selected random-shape state still advance exactly as on an audible tick.
    float tick(float rate, float sampleRate, int waveform, bool emitOutput = true) noexcept
    {
        // Historical sine: exactly the Lfo::tick float phase law. Its rounding is biased at
        // low rates (a 1 Hz sine at 96 kHz runs measurably fast), but it is pinned for
        // compatibility, so only the sine keeps it.
        phase += rate / sampleRate;
        phase -= std::floor(phase);
        // Every other shape runs on a double-precision phase, so its period is rate-exact.
        shapePhase += (double) rate / (double) sampleRate;
        const bool wrapped = shapePhase >= 1.0;
        if (wrapped)
            shapePhase -= std::floor(shapePhase);
        switch ((LfoWave) waveform)
        {
            case LfoWave::sine: return emitOutput ? std::sin(kTwoPi * phase) : 0.0f;
            case LfoWave::sampleHold:
                // A new level at every period start, and immediately on the first tick.
                if (! started || wrapped)
                {
                    held = rng.nextBipolar();
                    started = true;
                }
                return held;
            case LfoWave::smoothRandom:
                // Raised-cosine interpolation from one random level to the next across
                // each period: continuous, with zero slope at every period boundary.
                if (! started)
                {
                    from = rng.nextBipolar();
                    to = rng.nextBipolar();
                    started = true;
                }
                else if (wrapped)
                {
                    from = to;
                    to = rng.nextBipolar();
                }
                if (! emitOutput) return 0.0f;
                return from + (to - from) * (0.5f - 0.5f * (float) std::cos(3.14159265358979323846 * shapePhase));
            default: return emitOutput ? lfoShape(waveform, (float) shapePhase) : 0.0f;
        }
    }

    float getPhase() const noexcept { return phase; }            // historical sine phase
    double getShapePhase() const noexcept { return shapePhase; } // every other shape

private:
    std::uint32_t seed;
    Rng rng;
    float phase = 0.0f;
    double shapePhase = 0.0;
    bool started = false;
    float held = 0.0f, from = 0.0f, to = 0.0f;
};

} // namespace cs80
