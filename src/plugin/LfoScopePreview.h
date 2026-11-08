#pragma once

#include "core/ModulationLfo.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace cs80::ui
{
struct LfoPreviewPoint
{
    float x;     // Fraction of the four-second preview window, [0, 1].
    float value; // Unit waveform, [-1, 1].
};

// A static shape preview, not audio-thread telemetry. Analytic shapes use the DSP's
// phase convention; random shapes use its hold/interpolation laws with a private,
// repeatable example sequence. Repainting never advances or reads the live LFO/RNG.
inline std::vector<LfoPreviewPoint> makeLfoScopePreview(int waveform, float rate)
{
    waveform = std::clamp(waveform, 0, kLfoWaveCount - 1);
    rate = std::isfinite(rate) ? std::clamp(rate, 0.05f, 20.0f) : 0.05f;
    const double cycles = 4.0 * static_cast<double>(rate);
    constexpr int stepsPerCycle = 64; // Includes every corner and discontinuity exactly.
    const int steps = static_cast<int>(std::floor(cycles * stepsPerCycle));

    std::array<float, 82> levels {}; // At most 80 periods plus the last interpolation endpoint.
    Rng rng(0x41c64e6du);
    for (auto& level : levels)
        level = rng.nextBipolar();
    const auto valueAt = [&](double position)
    {
        const int cycle = static_cast<int>(std::floor(position));
        const double phase = position - cycle;
        if (waveform == static_cast<int>(LfoWave::sampleHold))
            return levels[static_cast<size_t>(cycle)];
        if (waveform == static_cast<int>(LfoWave::smoothRandom))
        {
            const float from = levels[static_cast<size_t>(cycle)];
            const float to = levels[static_cast<size_t>(cycle + 1)];
            const float blend = 0.5f - 0.5f * static_cast<float>(std::cos(3.14159265358979323846 * phase));
            return from + (to - from) * blend;
        }
        return lfoShape(waveform, static_cast<float>(phase));
    };

    std::vector<LfoPreviewPoint> points;
    points.reserve(static_cast<size_t>(steps + 2 + 2 * std::ceil(cycles)));
    for (int step = 0; step <= steps; ++step)
    {
        const double position = static_cast<double>(step) / stepsPerCycle;
        const float x = static_cast<float>(position / cycles);
        const int phaseStep = step % stepsPerCycle;
        // Both sides of a jump share x, so square/S&H and saw resets stay vertical
        // instead of becoming sloped ramps at the display's sampling resolution.
        if (step > 0)
        {
            if (phaseStep == stepsPerCycle / 2 && waveform == static_cast<int>(LfoWave::sawUp))
                points.push_back({ x, 1.0f });
            if (phaseStep == stepsPerCycle / 2 && waveform == static_cast<int>(LfoWave::sawDown))
                points.push_back({ x, -1.0f });
            if (waveform == static_cast<int>(LfoWave::square) && (phaseStep == 0 || phaseStep == stepsPerCycle / 2))
                points.push_back({ x, phaseStep == 0 ? -1.0f : 1.0f });
            if (waveform == static_cast<int>(LfoWave::sampleHold) && phaseStep == 0)
                points.push_back({ x, levels[static_cast<size_t>(step / stepsPerCycle - 1)] });
        }
        points.push_back({ x, valueAt(position) });
    }
    if (static_cast<double>(steps) / stepsPerCycle < cycles)
        points.push_back({ 1.0f, valueAt(cycles) });
    return points;
}
} // namespace cs80::ui
