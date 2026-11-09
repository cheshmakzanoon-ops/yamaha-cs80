// Frozen EnsembleFx from main a53c1a8b196d494dedd9f0438be173c1b0ecd28d (src/core/FxModels.h).
// Only the class name is changed. This preserves the pre-cache arithmetic.
#pragma once

#include "core/FxModels.h"

namespace cs80::fx::test
{
class EnsembleFxBaseline
{
public:
    static constexpr double kBaseDelay = 0.0060;
    static constexpr double kSlowHz = 0.63, kFastHz = 5.7;
    static constexpr double kSlowDepth = 0.0022, kFastDepth = 0.00035; // seconds at B = 1
    static constexpr float kBandwidthHz = 9000.0f;

    void prepare(double fs)
    {
        sr = fs;
        for (auto* line : { &lineL, &lineR })
            line->prepare(0.016f, (float) fs);
        antiAliasL = antiAliasR = Eq::lowPass(fs, kBandwidthHz, 0.7071);
        reconL = reconR = Eq::lowPass(fs, kBandwidthHz, 0.7071);
        reset();
    }
    void reset()
    {
        lineL.reset(); lineR.reset();
        antiAliasL.reset(); antiAliasR.reset(); reconL.reset(); reconR.reset();
        slowPhase = fastPhase = 0.0;
    }

    StereoSample process(float l, float r, float a, float b) noexcept
    {
        const double speed = std::exp2(2.0 * (double) clampf(a, 0.0f, 1.0f) - 1.0);
        slowPhase += kSlowHz * speed / sr;
        fastPhase += kFastHz * speed / sr;
        slowPhase -= std::floor(slowPhase);
        fastPhase -= std::floor(fastPhase);
        const double depth = 0.15 + 0.85 * (double) clampf(b, 0.0f, 1.0f);
        const double ss = std::sin(6.283185307179586 * slowPhase), sc = std::cos(6.283185307179586 * slowPhase);
        const double fs = std::sin(6.283185307179586 * fastPhase), fc = std::cos(6.283185307179586 * fastPhase);
        lineL.write(antiAliasL.process(l));
        lineR.write(antiAliasR.process(r));
        float wetL = 0.0f, wetR = 0.0f;
        // sin(x + k * 60 deg) by rotation: one sin/cos pair per LFO for all six taps.
        for (int k = 0; k < 6; ++k)
        {
            const double cs = kCos60[k], sn = kSin60[k];
            const double slow = ss * cs + sc * sn, fast = fs * cs + fc * sn;
            const double delay = (kBaseDelay + depth * (kSlowDepth * slow + kFastDepth * fast)) * sr;
            if (k % 2 == 0) wetL += lineL.readHermite(delay);
            else wetR += lineR.readHermite(delay);
        }
        const float gain = kTapGain * trim;
        return { headroom(reconL.process(wetL * gain)), headroom(reconR.process(wetR * gain)) };
    }

    void setTrim(float linear) noexcept { trim = linear; }

private:
    static float headroom(float x) noexcept { return std::tanh(x * 0.5f) * 2.0f; }
    static constexpr float kTapGain = 0.57735027f; // 1 / sqrt(3): three decorrelated taps
    static constexpr double kCos60[6] = { 1.0, 0.5, -0.5, -1.0, -0.5, 0.5 };
    static constexpr double kSin60[6] = { 0.0, 0.8660254037844386, 0.8660254037844386, 0.0,
                                          -0.8660254037844386, -0.8660254037844386 };
    DelayLine lineL, lineR;
    Eq antiAliasL, antiAliasR, reconL, reconR;
    double sr = 96000.0, slowPhase = 0.0, fastPhase = 0.0;
    float trim = 1.0f;
};
} // namespace cs80::fx::test
