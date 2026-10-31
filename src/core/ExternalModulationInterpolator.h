#pragma once

#include "Dsp.h"

namespace cs80
{
// Software anti-imaging reconstruction, not an analog input bandwidth model.
// Reuse the established Kaiser coefficient design of the current oversampling
// factor, with independent fixed-size history. Zero insertion and gain N retain
// unity passband gain. Group delay (64N - 1) internal samples: 63.5 host samples
// at 2x, 63.75 at 4x, 63.875 at 8x, EXTERNAL CONTROL ONLY. The synth's declared
// audio output latency remains 63 host samples at every factor.
class ExternalModulationInterpolator
{
public:
    static constexpr double kHostDelay = (double) Decimator::kCenter / 2.0; // 2x
    using Frame = std::array<float, OversamplingDecimator::kMaxFactor>;

    void prepare(float hostRate, int oversampling = 2)
    {
        (void) hostRate; // normalized design, like the output decimator
        filter.prepare(oversampling);
        reset();
    }

    void reset() noexcept
    {
        filter.reset();
        connected = false;
    }

    void setConnected(bool enabled) noexcept
    {
        if (enabled != connected)
        {
            filter.reset();
            connected = enabled;
        }
    }

    static float sanitise(float input) noexcept
    {
        // Explicit software protection, NOT measured input-stage clipping.
        return std::isfinite(input) ? clampf(input, -1.0f, 1.0f) : 0.0f;
    }

    int getFactor() const noexcept { return filter.getFactor(); }
    double getHostDelay() const noexcept
    { return (double) filter.getCenter() / (double) filter.getFactor(); }

    // The first getFactor() entries are this host frame's internal samples.
    Frame tick(float input) noexcept
    {
        Frame out {};
        if (!connected) return out;
        const int factor = filter.getFactor();
        const float x = static_cast<float>(factor) * sanitise(input);
        // Finite FIR ringing can exceed1; do not silently add a second clipper.
        out[0] = filter.process(x);
        for (int i = 1; i < factor; ++i)
            out[(size_t) i] = filter.process(0.0f);
        return out;
    }

private:
    OversamplingDecimator filter;
    bool connected = false;
};
} // namespace cs80
