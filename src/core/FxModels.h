#pragma once

#include "Dsp.h"
#include "FxLoudness.h"
#include "FxOversampler.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

// Modern FX models for the three FX slots (IMPROVEMENT_PLAN Phase 6). None of these is a
// CS-80 circuit: the instrument's own effects are the OE1/OE2 chorus/tremolo and the PRA
// wah (ws4-circuits). Each model cites the published design it follows; every constant
// is a declared engineering choice. The classic slot effects (Dsp.h Chorus/Phaser/Reverb
// and the Engine.cpp tape/drive) stay byte-identical and remain selectable.
//
// Every model is statically loudness-calibrated: at full mix its K-weighted output
// loudness matches its input for pink noise at -18 dBFS RMS (tests/ModernFxTests.cpp).
// The reverbs and the echo use an equal-power dry/wet law (their wet is decorrelated from
// the dry); the ensemble and the drive cross-fade linearly (their wet is correlated).
namespace cs80::fx
{

enum class Model : int { classic = 0, ensemble, spring, plate, tape, drive };
inline constexpr int kModelCount = 6;
// Engine FxSlot::liveType code of a modern model (classic types are 1..5).
inline constexpr int kModernTypeBase = 100;

inline const char* modelName(int model) noexcept
{
    static constexpr const char* names[kModelCount] = { "Classic", "BBD ensemble", "Spring reverb",
                                                        "Plate reverb", "Tape echo", "Drive" };
    return model >= 0 && model < kModelCount ? names[model] : "Classic";
}
inline const char* paramAName(int model) noexcept
{
    static constexpr const char* names[kModelCount] = { "-", "Rate", "Decay", "Decay", "Time", "Drive" };
    return model >= 0 && model < kModelCount ? names[model] : "-";
}
inline const char* paramBName(int model) noexcept
{
    static constexpr const char* names[kModelCount] = { "-", "Depth", "Tone", "Brightness", "Repeats", "Tone" };
    return model >= 0 && model < kModelCount ? names[model] : "-";
}

struct StereoSample
{
    float l = 0.0f, r = 0.0f;
};

// RBJ audio-EQ-cookbook biquad (float, transposed direct form II) for in-model EQ.
struct Eq
{
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f, z1 = 0.0f, z2 = 0.0f;

    float process(float x) noexcept
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
    void reset() noexcept { z1 = z2 = 0.0f; }

    static Eq make(double b0, double b1, double b2, double a0, double a1, double a2) noexcept
    {
        Eq e;
        e.b0 = (float) (b0 / a0); e.b1 = (float) (b1 / a0); e.b2 = (float) (b2 / a0);
        e.a1 = (float) (a1 / a0); e.a2 = (float) (a2 / a0);
        return e;
    }
    static double omega(double fs, double hz) noexcept
    {
        return 2.0 * 3.14159265358979323846 * std::clamp(hz, 1.0, 0.45 * fs) / fs;
    }
    static Eq lowPass(double fs, double hz, double q) noexcept
    {
        const double w = omega(fs, hz), c = std::cos(w), alpha = std::sin(w) / (2.0 * q);
        return make((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + alpha, -2 * c, 1 - alpha);
    }
    static Eq highPass(double fs, double hz, double q) noexcept
    {
        const double w = omega(fs, hz), c = std::cos(w), alpha = std::sin(w) / (2.0 * q);
        return make((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + alpha, -2 * c, 1 - alpha);
    }
    static Eq peak(double fs, double hz, double q, double db) noexcept
    {
        const double w = omega(fs, hz), c = std::cos(w), alpha = std::sin(w) / (2.0 * q), A = std::pow(10.0, db / 40.0);
        return make(1 + alpha * A, -2 * c, 1 - alpha * A, 1 + alpha / A, -2 * c, 1 - alpha / A);
    }
    static Eq highShelf(double fs, double hz, double db) noexcept
    {
        const double w = omega(fs, hz), c = std::cos(w), A = std::pow(10.0, db / 40.0);
        const double alpha = std::sin(w) / 2.0 * std::sqrt(2.0), s = 2.0 * std::sqrt(A) * alpha;
        return make(A * ((A + 1) + (A - 1) * c + s), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - s),
                    (A + 1) - (A - 1) * c + s, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - s);
    }
};

// Static loudness calibration of a model's wet path, in dB, on a 5 x 5 grid of the A and B
// controls (0, .25, .5, .75, 1), interpolated bilinearly. Measured on the models' own
// output (pink noise, -18 dBFS RMS, K-weighted, 96 kHz): a calibration of a modern effect,
// not a CS-80 measurement. Regenerate with `cs80_modern_fx_tests --calibrate`.
using CalibrationTable = std::array<std::array<float, 5>, 5>; // [a][b]
inline float calibrationDb(const CalibrationTable& t, float a, float b) noexcept
{
    const float x = clampf(a, 0.0f, 1.0f) * 4.0f, y = clampf(b, 0.0f, 1.0f) * 4.0f;
    const int i = std::min((int) x, 3), j = std::min((int) y, 3);
    const float u = x - (float) i, v = y - (float) j;
    const float top = lerp(t[(size_t) i][(size_t) j], t[(size_t) i][(size_t) j + 1], v);
    const float bottom = lerp(t[(size_t) i + 1][(size_t) j], t[(size_t) i + 1][(size_t) j + 1], v);
    return lerp(top, bottom, u);
}
inline float dbToGain(float db) noexcept { return std::pow(10.0f, db / 20.0f); }

// ---- BBD ensemble ---------------------------------------------------------------------
// Three-phase multi-tap ensemble in the manner of the 1970s string-ensemble BBD units:
// three delay taps per channel, each swept by a slow chorus LFO and a fast vibrato LFO
// whose phases are spaced 120 degrees; the right channel's taps sit 60 degrees from the
// left's. Bucket-brigade character is a 9 kHz anti-alias and reconstruction low-pass pair
// and a soft headroom limit. A = rate (0.5x..2x of 0.63 Hz / 5.7 Hz), B = depth. Separate
// from the original OE2 chorus (src/core/BbdChorus.h, ws4-circuits).
class EnsembleFx
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

// ---- Plate reverb ----------------------------------------------------------------------
// J. Dattorro, "Effect Design, Part 1: Reverberator and Other Filters", J. Audio Eng. Soc.
// 45(9), 1997: the plate-class figure-of-eight tank with its published delay lengths
// (at 29761 Hz, scaled here to the running rate), input/decay diffusion 0.75/0.625 and
// 0.70/0.50, two modulated tank all-passes (16 samples at 29761 Hz, ~1 Hz) and the
// published seven-tap stereo outputs; decay diffusion 2 follows the decay as the paper
// specifies (decay + 0.15, clipped to 0.25..0.50). A = decay (RT60 0.8..8 s at 1 kHz with
// B = 1: the fixed 0.70 tank all-passes ring for ~0.4 s by themselves, so the structure
// cannot honour shorter times), B = brightness (input bandwidth and in-tank damping).
class PlateReverbFx
{
public:
    static constexpr double kDesignRate = 29761.0;

    static double targetRt60(float a) noexcept { return 0.8 * std::pow(10.0, (double) clampf(a, 0.0f, 1.0f)); }
    // Tank loop: both halves' all-passes and delays, in seconds.
    static constexpr double kLoopSeconds = (672.0 + 4453.0 + 1800.0 + 3720.0 + 908.0 + 4217.0 + 2656.0 + 3163.0) / kDesignRate;
    // Four decay multiplications per figure-eight loop: g^4 per loop -> 60 dB in RT60.
    static double decayFor(double rt60) noexcept { return std::pow(10.0, -3.0 * kLoopSeconds / (4.0 * rt60)); }

    void prepare(double fs)
    {
        sr = fs;
        scale = fs / kDesignRate;
        auto len = [this](double n) { return std::max(4, (int) std::lround(n * scale)); };
        excursion = 16.0 * scale;
        const int inputLengths[4] = { len(142), len(107), len(379), len(277) };
        for (int i = 0; i < 4; ++i)
        {
            inputDiffusers[(size_t) i].length = inputLengths[i];
            inputDiffusers[(size_t) i].line.prepare((float) (inputLengths[i] + 4) / (float) fs, (float) fs);
        }
        tankAp1[0].length = len(672);  tankAp1[1].length = len(908);
        tankAp2[0].length = len(1800); tankAp2[1].length = len(2656);
        delay1Length = { len(4453), len(4217) };
        delay2Length = { len(3720), len(3163) };
        for (int h = 0; h < 2; ++h)
        {
            tankAp1[(size_t) h].line.prepare((float) (tankAp1[(size_t) h].length + excursion + 8) / (float) fs, (float) fs);
            tankAp2[(size_t) h].line.prepare((float) (tankAp2[(size_t) h].length + 4) / (float) fs, (float) fs);
            delay1[(size_t) h].prepare((float) (delay1Length[(size_t) h] + 4) / (float) fs, (float) fs);
            delay2[(size_t) h].prepare((float) (delay2Length[(size_t) h] + 4) / (float) fs, (float) fs);
        }
        predelay.prepare(0.012f, (float) fs);
        predelaySamples = (int) std::lround(0.010 * fs);
        // Output taps (Dattorro Table 2), scaled.
        const int left[7] = { 266, 2974, 1913, 1996, 1990, 187, 1066 };
        const int right[7] = { 353, 3627, 1228, 2673, 2111, 335, 121 };
        for (int i = 0; i < 7; ++i)
        {
            tapsL[(size_t) i] = len(left[i]);
            tapsR[(size_t) i] = len(right[i]);
        }
        reset();
    }

    void reset()
    {
        for (auto& d : inputDiffusers) d.line.reset();
        for (auto& d : tankAp1) d.line.reset();
        for (auto& d : tankAp2) d.line.reset();
        for (auto& d : delay1) d.reset();
        for (auto& d : delay2) d.reset();
        predelay.reset();
        bandwidthState = 0.0f;
        damping = { 0.0f, 0.0f };
        feedback = { 0.0f, 0.0f };
        lfoPhase = 0.0;
    }

    StereoSample process(float l, float r, float a, float b) noexcept
    {
        const float bright = clampf(b, 0.0f, 1.0f);
        const float bandwidth = 0.45f + 0.5495f * bright; // Dattorro default 0.9995 at B = 1
        const float damp = 0.6f * (1.0f - bright);        // 0 (none) at B = 1
        if (a != lastDecayControl)
        {
            lastDecayControl = a;
            decay = (float) decayFor(targetRt60(a));
            decayDiffusion2 = clampf(decay + 0.15f, 0.25f, 0.5f);
        }

        predelay.write(0.5f * (l + r));
        float x = predelay.tap(predelaySamples);
        bandwidthState += bandwidth * (x - bandwidthState);
        x = bandwidthState;
        x = inputDiffusers[0].process(x, 0.75f);
        x = inputDiffusers[1].process(x, 0.75f);
        x = inputDiffusers[2].process(x, 0.625f);
        x = inputDiffusers[3].process(x, 0.625f);

        lfoPhase += 1.0 / sr;
        lfoPhase -= std::floor(lfoPhase);
        const double mod[2] = { excursion * std::sin(6.283185307179586 * lfoPhase),
                                excursion * std::cos(6.283185307179586 * lfoPhase) };
        std::array<float, 2> next {};
        for (int h = 0; h < 2; ++h)
        {
            float y = x + feedback[(size_t) (1 - h)];
            y = tankAp1[(size_t) h].processModulated(y, -0.70f, (double) tankAp1[(size_t) h].length + mod[h]);
            delay1[(size_t) h].write(y);
            y = delay1[(size_t) h].tap(delay1Length[(size_t) h]);
            damping[(size_t) h] += (1.0f - damp) * (y - damping[(size_t) h]);
            y = damping[(size_t) h] * decay;
            y = tankAp2[(size_t) h].process(y, decayDiffusion2);
            delay2[(size_t) h].write(y);
            next[(size_t) h] = delay2[(size_t) h].tap(delay2Length[(size_t) h]) * decay;
        }
        feedback = next;

        // Half 0 = Dattorro's left tank (nodes 24, 31, 33), half 1 = right (48, 55, 59).
        const float outL = delay1[1].tap(tapsL[0]) + delay1[1].tap(tapsL[1]) - tankAp2[1].line.tap(tapsL[2])
                           + delay2[1].tap(tapsL[3]) - delay1[0].tap(tapsL[4]) - tankAp2[0].line.tap(tapsL[5])
                           - delay2[0].tap(tapsL[6]);
        const float outR = delay1[0].tap(tapsR[0]) + delay1[0].tap(tapsR[1]) - tankAp2[0].line.tap(tapsR[2])
                           + delay2[0].tap(tapsR[3]) - delay1[1].tap(tapsR[4]) - tankAp2[1].line.tap(tapsR[5])
                           - delay2[1].tap(tapsR[6]);
        // Energy normalisation for the tank's recirculation (approximately 1 - g^2 per
        // quarter loop), then the measured calibration trim.
        const float norm = 0.6f * std::sqrt(std::max(0.0f, 1.0f - decay * decay)) * trim;
        return { outL * norm, outR * norm };
    }

    void setTrim(float linear) noexcept { trim = linear; }

private:
    struct Allpass
    {
        DelayLine line;
        int length = 1;
        // Schroeder all-pass: w = x + g d, y = d - g w, d = w delayed by `length`.
        float process(float x, float g) noexcept
        {
            const float d = line.tap(length);
            const float w = x + g * d;
            line.write(w);
            return d - g * w;
        }
        float processModulated(float x, float g, double delay) noexcept
        {
            const float d = line.readHermite(delay);
            const float w = x + g * d;
            line.write(w);
            return d - g * w;
        }
    };
    std::array<Allpass, 4> inputDiffusers;
    std::array<Allpass, 2> tankAp1, tankAp2;
    std::array<DelayLine, 2> delay1, delay2;
    std::array<int, 2> delay1Length {}, delay2Length {};
    std::array<int, 7> tapsL {}, tapsR {};
    DelayLine predelay;
    int predelaySamples = 1;
    std::array<float, 2> damping {}, feedback {};
    float bandwidthState = 0.0f;
    double sr = 96000.0, scale = 1.0, excursion = 16.0, lfoPhase = 0.0;
    float trim = 1.0f, decay = 0.0f, decayDiffusion2 = 0.5f, lastDecayControl = -1.0f;
};

// ---- Spring reverb ---------------------------------------------------------------------
// Dispersive spring model after V. Valimaki, J. Parker and J. S. Abel, "Parametric spring
// reverberation effect", J. Audio Eng. Soc. 58(7/8), 2010 (and Abel, Berners, Costello and
// Smith, AES 121st Convention, 2006): two springs, each a feedback loop of a cascade of
// stretched first-order all-passes H(z) = (a + z^-K) / (1 + a z^-K), K = fs / (2 fC), a
// transit delay and a low-pass at the transition frequency fC. With a < 0 low frequencies
// travel slower, so every round trip is a descending chirp. The high-frequency chirp path
// of the paper is omitted (declared simplification). A = decay (RT60 0.8..4.8 s at
// 1 kHz), B = tone (output low-pass 2..8 kHz).
class SpringReverbFx
{
public:
    static constexpr double kTransitionHz = 4300.0;
    static constexpr int kSections = 48;
    static constexpr float kAllpassCoef = -0.6f;
    static constexpr double kTransitSeconds[2] = { 0.0371, 0.0439 };

    static double targetRt60(float a) noexcept { return 0.8 * std::pow(6.0, (double) clampf(a, 0.0f, 1.0f)); }

    void prepare(double fs)
    {
        sr = fs;
        stretch = std::max(1, (int) std::lround(fs / (2.0 * kTransitionHz)));
        for (int s = 0; s < 2; ++s)
        {
            auto& spring = springs[(size_t) s];
            spring.state.assign((size_t) (kSections * stretch), 0.0f);
            spring.loop.prepare((float) (kTransitSeconds[s] + 0.004), (float) fs);
            spring.lowPass = Eq::lowPass(fs, kTransitionHz, 0.7071);
            spring.highPass = Eq::highPass(fs, 60.0, 0.5);
            // Loop time at 1 kHz: transit plus the dispersion cascade's group delay there.
            const double w = 2.0 * 3.14159265358979323846 * 1000.0 * (double) stretch / fs;
            const double a = kAllpassCoef;
            const double prototype = (1.0 - a * a) / (1.0 + 2.0 * a * std::cos(w) + a * a);
            spring.loopSeconds = kTransitSeconds[s] + (double) kSections * (double) stretch * prototype / fs;
        }
        inputHighPass = Eq::highPass(fs, 100.0, 0.7071);
        outputHighPass[0] = outputHighPass[1] = Eq::highPass(fs, 150.0, 0.7071);
        outputPeak[0] = outputPeak[1] = Eq::peak(fs, 1800.0, 0.9, 3.0);
        reset();
    }

    void reset()
    {
        for (auto& spring : springs)
        {
            std::fill(spring.state.begin(), spring.state.end(), 0.0f);
            spring.loop.reset();
            spring.lowPass.reset();
            spring.highPass.reset();
            spring.feedback = 0.0f;
            spring.drift = 0.0f;
        }
        index = 0;
        inputHighPass.reset();
        for (int c = 0; c < 2; ++c) { outputHighPass[c].reset(); outputPeak[c].reset(); outputLowPass[c].reset(); }
        rng = Rng(0x5eed5917u);
        lastTone = -1.0f;
        lastDecayControl = -1.0f;
    }

    StereoSample process(float l, float r, float a, float b) noexcept
    {
        const float tone = clampf(b, 0.0f, 1.0f);
        if (tone != lastTone)
        {
            lastTone = tone;
            const double hz = 2000.0 * std::pow(4.0, (double) tone);
            const auto z0 = outputLowPass[0], z1 = outputLowPass[1];
            outputLowPass[0] = outputLowPass[1] = Eq::lowPass(sr, hz, 0.7071);
            outputLowPass[0].z1 = z0.z1; outputLowPass[0].z2 = z0.z2;
            outputLowPass[1].z1 = z1.z1; outputLowPass[1].z2 = z1.z2;
        }
        if (a != lastDecayControl)
        {
            lastDecayControl = a;
            for (auto& spring : springs)
                spring.gain = (float) std::pow(10.0, -3.0 * spring.loopSeconds / targetRt60(a));
        }
        float in = inputHighPass.process(0.5f * (l + r));
        in = std::tanh(1.5f * in) / 1.5f; // transducer drive

        std::array<float, 2> out {};
        for (int s = 0; s < 2; ++s)
        {
            auto& spring = springs[(size_t) s];
            const float g = spring.gain;
            float x = 0.5f * in + spring.feedback;
            // Stretched all-pass cascade sharing one ring position: w = x - a w[n-K],
            // y = a w + w[n-K].
            float* state = spring.state.data();
            for (int k = 0; k < kSections; ++k)
            {
                float& slot = state[(size_t) (k * stretch + index)];
                const float delayed = slot;
                const float w = x - kAllpassCoef * delayed;
                slot = w;
                x = kAllpassCoef * w + delayed;
            }
            spring.loop.write(x);
            // Slow random wander of the transit time (+/- 0.15 ms): breaks static ringing.
            spring.drift += 0.0005f * (rng.nextBipolar() - spring.drift * 0.02f);
            const double delay = (kTransitSeconds[s] + 0.00015 * (double) clampf(spring.drift, -1.0f, 1.0f)) * sr;
            const float y = spring.loop.readHermite(delay);
            spring.feedback = g * spring.highPass.process(spring.lowPass.process(y));
            out[(size_t) s] = y;
        }
        if (++index >= stretch)
            index = 0;
        float wetL = out[0] + 0.35f * out[1], wetR = out[1] + 0.35f * out[0];
        wetL = outputLowPass[0].process(outputPeak[0].process(outputHighPass[0].process(wetL)));
        wetR = outputLowPass[1].process(outputPeak[1].process(outputHighPass[1].process(wetR)));
        return { wetL * trim, wetR * trim };
    }

    void setTrim(float linear) noexcept { trim = linear; }
    double loopSeconds(int spring) const noexcept { return springs[(size_t) std::clamp(spring, 0, 1)].loopSeconds; }

private:
    struct Spring
    {
        std::vector<float> state;
        DelayLine loop;
        Eq lowPass, highPass;
        double loopSeconds = 0.05;
        float feedback = 0.0f, drift = 0.0f, gain = 0.0f;
    };
    std::array<Spring, 2> springs;
    int stretch = 11, index = 0;
    Eq inputHighPass;
    std::array<Eq, 2> outputHighPass, outputPeak, outputLowPass;
    Rng rng { 0x5eed5917u };
    double sr = 96000.0;
    float lastTone = -1.0f, trim = 1.0f, lastDecayControl = -1.0f;
};

// ---- Tape echo -----------------------------------------------------------------------
// Tape loop with a record head, one playback head and feedback. The transport law is
// physical: with tape speed v0 (1 + s(t)), the tape under the playback head was recorded
// D(t) earlier where  integral over [t - D, t] of (1 + s) = T. For each sinusoidal speed
// component s = A sin(w t) that gives  D(t) = T - (A / w) [cos(w (t - T)) - cos(w t)], so
// the pitch deviation is s(t) - s(t - T): small for short delays, up to 2A for long ones,
// as on a real machine. Wow 0.55 Hz / 0.18 %, flutter 6.1 Hz / 0.04 % (declared). Record
// pre-emphasis, tape saturation, playback de-emphasis, head-gap high-frequency loss and a
// loop high-pass are in the loop (loop gain never above the repeat gain); the head bump
// is on the output only. A = time (60 ms..1.2 s, glides like a tape-speed change),
// B = repeats as the echo train's 60 dB decay time (0.3..8 s).
class TapeEchoFx
{
public:
    static constexpr double kWowHz = 0.55, kWowDepth = 0.0018, kFlutterHz = 6.1, kFlutterDepth = 0.0004;
    static constexpr double kMaxSeconds = 1.2;

    static double delaySeconds(float a) noexcept { return 0.06 * std::pow(20.0, (double) clampf(a, 0.0f, 1.0f)); }
    static double tailSeconds(float b) noexcept { return 0.3 * std::pow(8.0 / 0.3, (double) clampf(b, 0.0f, 1.0f)); }

    void prepare(double fs)
    {
        sr = fs;
        for (auto& c : channels)
        {
            c.line.prepare((float) (kMaxSeconds + 0.02), (float) fs);
            c.preEmphasis = Eq::highShelf(fs, 3000.0, 6.0);
            c.deEmphasis = Eq::highShelf(fs, 3000.0, -6.0);
            c.headLoss = Eq::lowPass(fs, 7000.0, 0.6);
            c.loopHighPass = Eq::highPass(fs, 50.0, 0.6);
            c.headBump = Eq::peak(fs, 90.0, 1.0, 1.5);
        }
        glide = 1.0 - std::exp(-1.0 / (0.25 * fs));
        reset();
    }

    void reset()
    {
        for (int i = 0; i < 2; ++i)
        {
            auto& c = channels[(size_t) i];
            c.line.reset();
            for (auto* e : { &c.preEmphasis, &c.deEmphasis, &c.headLoss, &c.loopHighPass, &c.headBump })
                e->reset();
            c.playback = 0.0f;
            c.wow = i == 0 ? 0.0 : 0.37;
            c.flutter = i == 0 ? 0.0 : 0.61;
        }
        time = -1.0;
    }

    StereoSample process(float l, float r, float a, float b) noexcept
    {
        const double target = delaySeconds(a);
        time = time < 0.0 ? target : time + glide * (target - time);
        const float g = (float) std::min(0.95, std::pow(10.0, -3.0 * time / tailSeconds(b)));
        const float in[2] = { l, r };
        const float cross[2] = { channels[1].playback, channels[0].playback }; // ping-pong
        StereoSample out;
        for (int i = 0; i < 2; ++i)
        {
            auto& c = channels[(size_t) i];
            c.wow += kWowHz / sr;
            c.flutter += kFlutterHz / sr;
            c.wow -= std::floor(c.wow);
            c.flutter -= std::floor(c.flutter);
            const double D = time - transport(kWowDepth, kWowHz, c.wow, time) - transport(kFlutterDepth, kFlutterHz, c.flutter, time);
            float record = c.preEmphasis.process(in[i] + g * cross[i]);
            record = std::tanh(0.9f * record) / 0.9f;
            c.line.write(record);
            float y = c.line.readHermite(D * sr);
            y = c.loopHighPass.process(c.deEmphasis.process(c.headLoss.process(y)));
            c.playback = y;
            (i == 0 ? out.l : out.r) = c.headBump.process(y) * trim;
        }
        return out;
    }

    void setTrim(float linear) noexcept { trim = linear; }

private:
    // (A / w) [cos(w (t - T)) - cos(w t)] with w t = 2 pi phase.
    static double transport(double depth, double hz, double phase, double T) noexcept
    {
        const double w = 6.283185307179586 * hz;
        const double theta = 6.283185307179586 * phase;
        return depth / w * (std::cos(theta - w * T) - std::cos(theta));
    }
    struct Channel
    {
        DelayLine line;
        Eq preEmphasis, deEmphasis, headLoss, loopHighPass, headBump;
        float playback = 0.0f;
        double wow = 0.0, flutter = 0.0;
    };
    std::array<Channel, 2> channels;
    double sr = 96000.0, glide = 0.0, time = -1.0;
    float trim = 1.0f;
};

// ---- Drive -----------------------------------------------------------------------------
// Asymmetric tanh stage with per-slot oversampling (1x..8x of the internal rate, through
// the half-band cascade in FxOversampler.h): y = [tanh(g x + beta) - tanh(beta)] /
// (g sech^2(beta)), so small signals pass at unity gain; beta = 0.2 adds even harmonics.
// A = drive (0..30 dB), B = tone (post low-pass 1.5..18 kHz). A 10 Hz DC blocker removes
// the asymmetry's offset. The slot adds a K-weighted loudness follower, so the drive is
// loudness-compensated at every setting and input level.
class DriveFx
{
public:
    static constexpr float kAsymmetry = 0.2f;

    static float gainFor(float a) noexcept { return std::pow(10.0f, 1.5f * clampf(a, 0.0f, 1.0f)); }

    static float shape(float x, float g) noexcept
    {
        const float t = std::tanh(kAsymmetry);
        return (std::tanh(g * x + kAsymmetry) - t) / (g * (1.0f - t * t));
    }

    void prepare(double fs)
    {
        sr = fs;
        dcCoef = (float) std::exp(-6.283185307179586 * 10.0 / fs);
        reset();
    }
    void reset()
    {
        for (auto& o : oversampler) o.reset();
        dcIn = dcOut = { 0.0f, 0.0f };
        toneState = { 0.0f, 0.0f };
    }

    // Shaper core only (no DC blocker or tone): one channel through the oversampler.
    float processCore(int channel, float x, float g, int stages) noexcept
    {
        return oversampler[(size_t) (channel & 1)].process(x, stages, [g](float v) { return shape(v, g); });
    }

    StereoSample process(float l, float r, float a, float b, int stages) noexcept
    {
        const float g = gainFor(a);
        const float toneHz = 1500.0f * std::pow(12.0f, clampf(b, 0.0f, 1.0f));
        const float toneCoef = 1.0f - (float) std::exp(-6.283185307179586 * std::min((double) toneHz, 0.45 * sr) / sr);
        const float in[2] = { l, r };
        float out[2] {};
        for (int c = 0; c < 2; ++c)
        {
            const float y = processCore(c, in[c], g, stages);
            const float blocked = y - dcIn[(size_t) c] + dcCoef * dcOut[(size_t) c];
            dcIn[(size_t) c] = y;
            dcOut[(size_t) c] = blocked;
            toneState[(size_t) c] += toneCoef * (blocked - toneState[(size_t) c]);
            out[c] = toneState[(size_t) c];
        }
        return { out[0], out[1] };
    }

private:
    std::array<Oversampler, 2> oversampler;
    std::array<float, 2> dcIn {}, dcOut {}, toneState {};
    double sr = 96000.0;
    float dcCoef = 0.0f;
};

// Measured static calibration (dB) of each model's wet path; see calibrationDb above.
inline constexpr CalibrationTable kEnsembleCalibrationDb {{
    {{ 0.66f, 1.43f, 1.78f, 1.98f, 2.13f }},
    {{ 0.64f, 1.38f, 1.70f, 1.95f, 2.09f }},
    {{ 0.65f, 1.39f, 1.70f, 1.93f, 2.08f }},
    {{ 0.68f, 1.44f, 1.78f, 1.99f, 2.12f }},
    {{ 0.70f, 1.48f, 1.79f, 2.01f, 2.12f }}
}};
inline constexpr CalibrationTable kSpringCalibrationDb {{
    {{ 11.06f, 10.07f, 9.34f, 8.81f, 8.40f }},
    {{ 9.81f, 8.86f, 8.19f, 7.72f, 7.38f }},
    {{ 8.41f, 7.51f, 6.90f, 6.50f, 6.22f }},
    {{ 6.95f, 6.10f, 5.55f, 5.21f, 4.98f }},
    {{ 5.49f, 4.68f, 4.19f, 3.91f, 3.73f }}
}};
inline constexpr CalibrationTable kPlateCalibrationDb {{
    {{ 4.52f, 3.99f, 3.55f, 3.17f, 2.80f }},
    {{ 4.42f, 3.85f, 3.39f, 2.97f, 2.55f }},
    {{ 4.37f, 3.77f, 3.27f, 2.80f, 2.31f }},
    {{ 4.44f, 3.80f, 3.25f, 2.73f, 2.16f }},
    {{ 4.65f, 3.95f, 3.36f, 2.79f, 2.13f }}
}};
inline constexpr CalibrationTable kTapeCalibrationDb {{
    {{ 2.79f, 1.86f, 0.27f, -1.40f, -2.68f }},
    {{ 3.00f, 2.76f, 1.78f, 0.19f, -1.44f }},
    {{ 3.01f, 2.99f, 2.69f, 1.65f, 0.06f }},
    {{ 3.02f, 3.02f, 3.00f, 2.63f, 1.47f }},
    {{ 3.05f, 3.05f, 3.05f, 3.01f, 2.57f }}
}};

// One FX slot's set of modern models (each model owns its state, like FxSlot's classic set).
class ModernFxSlot
{
public:
    void prepare(double fs)
    {
        ensemble.prepare(fs);
        spring.prepare(fs);
        plate.prepare(fs);
        tape.prepare(fs);
        drive.prepare(fs);
        driveMatch.prepare(fs);
        used = 0;
    }
    // Only models that have run hold audio: clearing the others would zero megabytes of
    // already-silent delay memory on the audio thread (a render-policy change resets).
    void reset()
    {
        for (int model = 1; model < kModelCount; ++model)
            clear(model);
    }
    // Drop the state of one model (it may hold audio) so it never replays stale sound.
    void clear(int model)
    {
        const std::uint32_t bit = 1u << (model & 31);
        if ((used & bit) == 0)
            return;
        used &= ~bit;
        switch ((Model) model)
        {
            case Model::ensemble: ensemble.reset(); break;
            case Model::spring: spring.reset(); break;
            case Model::plate: plate.reset(); break;
            case Model::tape: tape.reset(); break;
            case Model::drive: drive.reset(); driveMatch.reset(); break;
            default: break;
        }
    }

    static const CalibrationTable* calibration(int model) noexcept
    {
        switch ((Model) model)
        {
            case Model::ensemble: return &kEnsembleCalibrationDb;
            case Model::spring: return &kSpringCalibrationDb;
            case Model::plate: return &kPlateCalibrationDb;
            case Model::tape: return &kTapeCalibrationDb;
            default: return nullptr;
        }
    }

    // Wet only (mix = 1, no dry), for calibration and tests.
    StereoSample wet(int model, float l, float r, float a, float b, int stages) noexcept
    {
        used |= 1u << (model & 31);
        if (model != trimModel || a != trimA || b != trimB)
        {
            trimModel = model;
            trimA = a;
            trimB = b;
            const auto* table = calibration(model);
            trimGain = table != nullptr ? dbToGain(calibrationDb(*table, a, b)) : 1.0f;
        }
        switch ((Model) model)
        {
            case Model::ensemble:
                ensemble.setTrim(trimGain);
                return ensemble.process(l, r, a, b);
            case Model::spring:
                spring.setTrim(trimGain);
                return spring.process(l, r, a, b);
            case Model::plate:
                plate.setTrim(trimGain);
                return plate.process(l, r, a, b);
            case Model::tape:
                tape.setTrim(trimGain);
                return tape.process(l, r, a, b);
            case Model::drive:
            {
                auto w = drive.process(l, r, a, b, stages);
                const float g = driveMatch.process(l, r, w.l, w.r);
                return { w.l * g, w.r * g };
            }
            default: return { l, r };
        }
    }

    StereoSample process(int model, float l, float r, float mix, float a, float b, int stages) noexcept
    {
        const auto w = wet(model, l, r, a, b, stages);
        mix = clampf(mix, 0.0f, 1.0f);
        if (model == (int) Model::ensemble || model == (int) Model::drive)
            return { lerp(l, w.l, mix), lerp(r, w.r, mix) };
        const float dry = std::cos(1.5707963267948966f * mix), wetGain = std::sin(1.5707963267948966f * mix);
        return { l * dry + w.l * wetGain, r * dry + w.r * wetGain };
    }

    EnsembleFx ensemble;
    SpringReverbFx spring;
    PlateReverbFx plate;
    TapeEchoFx tape;
    DriveFx drive;
    LoudnessMatcher driveMatch;

private:
    std::uint32_t used = 0; // models that have processed audio since they were last cleared
    int trimModel = -1;
    float trimA = -1.0f, trimB = -1.0f, trimGain = 1.0f;
};

} // namespace cs80::fx
