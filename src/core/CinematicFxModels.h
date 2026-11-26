#pragma once

#include "Dsp.h"
#include "FxModels.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

// Cinematic / studio FX suite for CS-80 Reference (4-slot serial FX rack).
// Pure DSP, zero allocations on the audio thread, denormal-safe, bounded feedback,
// cached control-rate transcendentals for ultra-low real-time CPU, and loudness-balanced
// dry/wet laws. Appended as ExtModel 1..7 so all existing classic and modern FX models
// (Model 0..5) remain byte-identical.
namespace cs80::fx
{

enum class ExtModel : int
{
    none = 0,
    hall = 1,       // Lush Hall Reverb (8-line modulated FDN cinema hall)
    shimmer = 2,    // Shimmer Reverb (modulated FDN + dual-grain octave/fifth pitch shifter)
    pingPong = 3,   // Stereo / Ping-Pong Delay (tempo-syncable, filtered feedback)
    dimChorus = 4,  // Dimension Stereo Chorus (4-voice quadrature warm chorus)
    flanger = 5,    // Analog Stereo Flanger (bipolar feedback, stereo LFO)
    studioEq = 6,   // Studio EQ & Stereo Widener (3-band musical EQ + bass-safe M/S width)
    busComp = 7     // Bus Compressor / Glue Limiter (linked stereo VCA glue + warmth)
};

inline constexpr int kExtModelCount = 8;
inline constexpr int kExtTypeBase = 200; // FxSlot::liveType = kExtTypeBase + (int)ExtModel

inline const char* extModelName(int extModel) noexcept
{
    static constexpr const char* names[kExtModelCount] = {
        "Standard",
        "Lush Hall",
        "Shimmer Reverb",
        "Ping-Pong Delay",
        "Dimension Chorus",
        "Analog Flanger",
        "Studio EQ & Width",
        "Bus Compressor"
    };
    return extModel >= 0 && extModel < kExtModelCount ? names[extModel] : "Standard";
}

inline const char* extParamAName(int extModel) noexcept
{
    static constexpr const char* names[kExtModelCount] = {
        "-", "Decay", "Decay", "Time", "Rate", "Rate", "Low", "Amount"
    };
    return extModel >= 0 && extModel < kExtModelCount ? names[extModel] : "-";
}

inline const char* extParamBName(int extModel) noexcept
{
    static constexpr const char* names[kExtModelCount] = {
        "-", "Size", "Shimmer", "Feedback", "Depth", "Depth", "Presence", "Punch"
    };
    return extModel >= 0 && extModel < kExtModelCount ? names[extModel] : "-";
}

inline const char* extParamCName(int extModel) noexcept
{
    static constexpr const char* names[kExtModelCount] = {
        "-", "Damping", "Tone", "Tone", "Width", "Feedback", "Air", "Ratio"
    };
    return extModel >= 0 && extModel < kExtModelCount ? names[extModel] : "-";
}

inline const char* extParamDName(int extModel) noexcept
{
    static constexpr const char* names[kExtModelCount] = {
        "-", "Pre-Delay", "5th / Mod", "Spread", "Warmth", "Manual", "Width", "Warmth"
    };
    return extModel >= 0 && extModel < kExtModelCount ? names[extModel] : "-";
}

inline bool extSupportsSync(int extModel) noexcept
{
    return extModel == (int) ExtModel::pingPong || extModel == (int) ExtModel::flanger;
}

inline float flushDenormal(float x) noexcept
{
    return (std::fabs(x) < 1.0e-20f) ? 0.0f : x;
}

inline float softCeiling(float x) noexcept
{
    // Fast smooth headroom ceiling: linear below 0.9, soft-saturated above.
    const float ax = std::fabs(x);
    if (ax < 0.85f) return x;
    return std::tanh(x * 0.5f) * 2.0f;
}

inline Eq makeLowShelf(double fs, double hz, double db) noexcept
{
    const double w = Eq::omega(fs, hz);
    const double c = std::cos(w);
    const double A = std::pow(10.0, db / 40.0);
    const double alpha = std::sin(w) / 2.0 * std::sqrt(2.0);
    const double s = 2.0 * std::sqrt(A) * alpha;
    return Eq::make(
        A * ((A + 1.0) - (A - 1.0) * c + s),
        2.0 * A * ((A - 1.0) - (A + 1.0) * c),
        A * ((A + 1.0) - (A - 1.0) * c - s),
        (A + 1.0) + (A - 1.0) * c + s,
        -2.0 * ((A - 1.0) + (A + 1.0) * c),
        (A + 1.0) + (A - 1.0) * c - s);
}

// Musical note divisions in beats (quarter notes) for tempo-synced FX.
inline float syncDivisionBeats(float norm) noexcept
{
    static constexpr float kDivisions[10] = {
        0.25f,          // 1/16
        1.0f / 3.0f,    // 1/8T
        0.5f,           // 1/8
        0.75f,          // 1/8.
        2.0f / 3.0f,    // 1/4T
        1.0f,           // 1/4
        1.5f,           // 1/4.
        2.0f,           // 1/2
        3.0f,           // 1/2.
        4.0f            // 1/1
    };
    const int idx = std::clamp((int) std::lround(clampf(norm, 0.0f, 1.0f) * 9.0f), 0, 9);
    return kDivisions[idx];
}

// Quadrature rotation oscillator (2 multiplies per sample, no trig per sample).
struct QuadOsc
{
    double c = 1.0, s = 0.0;
    double dc = 1.0, ds = 0.0;
    int renormCounter = 0;

    void setRate(double hz, double sr) noexcept
    {
        const double w = 6.283185307179586 * hz / sr;
        dc = std::cos(w);
        ds = std::sin(w);
    }
    void reset() noexcept { c = 1.0; s = 0.0; renormCounter = 0; }
    void step() noexcept
    {
        const double nc = c * dc - s * ds;
        const double ns = s * dc + c * ds;
        c = nc;
        s = ns;
        if (++renormCounter >= 2048)
        {
            renormCounter = 0;
            const double inv = 1.0 / std::sqrt(c * c + s * s);
            c *= inv;
            s *= inv;
        }
    }
};

// ---- Simple Schroeder allpass diffuser for reverb input stage --------------------------
struct SimpleAllpass
{
    DelayLine line;
    float delaySamples = 240.0f;
    float gain = 0.55f;

    void prepare(double maxSec, double nominalSec, double fs, float g)
    {
        delaySamples = (float) (nominalSec * fs);
        gain = g;
        line.prepare((float) maxSec, (float) fs);
    }
    void reset() noexcept { line.reset(); }

    float process(float x) noexcept
    {
        const float d = line.read(delaySamples);
        const float v = flushDenormal(x + gain * d);
        line.write(v);
        return flushDenormal(d - gain * v);
    }
};

// ---- 1. Lush Hall Reverb (8-line modulated Hadamard FDN) -------------------------------
class LushHallReverbFx
{
public:
    static constexpr int kLines = 8;
    static constexpr double kBaseSec[kLines] = {
        0.0297, 0.0359, 0.0413, 0.0479, 0.0533, 0.0599, 0.0661, 0.0727
    };

    void prepare(double fs)
    {
        sr = fs;
        preDelayL.prepare(0.10f, (float) fs);
        preDelayR.prepare(0.10f, (float) fs);
        diffL[0].prepare(0.025, 0.0047, fs, 0.58f);
        diffL[1].prepare(0.025, 0.0113, fs, 0.54f);
        diffR[0].prepare(0.025, 0.0053, fs, 0.58f);
        diffR[1].prepare(0.025, 0.0127, fs, 0.54f);
        for (int i = 0; i < kLines; ++i)
            tank[i].prepare(0.16f, (float) fs);
        inHpfL = inHpfR = Eq::highPass(fs, 85.0, 0.7071);
        airShelfL = airShelfR = Eq::highShelf(fs, 5500.0, 1.5);
        osc.setRate(0.41, fs);
        lastA = lastB = lastC = lastD = -1.0f;
        reset();
    }

    void reset() noexcept
    {
        preDelayL.reset();
        preDelayR.reset();
        for (int i = 0; i < 2; ++i) { diffL[i].reset(); diffR[i].reset(); }
        for (int i = 0; i < kLines; ++i) { tank[i].reset(); dampState[i] = 0.0f; }
        inHpfL.reset();
        inHpfR.reset();
        airShelfL.reset();
        airShelfR.reset();
        osc.reset();
        lastA = lastB = lastC = lastD = -1.0f;
    }

    StereoSample process(float l, float r, float a, float b, float c, float d) noexcept
    {
        const float ca = clampf(a, 0.0f, 1.0f);
        const float cb = clampf(b, 0.0f, 1.0f);
        const float cc = clampf(c, 0.0f, 1.0f);
        const float cd = clampf(d, 0.0f, 1.0f);
        if (ca != lastA || cb != lastB || cc != lastC || cd != lastD)
        {
            lastA = ca; lastB = cb; lastC = cc; lastD = cd;
            const float size = 0.55f + 1.05f * cb;
            const float rt60 = 0.45f * std::pow(31.0f, ca);
            const float dampHz = 1800.0f * std::pow(7.8f, cc);
            dampAlpha = std::clamp(1.0f - std::exp(-6.2831853f * dampHz / (float) sr), 0.01f, 0.99f);
            preSamples = (0.001f + 0.079f * cd) * (float) sr;
            modDepthSamples = (float) ((0.00035 + 0.00065 * (double) cd) * sr);
            for (int i = 0; i < kLines; ++i)
            {
                const double sec = std::clamp(kBaseSec[i] * (double) size, 0.004, 0.152);
                baseSamples[i] = (float) (sec * sr);
                lineGain[i] = std::min(0.992f, std::pow(10.0f, -3.0f * (float) sec / rt60));
            }
        }

        osc.step();
        const float s0 = (float) osc.s;
        const float c0 = (float) osc.c;

        const float inL = inHpfL.process(l);
        const float inR = inHpfR.process(r);
        preDelayL.write(inL);
        preDelayR.write(inR);
        float dL = preDelayL.read(preSamples);
        float dR = preDelayR.read(preSamples);
        dL = diffL[1].process(diffL[0].process(dL));
        dR = diffR[1].process(diffR[0].process(dR));

        float y[kLines];
        for (int i = 0; i < kLines; ++i)
        {
            const float mod = (i & 1) ? ((i & 2) ? -c0 : c0) : ((i & 2) ? -s0 : s0);
            const float raw = tank[i].read(baseSamples[i] + mod * modDepthSamples);
            dampState[i] = flushDenormal(dampState[i] + dampAlpha * (raw - dampState[i]));
            y[i] = dampState[i] * lineGain[i];
        }

        const float a0 = y[0] + y[1], a1 = y[0] - y[1];
        const float a2 = y[2] + y[3], a3 = y[2] - y[3];
        const float a4 = y[4] + y[5], a5 = y[4] - y[5];
        const float a6 = y[6] + y[7], a7 = y[6] - y[7];
        const float b0 = a0 + a2, b1 = a1 + a3, b2 = a0 - a2, b3 = a1 - a3;
        const float b4 = a4 + a6, b5 = a5 + a7, b6 = a4 - a6, b7 = a5 - a7;
        constexpr float kNorm = 0.35355339f;
        constexpr float kIn = 0.42f;
        tank[0].write(flushDenormal((b0 + b4) * kNorm + dL * kIn));
        tank[1].write(flushDenormal((b1 + b5) * kNorm + dR * kIn));
        tank[2].write(flushDenormal((b2 + b6) * kNorm - dL * kIn));
        tank[3].write(flushDenormal((b3 + b7) * kNorm + dR * kIn));
        tank[4].write(flushDenormal((b0 - b4) * kNorm + dL * kIn));
        tank[5].write(flushDenormal((b1 - b5) * kNorm - dR * kIn));
        tank[6].write(flushDenormal((b2 - b6) * kNorm - dL * kIn));
        tank[7].write(flushDenormal((b3 - b7) * kNorm - dR * kIn));

        const float outL = (y[0] + y[2] - y[4] + y[6]) * 0.58f;
        const float outR = (y[1] - y[3] + y[5] + y[7]) * 0.58f;
        return {
            softCeiling(airShelfL.process(outL)),
            softCeiling(airShelfR.process(outR))
        };
    }

private:
    double sr = 48000.0;
    DelayLine preDelayL, preDelayR;
    SimpleAllpass diffL[2], diffR[2];
    DelayLine tank[kLines];
    float dampState[kLines] {};
    float baseSamples[kLines] {};
    float lineGain[kLines] {};
    Eq inHpfL, inHpfR, airShelfL, airShelfR;
    QuadOsc osc;
    float dampAlpha = 0.5f, preSamples = 480.0f, modDepthSamples = 24.0f;
    float lastA = -1.0f, lastB = -1.0f, lastC = -1.0f, lastD = -1.0f;
};

// ---- Dual-grain circular-buffer pitch shifter (zero-allocation, triangle/cosine window)
class GrainPitchShifter
{
public:
    static constexpr double kWindowSec = 0.055;

    void prepare(double fs)
    {
        sr = fs;
        windowSamples = std::max(64.0f, (float) (kWindowSec * fs));
        line.prepare(0.12f, (float) fs);
        reset();
    }
    void reset() noexcept
    {
        line.reset();
        phase = 0.0f;
    }

    float process(float x, float ratio) noexcept
    {
        line.write(x);
        phase += (ratio - 1.0f) / windowSamples;
        if (phase >= 1.0f) phase -= 1.0f;

        const float p1 = phase;
        float p2 = phase + 0.5f;
        if (p2 >= 1.0f) p2 -= 1.0f;

        const float d1 = (1.0f - p1) * windowSamples + 2.0f;
        const float d2 = (1.0f - p2) * windowSamples + 2.0f;

        // Smooth triangle crossfade window (sums identically to 1.0, zero trig per sample!)
        const float w1 = 1.0f - std::fabs(2.0f * p1 - 1.0f);
        const float w2 = 1.0f - std::fabs(2.0f * p2 - 1.0f);
        return flushDenormal(line.read(d1) * w1 + line.read(d2) * w2);
    }

private:
    double sr = 48000.0;
    float windowSamples = 2640.0f;
    DelayLine line;
    float phase = 0.0f;
};

// ---- 2. Shimmer Reverb (Modulated FDN + Octave/Fifth Pitch-Shifted Feedback) -----------
class ShimmerReverbFx
{
public:
    static constexpr int kLines = 8;
    static constexpr double kBaseSec[kLines] = {
        0.0331, 0.0389, 0.0443, 0.0509, 0.0571, 0.0641, 0.0709, 0.0787
    };

    void prepare(double fs)
    {
        sr = fs;
        diffL.prepare(0.025, 0.0083, fs, 0.60f);
        diffR.prepare(0.025, 0.0097, fs, 0.60f);
        for (int i = 0; i < kLines; ++i)
        {
            tank[i].prepare(0.14f, (float) fs);
            baseSamples[i] = (float) (kBaseSec[i] * fs);
        }
        octL.prepare(fs);
        octR.prepare(fs);
        fifthL.prepare(fs);
        fifthR.prepare(fs);
        inHpfL = inHpfR = Eq::highPass(fs, 110.0, 0.7071);
        shimLpfL = shimLpfR = Eq::lowPass(fs, 8500.0, 0.7071);
        osc.setRate(0.37, fs);
        lastA = lastB = lastC = lastD = -1.0f;
        reset();
    }

    void reset() noexcept
    {
        diffL.reset(); diffR.reset();
        for (int i = 0; i < kLines; ++i) { tank[i].reset(); dampState[i] = 0.0f; }
        octL.reset(); octR.reset();
        fifthL.reset(); fifthR.reset();
        inHpfL.reset(); inHpfR.reset();
        shimLpfL.reset(); shimLpfR.reset();
        osc.reset();
        lastA = lastB = lastC = lastD = -1.0f;
    }

    StereoSample process(float l, float r, float a, float b, float c, float d) noexcept
    {
        const float ca = clampf(a, 0.0f, 1.0f);
        const float cb = clampf(b, 0.0f, 1.0f);
        const float cc = clampf(c, 0.0f, 1.0f);
        const float cd = clampf(d, 0.0f, 1.0f);
        if (ca != lastA || cb != lastB || cc != lastC || cd != lastD)
        {
            lastA = ca; lastB = cb; lastC = cc; lastD = cd;
            shimAmt = cb * 0.55f;
            fifthMix = cd;
            const float rt60 = 1.0f * std::pow(16.0f, ca);
            const float dampHz = 2000.0f * std::pow(6.0f, cc);
            dampAlpha = std::clamp(1.0f - std::exp(-6.2831853f * dampHz / (float) sr), 0.01f, 0.99f);
            modDepthSamples = (float) ((0.00055 + 0.00055 * (double) cd) * sr);
            fbScale = 0.35355339f * (1.0f - 0.18f * shimAmt);
            for (int i = 0; i < kLines; ++i)
                lineGain[i] = std::min(0.988f, std::pow(10.0f, -3.0f * (float) kBaseSec[i] / rt60));
        }

        osc.step();
        const float s0 = (float) osc.s;
        const float c0 = (float) osc.c;

        const float dL = diffL.process(inHpfL.process(l));
        const float dR = diffR.process(inHpfR.process(r));

        float y[kLines];
        for (int i = 0; i < kLines; ++i)
        {
            const float mod = (i & 1) ? ((i & 2) ? -c0 : c0) : ((i & 2) ? -s0 : s0);
            const float raw = tank[i].read(baseSamples[i] + mod * modDepthSamples);
            dampState[i] = flushDenormal(dampState[i] + dampAlpha * (raw - dampState[i]));
            y[i] = dampState[i] * lineGain[i];
        }

        const float tailL = (y[0] + y[2] - y[4] + y[6]) * 0.5f;
        const float tailR = (y[1] - y[3] + y[5] + y[7]) * 0.5f;
        float shL = octL.process(tailL, 2.0f);
        float shR = octR.process(tailR, 2.0f);
        if (fifthMix > 0.01f)
        {
            const float fL = fifthL.process(tailL, 1.498307f);
            const float fR = fifthR.process(tailR, 1.498307f);
            shL = lerp(shL, 0.65f * shL + 0.55f * fL, fifthMix);
            shR = lerp(shR, 0.65f * shR + 0.55f * fR, fifthMix);
        }
        shL = shimLpfL.process(softCeiling(shL)) * shimAmt;
        shR = shimLpfR.process(softCeiling(shR)) * shimAmt;

        const float a0 = y[0] + y[1], a1 = y[0] - y[1];
        const float a2 = y[2] + y[3], a3 = y[2] - y[3];
        const float a4 = y[4] + y[5], a5 = y[4] - y[5];
        const float a6 = y[6] + y[7], a7 = y[6] - y[7];
        const float b0 = a0 + a2, b1 = a1 + a3, b2 = a0 - a2, b3 = a1 - a3;
        const float b4 = a4 + a6, b5 = a5 + a7, b6 = a4 - a6, b7 = a5 - a7;

        const float feedL = (dL * 0.40f) + shL * 0.42f;
        const float feedR = (dR * 0.40f) + shR * 0.42f;
        tank[0].write(flushDenormal((b0 + b4) * fbScale + feedL));
        tank[1].write(flushDenormal((b1 + b5) * fbScale + feedR));
        tank[2].write(flushDenormal((b2 + b6) * fbScale - feedL));
        tank[3].write(flushDenormal((b3 + b7) * fbScale + feedR));
        tank[4].write(flushDenormal((b0 - b4) * fbScale + feedL));
        tank[5].write(flushDenormal((b1 - b5) * fbScale - feedR));
        tank[6].write(flushDenormal((b2 - b6) * fbScale - feedL));
        tank[7].write(flushDenormal((b3 - b7) * fbScale - feedR));

        return {
            softCeiling((tailL + shL * 0.45f) * 1.12f),
            softCeiling((tailR + shR * 0.45f) * 1.12f)
        };
    }

private:
    double sr = 48000.0;
    SimpleAllpass diffL, diffR;
    DelayLine tank[kLines];
    float dampState[kLines] {};
    float baseSamples[kLines] {};
    float lineGain[kLines] {};
    GrainPitchShifter octL, octR, fifthL, fifthR;
    Eq inHpfL, inHpfR, shimLpfL, shimLpfR;
    QuadOsc osc;
    float dampAlpha = 0.5f, modDepthSamples = 24.0f, shimAmt = 0.25f, fifthMix = 0.0f, fbScale = 0.35f;
    float lastA = -1.0f, lastB = -1.0f, lastC = -1.0f, lastD = -1.0f;
};

// ---- 3. Stereo / Ping-Pong Tempo-Synced Delay ------------------------------------------
class PingPongDelayFx
{
public:
    static constexpr float kMaxDelaySec = 2.5f;

    void prepare(double fs)
    {
        sr = fs;
        slew = 1.0f - std::exp(-1.0f / (0.040f * (float) fs));
        lineL.prepare(kMaxDelaySec, (float) fs);
        lineR.prepare(kMaxDelaySec, (float) fs);
        hpfL = hpfR = Eq::highPass(fs, 110.0, 0.7071);
        osc.setRate(0.31, fs);
        lastA = lastC = lastD = lastBpm = -1.0f;
        reset();
    }

    void reset() noexcept
    {
        lineL.reset();
        lineR.reset();
        hpfL.reset();
        hpfR.reset();
        lpL = lpR = 0.0f;
        smoothDelayL = smoothDelayR = 0.375f;
        osc.reset();
        lastA = lastC = lastD = lastBpm = -1.0f;
    }

    StereoSample process(float l, float r, float a, float b, float c, float d, bool sync, float bpm) noexcept
    {
        const float ca = clampf(a, 0.0f, 1.0f);
        const float cc = clampf(c, 0.0f, 1.0f);
        const float cd = clampf(d, 0.0f, 1.0f);
        if (ca != lastA || cc != lastC || cd != lastD || sync != lastSync || bpm != lastBpm)
        {
            lastA = ca; lastC = cc; lastD = cd; lastSync = sync; lastBpm = bpm;
            float baseSec = 0.020f * std::pow(75.0f, ca);
            if (sync)
            {
                const float safeBpm = std::clamp(bpm > 1.0f ? bpm : 120.0f, 30.0f, 300.0f);
                baseSec = (60.0f / safeBpm) * syncDivisionBeats(ca);
            }
            baseSec = std::clamp(baseSec, 0.010f, kMaxDelaySec - 0.02f);
            targetL = baseSec;
            targetR = std::clamp(baseSec * (1.0f + 0.07f * std::sin(3.14159265f * cd)), 0.010f, kMaxDelaySec - 0.02f);
            const float toneHz = 900.0f * std::pow(13.5f, cc);
            lpAlpha = std::clamp(1.0f - std::exp(-6.2831853f * toneHz / (float) sr), 0.01f, 0.99f);
            pp = clampf((cd - 0.35f) / 0.65f, 0.0f, 1.0f);
            modAmpSec = 0.00045f * cd;
        }

        smoothDelayL += slew * (targetL - smoothDelayL);
        smoothDelayR += slew * (targetR - smoothDelayR);
        osc.step();
        const float mod = (float) osc.s * modAmpSec;

        const float fb = clampf(b, 0.0f, 1.0f) * 0.86f;
        const float tapL = lineL.read(std::max(0.002f, smoothDelayL + mod) * (float) sr);
        const float tapR = lineR.read(std::max(0.002f, smoothDelayR - mod) * (float) sr);

        lpL = flushDenormal(lpL + lpAlpha * (hpfL.process(tapL) - lpL));
        lpR = flushDenormal(lpR + lpAlpha * (hpfR.process(tapR) - lpR));

        const float inMono = 0.5f * (l + r);
        const float feedInL = lerp(l, inMono, pp);
        const float feedInR = lerp(r, 0.0f, pp);
        const float fbL = lerp(lpL, lpR, pp) * fb;
        const float fbR = lerp(lpR, lpL, pp) * fb;

        lineL.write(flushDenormal(softCeiling(feedInL + fbL)));
        lineR.write(flushDenormal(softCeiling(feedInR + fbR)));

        return { softCeiling(lpL), softCeiling(lpR) };
    }

private:
    double sr = 48000.0;
    float slew = 0.0005f;
    DelayLine lineL, lineR;
    Eq hpfL, hpfR;
    QuadOsc osc;
    float lpL = 0.0f, lpR = 0.0f;
    float smoothDelayL = 0.375f, smoothDelayR = 0.375f;
    float targetL = 0.375f, targetR = 0.375f, lpAlpha = 0.5f, pp = 0.0f, modAmpSec = 0.0f;
    float lastA = -1.0f, lastC = -1.0f, lastD = -1.0f, lastBpm = -1.0f;
    bool lastSync = false;
};

// ---- 4. Dimension Stereo Chorus (4-voice quadrature warm stereo chorus) ----------------
class DimensionChorusFx
{
public:
    void prepare(double fs)
    {
        sr = fs;
        baseSamples = (float) (0.0072 * fs);
        lineL.prepare(0.030f, (float) fs);
        lineR.prepare(0.030f, (float) fs);
        warmL = warmR = Eq::highShelf(fs, 4800.0, -2.5);
        lastA = lastB = -1.0f;
        reset();
    }

    void reset() noexcept
    {
        lineL.reset();
        lineR.reset();
        warmL.reset();
        warmR.reset();
        osc.reset();
        lastA = lastB = -1.0f;
    }

    StereoSample process(float l, float r, float a, float b, float c, float d) noexcept
    {
        const float ca = clampf(a, 0.0f, 1.0f);
        const float cb = clampf(b, 0.0f, 1.0f);
        if (ca != lastA || cb != lastB)
        {
            lastA = ca; lastB = cb;
            const double rateHz = 0.12 * std::pow(20.0, (double) ca);
            osc.setRate(rateHz, sr);
            depthSamples = (float) ((0.0005 + 0.0038 * (double) cb) * sr);
        }

        const float width = clampf(c, 0.0f, 1.0f);
        const float warmth = clampf(d, 0.0f, 1.0f);
        osc.step();
        const float s = (float) osc.s;
        const float cs = (float) osc.c;

        lineL.write(l);
        lineR.write(r);

        const float v0 = lineL.read(baseSamples + depthSamples * s);
        const float v1 = lineR.read(baseSamples + depthSamples * cs);
        const float v2 = lineL.read(baseSamples - depthSamples * s);
        const float v3 = lineR.read(baseSamples - depthSamples * cs);

        float wetL = 0.5f * (v0 + v2) + 0.25f * width * (v1 - v3);
        float wetR = 0.5f * (v1 + v3) + 0.25f * width * (v2 - v0);

        if (warmth > 0.01f)
        {
            const float wL = warmL.process(softCeiling(wetL * (1.0f + 0.35f * warmth)));
            const float wR = warmR.process(softCeiling(wetR * (1.0f + 0.35f * warmth)));
            wetL = lerp(wetL, wL, warmth);
            wetR = lerp(wetR, wR, warmth);
        }

        const float mid = 0.5f * (wetL + wetR);
        const float side = 0.5f * (wetL - wetR) * (0.85f + 0.75f * width);
        return { softCeiling(mid + side), softCeiling(mid - side) };
    }

private:
    double sr = 48000.0;
    float baseSamples = 345.0f, depthSamples = 96.0f;
    DelayLine lineL, lineR;
    Eq warmL, warmR;
    QuadOsc osc;
    float lastA = -1.0f, lastB = -1.0f;
};

// ---- 5. Analog Stereo Flanger ----------------------------------------------------------
class AnalogFlangerFx
{
public:
    void prepare(double fs)
    {
        sr = fs;
        lineL.prepare(0.025f, (float) fs);
        lineR.prepare(0.025f, (float) fs);
        lastA = lastB = lastD = lastBpm = -1.0f;
        reset();
    }

    void reset() noexcept
    {
        lineL.reset();
        lineR.reset();
        fbL = fbR = 0.0f;
        osc.reset();
        lastA = lastB = lastD = lastBpm = -1.0f;
    }

    StereoSample process(float l, float r, float a, float b, float c, float d, bool sync, float bpm) noexcept
    {
        const float ca = clampf(a, 0.0f, 1.0f);
        const float cb = clampf(b, 0.0f, 1.0f);
        const float cd = clampf(d, 0.0f, 1.0f);
        if (ca != lastA || cb != lastB || cd != lastD || sync != lastSync || bpm != lastBpm)
        {
            lastA = ca; lastB = cb; lastD = cd; lastSync = sync; lastBpm = bpm;
            double rateHz = 0.05 * std::pow(70.0, (double) ca);
            if (sync)
            {
                const double safeBpm = (double) std::clamp(bpm > 1.0f ? bpm : 120.0f, 30.0f, 300.0f);
                const double beats = (double) syncDivisionBeats(ca) * 4.0;
                rateHz = safeBpm / (60.0 * beats);
            }
            osc.setRate(rateHz, sr);
            depthSamples = (float) ((0.0002 + 0.0058 * (double) cb) * sr);
            manualSamples = (float) ((0.0004 + 0.0046 * (double) cd) * sr);
        }

        osc.step();
        const float fb = (clampf(c, 0.0f, 1.0f) * 2.0f - 1.0f) * 0.80f;
        const float lfoL = 0.5f + 0.5f * (float) osc.s;
        const float lfoR = 0.5f + 0.5f * (float) osc.c;

        const float dL = std::max(2.0f, manualSamples + depthSamples * lfoL);
        const float dR = std::max(2.0f, manualSamples + depthSamples * lfoR);

        lineL.write(flushDenormal(softCeiling(l + fbL * fb)));
        lineR.write(flushDenormal(softCeiling(r + fbR * fb)));

        const float tapL = lineL.read(dL);
        const float tapR = lineR.read(dR);
        fbL = tapL;
        fbR = tapR;

        return { softCeiling(0.707f * (l + tapL)), softCeiling(0.707f * (r + tapR)) };
    }

private:
    double sr = 48000.0;
    DelayLine lineL, lineR;
    QuadOsc osc;
    float fbL = 0.0f, fbR = 0.0f;
    float depthSamples = 120.0f, manualSamples = 120.0f;
    float lastA = -1.0f, lastB = -1.0f, lastD = -1.0f, lastBpm = -1.0f;
    bool lastSync = false;
};

// ---- 6. Studio EQ & Bass-Safe Stereo Widener -------------------------------------------
class StudioEqWidenerFx
{
public:
    void prepare(double fs)
    {
        sr = fs;
        bassLpL = bassLpR = Eq::lowPass(fs, 140.0, 0.7071);
        lastA = lastB = lastC = -1.0f;
        reset();
    }

    void reset() noexcept
    {
        lowL.reset(); lowR.reset();
        midL.reset(); midR.reset();
        highL.reset(); highR.reset();
        bassLpL.reset(); bassLpR.reset();
    }

    StereoSample process(float l, float r, float a, float b, float c, float d) noexcept
    {
        const float ca = clampf(a, 0.0f, 1.0f);
        const float cb = clampf(b, 0.0f, 1.0f);
        const float cc = clampf(c, 0.0f, 1.0f);
        if (ca != lastA || cb != lastB || cc != lastC)
        {
            lastA = ca; lastB = cb; lastC = cc;
            const double lowDb = ((double) ca - 0.5) * 12.0;
            const double midDb = ((double) cb - 0.5) * 10.0;
            const double highDb = ((double) cc - 0.5) * 12.0;
            auto copyCoeff = [](Eq& dst, const Eq& src) {
                dst.b0 = src.b0; dst.b1 = src.b1; dst.b2 = src.b2;
                dst.a1 = src.a1; dst.a2 = src.a2;
            };
            const Eq lq = makeLowShelf(sr, 110.0, lowDb);
            const Eq mq = Eq::peak(sr, 2200.0, 0.7, midDb);
            const Eq hq = Eq::highShelf(sr, 8500.0, highDb);
            copyCoeff(lowL, lq); copyCoeff(lowR, lq);
            copyCoeff(midL, mq); copyCoeff(midR, mq);
            copyCoeff(highL, hq); copyCoeff(highR, hq);
        }

        float yL = highL.process(midL.process(lowL.process(l)));
        float yR = highR.process(midR.process(lowR.process(r)));

        const float bassL = bassLpL.process(yL);
        const float bassR = bassLpR.process(yR);
        const float bassMono = 0.5f * (bassL + bassR);
        const float hiL = yL - bassL;
        const float hiR = yR - bassR;

        const float widthGain = clampf(d, 0.0f, 1.0f) * 1.85f;
        const float hiMid = 0.5f * (hiL + hiR);
        const float hiSide = 0.5f * (hiL - hiR) * widthGain;
        const float bassKeepL = lerp(bassMono, bassL, 0.5f);
        const float bassKeepR = lerp(bassMono, bassR, 0.5f);

        return {
            softCeiling(bassKeepL + hiMid + hiSide),
            softCeiling(bassKeepR + hiMid - hiSide)
        };
    }

private:
    double sr = 48000.0;
    Eq lowL, lowR, midL, midR, highL, highR, bassLpL, bassLpR;
    float lastA = -1.0f, lastB = -1.0f, lastC = -1.0f;
};

// ---- 7. Bus Compressor / Glue Limiter --------------------------------------------------
class BusCompressorFx
{
public:
    void prepare(double fs)
    {
        sr = fs;
        scHpfL = scHpfR = Eq::highPass(fs, 90.0, 0.7071);
        lastA = lastB = lastC = lastD = -1.0f;
        reset();
    }

    void reset() noexcept
    {
        scHpfL.reset();
        scHpfR.reset();
        env = 0.0f;
        lastA = lastB = lastC = lastD = -1.0f;
    }

    StereoSample process(float l, float r, float a, float b, float c, float d) noexcept
    {
        const float ca = clampf(a, 0.0f, 1.0f);
        const float cb = clampf(b, 0.0f, 1.0f);
        const float cc = clampf(c, 0.0f, 1.0f);
        const float cd = clampf(d, 0.0f, 1.0f);
        if (ca != lastA || cb != lastB || cc != lastC || cd != lastD)
        {
            lastA = ca; lastB = cb; lastC = cc; lastD = cd;
            const float ratio = 1.5f + 8.5f * cc;
            slope = 1.0f - (1.0f / ratio);
            threshLin = std::pow(10.0f, (-2.0f - 24.0f * ca) / 20.0f);
            makeupLin = std::pow(10.0f, (ca * slope * 7.5f) / 20.0f);
            const float attSec = 0.003f + 0.027f * cb;
            const float relSec = 0.055f + 0.220f * cb;
            attCoeff = std::exp(-1.0f / (attSec * (float) sr));
            relCoeff = std::exp(-1.0f / (relSec * (float) sr));
            warmth = cd;
            drive = 1.0f + 1.4f * cd;
            invDrive = 1.0f / std::tanh(drive);
        }

        const float detL = scHpfL.process(l);
        const float detR = scHpfR.process(r);
        const float peak = std::max(std::fabs(detL), std::fabs(detR));
        const float coeff = peak > env ? attCoeff : relCoeff;
        env = flushDenormal(coeff * env + (1.0f - coeff) * peak);

        // Linear-domain gain reduction (avoids log10/pow when below threshold!)
        float gain = makeupLin;
        if (env > threshLin)
            gain *= std::pow(threshLin / env, slope);

        float yL = l * gain;
        float yR = r * gain;
        if (warmth > 0.01f)
        {
            yL = lerp(yL, std::tanh(yL * drive) * invDrive, warmth);
            yR = lerp(yR, std::tanh(yR * drive) * invDrive, warmth);
        }
        return { softCeiling(yL), softCeiling(yR) };
    }

private:
    double sr = 48000.0;
    Eq scHpfL, scHpfR;
    float env = 0.0f;
    float threshLin = 0.5f, makeupLin = 1.0f, slope = 0.5f;
    float attCoeff = 0.99f, relCoeff = 0.999f, warmth = 0.0f, drive = 1.0f, invDrive = 1.0f;
    float lastA = -1.0f, lastB = -1.0f, lastC = -1.0f, lastD = -1.0f;
};

// ---- Extended FX Slot Container --------------------------------------------------------
class CinematicFxSlot
{
public:
    void prepare(double fs)
    {
        hall.prepare(fs);
        shimmer.prepare(fs);
        pingPong.prepare(fs);
        dimChorus.prepare(fs);
        flanger.prepare(fs);
        studioEq.prepare(fs);
        busComp.prepare(fs);
        used = 0;
    }

    void reset() noexcept
    {
        for (int m = 1; m < kExtModelCount; ++m)
            clear(m);
    }

    void clear(int extModel) noexcept
    {
        const std::uint32_t bit = 1u << (extModel & 31);
        if ((used & bit) == 0)
            return;
        used &= ~bit;
        switch ((ExtModel) extModel)
        {
            case ExtModel::hall: hall.reset(); break;
            case ExtModel::shimmer: shimmer.reset(); break;
            case ExtModel::pingPong: pingPong.reset(); break;
            case ExtModel::dimChorus: dimChorus.reset(); break;
            case ExtModel::flanger: flanger.reset(); break;
            case ExtModel::studioEq: studioEq.reset(); break;
            case ExtModel::busComp: busComp.reset(); break;
            default: break;
        }
    }

    StereoSample wet(int extModel, float l, float r, float a, float b, float c, float d, bool sync, float bpm) noexcept
    {
        used |= 1u << (extModel & 31);
        switch ((ExtModel) extModel)
        {
            case ExtModel::hall: return hall.process(l, r, a, b, c, d);
            case ExtModel::shimmer: return shimmer.process(l, r, a, b, c, d);
            case ExtModel::pingPong: return pingPong.process(l, r, a, b, c, d, sync, bpm);
            case ExtModel::dimChorus: return dimChorus.process(l, r, a, b, c, d);
            case ExtModel::flanger: return flanger.process(l, r, a, b, c, d, sync, bpm);
            case ExtModel::studioEq: return studioEq.process(l, r, a, b, c, d);
            case ExtModel::busComp: return busComp.process(l, r, a, b, c, d);
            default: return { l, r };
        }
    }

    StereoSample process(int extModel, float l, float r, float mix, float a, float b, float c, float d, bool sync, float bpm) noexcept
    {
        const auto w = wet(extModel, l, r, a, b, c, d, sync, bpm);
        mix = clampf(mix, 0.0f, 1.0f);
        if (extModel == (int) ExtModel::hall
            || extModel == (int) ExtModel::shimmer
            || extModel == (int) ExtModel::pingPong)
        {
            const float dry = std::cos(1.5707963267948966f * mix);
            const float wetGain = std::sin(1.5707963267948966f * mix);
            return { l * dry + w.l * wetGain, r * dry + w.r * wetGain };
        }
        return { lerp(l, w.l, mix), lerp(r, w.r, mix) };
    }

    LushHallReverbFx hall;
    ShimmerReverbFx shimmer;
    PingPongDelayFx pingPong;
    DimensionChorusFx dimChorus;
    AnalogFlangerFx flanger;
    StudioEqWidenerFx studioEq;
    BusCompressorFx busComp;

private:
    std::uint32_t used = 0;
};

} // namespace cs80::fx
