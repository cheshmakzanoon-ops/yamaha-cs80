#pragma once
// Exact memoization of existing control laws. These caches change no law,
// constant, arithmetic expression, state rate or accepted control value.
// Their independent equality contract is tests/cpu/exact-transform-memos.json.
#include "Dsp.h"
#include "OriginalSubOscillator.h"
#include "RingModulatorCircuit.h"
#include <cmath>
#include <limits>

namespace cs80
{
struct MidiPitchMemo
{
    float operator()(float pitch, float reference) noexcept
    {
        if (!(pitch == lastPitch && reference == lastReference
              && std::signbit(reference) == std::signbit(lastReference)))
        {
            value = midiToHz(pitch, reference);
            lastPitch = pitch;
            lastReference = reference;
        }
        return value;
    }
private:
    float lastPitch = std::numeric_limits<float>::quiet_NaN();
    float lastReference = std::numeric_limits<float>::quiet_NaN();
    float value = 0.0f;
};

struct RingFrequencyMemo
{
    float operator()(float base, float depth, float envelope) noexcept
    {
        if (!(base == lastBase && depth == lastDepth && envelope == lastEnvelope))
        {
            value = RingModulatorCircuit::frequency(base, depth, envelope);
            lastBase = base;
            lastDepth = depth;
            lastEnvelope = envelope;
        }
        return value;
    }
private:
    float lastBase = std::numeric_limits<float>::quiet_NaN();
    float lastDepth = std::numeric_limits<float>::quiet_NaN();
    float lastEnvelope = std::numeric_limits<float>::quiet_NaN();
    float value = 0.0f;
};

struct SubTargetRateMemo
{
    double operator()(double base, double depth, double pressure) noexcept
    {
        if (!(base == lastBase && depth == lastDepth && pressure == lastPressure))
        {
            value = OriginalSubOscillator::circuitTargetRate(base, depth, pressure);
            lastBase = base;
            lastDepth = depth;
            lastPressure = pressure;
        }
        return value;
    }
private:
    double lastBase = std::numeric_limits<double>::quiet_NaN();
    double lastDepth = std::numeric_limits<double>::quiet_NaN();
    double lastPressure = std::numeric_limits<double>::quiet_NaN();
    double value = 0.0;
};
} // namespace cs80
