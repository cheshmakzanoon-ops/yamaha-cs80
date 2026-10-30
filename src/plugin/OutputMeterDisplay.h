#pragma once

#include "OutputMeter.h"

#include <algorithm>
#include <cmath>

namespace cs80::ui
{
// Message-thread presentation only. No audio/parameter writes and no assumption
// that independently consumed peak/event fields describe one coherent block.
// The caller supplies monotonic wall time in seconds, not an audio frame count.
class OutputMeterDisplay
{
public:
    static constexpr float floorDbfs = -120.0f;
    static constexpr float floorGain = 0.000001f;
    static constexpr double releaseDbPerSecond = 12.0;
    static constexpr double overHoldSeconds = 1.0;
    static constexpr double nonFiniteHoldSeconds = 2.0;

    void reset(double nowSeconds) noexcept
    {
        peak = 0.0f;
        overRemaining = nonFiniteRemaining = 0.0;
        lastTime = std::isfinite(nowSeconds) ? nowSeconds : 0.0;
    }

    void update(OutputMeterReading reading, double nowSeconds) noexcept
    {
        // Backward/nonfinite clock samples cannot make decay run backwards or
        // extend a hold. A large forward jump expires stale presentation state.
        const double now = std::isfinite(nowSeconds) ? std::max(lastTime, nowSeconds) : lastTime;
        const double elapsed = now - lastTime;
        lastTime = now;
        const double release = std::pow(10.0, -releaseDbPerSecond * elapsed / 20.0);
        peak = static_cast<float>(static_cast<double>(peak) * release);
        overRemaining = std::max(0.0, overRemaining - elapsed);
        nonFiniteRemaining = std::max(0.0, nonFiniteRemaining - elapsed);

        // Production telemetry already excludes NaN/Inf from the numeric max.
        // Be defensive at the display boundary without ever replacing audio.
        const bool invalidPeak = !std::isfinite(reading.samplePeak);
        const float incoming = invalidPeak ? 0.0f : std::max(0.0f, reading.samplePeak);
        peak = std::max(peak, incoming);
        if (reading.overRange || incoming > 1.0f)
            overRemaining = overHoldSeconds;
        if (reading.nonFinite || invalidPeak)
            nonFiniteRemaining = nonFiniteHoldSeconds;
    }

    float samplePeak() const noexcept { return peak; }
    bool atFloor() const noexcept { return peak <= floorGain; }
    float peakDbfs() const noexcept
    {
        return atFloor() ? floorDbfs
            : static_cast<float>(20.0 * std::log10(static_cast<double>(peak)));
    }
    bool overRange() const noexcept { return overRemaining > 0.0; }
    bool nonFinite() const noexcept { return nonFiniteRemaining > 0.0; }

private:
    float peak = 0.0f;
    double lastTime = 0.0, overRemaining = 0.0, nonFiniteRemaining = 0.0;
};
} // namespace cs80::ui
