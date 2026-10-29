#pragma once

#include "Dsp.h"

namespace cs80
{
// Software anti-imaging reconstruction, not an analog input bandwidth model.
// Reuse the established255-tap Kaiser coefficient design, with independent
// fixed-size history. Zero insertion and gain2 retain unity passband gain.
// Group delay127 internal samples =63.5 host samples, EXTERNAL CONTROL ONLY.
// The synth's declared audio output latency remains63 host samples.
class ExternalModulationInterpolator
{
public:
    static constexpr double kHostDelay = (double) Decimator::kCenter / 2.0;

    void prepare(float hostRate)
    {
        filter.prepare(hostRate * 2.0f);
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

    std::array<float, 2> tick(float input) noexcept
    {
        if (!connected) return {0.0f, 0.0f};
        const float x = 2.0f * sanitise(input);
        // Finite FIR ringing can exceed1; do not silently add a second clipper.
        return {filter.process(x), filter.process(0.0f)};
    }

private:
    Decimator filter;
    bool connected = false;
};
} // namespace cs80
