#pragma once

#include <bit>
#include <cfenv>
#if defined(__SSE__) || defined(_M_X64) || defined(_M_IX86)
#include <xmmintrin.h>
#endif

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace cs80
{

constexpr float kPi = 3.14159265358979323846f;
constexpr float kTwoPi = 6.28318530717958647692f;

inline float clampf(float x, float lo, float hi) noexcept
{
    return x < lo ? lo : (x > hi ? hi : x);
}

inline float lerp(float a, float b, float t) noexcept
{
    return a + (b - a) * t;
}

inline float softClip(float x) noexcept
{
    // Generic soft saturation approximation; not an identified OTA/op-amp transfer.
    return std::tanh(x * 0.85f) / 0.85f;
}

inline float midiToHz(float note, float tuningRef = 440.0f) noexcept
{
    return tuningRef * std::pow(2.0f, (note - 69.0f) / 12.0f);
}

// IG00156 nominal law: Yamaha CS servicing guide p34; CS-80 IC sheet p42.
// MIDI60 / .5V is an interface reference, not a panel-slider voltage claim.
constexpr float kReferenceKeyVolts = 0.5f;
// Exact-argument exp2 memo; bit-identical to std::exp2 (see ExpMemo below).
struct Exp2Memo
{
    float arg = std::numeric_limits<float>::quiet_NaN();
    float value = 0.0f;
    float operator()(float x) noexcept
    {
        if (!(x == arg))
        {
            arg = x;
            value = std::exp2(x);
        }
        return value;
    }
};

inline float filterKeyVolts(float midiPitch) noexcept
{
    return kReferenceKeyVolts * std::exp2((midiPitch - 60.0f) / 12.0f);
}
inline float filterKeyVolts(float midiPitch, Exp2Memo& memo) noexcept
{
    return kReferenceKeyVolts * memo((midiPitch - 60.0f) / 12.0f);
}
inline float nominalFilterCutoff(float keyVolts, float effectiveCutoffCv) noexcept
{
    return 125.0f * keyVolts * std::exp2(effectiveCutoffCv);
}
inline float nominalFilterCutoff(float keyVolts, float effectiveCutoffCv, Exp2Memo& memo) noexcept
{
    return 125.0f * keyVolts * memo(effectiveCutoffCv);
}
inline float filterEffectiveCv(float referenceCutoffHz) noexcept
{
    return std::log2(std::max(referenceCutoffHz, 0.001f) / (125.0f * kReferenceKeyVolts));
}
// Only Q endpoints (.5 at 10V, 5 at 0V) are documented. Linear damping
// between them and the normalized panel mapping are explicitly provisional.
inline float provisionalFilterQ(float resonance) noexcept
{
    return 1.0f / (2.0f - 1.8f * clampf(resonance, 0.0f, 1.0f));
}

class Rng
{
public:
    explicit Rng(uint32_t seed = 0x8a5cd789u) : state(seed ? seed : 0x8a5cd789u) {}

    uint32_t nextUInt() noexcept
    {
        uint32_t x = state;
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        state = x;
        return x;
    }

    float nextUnipolar() noexcept
    {
        return (float) (nextUInt() >> 8) * (1.0f / 16777216.0f);
    }

    float nextBipolar() noexcept
    {
        return nextUnipolar() * 2.0f - 1.0f;
    }

private:
    uint32_t state;
};

// PolyBLEP residual for a unit step at phase 0.
inline float polyBlep(float t, float dt) noexcept
{
    if (dt <= 0.0f)
        return 0.0f;
    if (t < dt)
    {
        t /= dt;
        return t + t - t * t - 1.0f;
    }
    if (t > 1.0f - dt)
    {
        t = (t - 1.0f) / dt;
        return t * t + t + t + 1.0f;
    }
    return 0.0f;
}

// Band-limited pulse. Width is in phase units (0..1).
inline float blepPulse(float phase, float width, float dt) noexcept
{
    // Preserve sub-percent fixed-time reset pulses; a 1% minimum changes
    // their duration as pitch changes. Overlapping BLEPs are additive.
    width = clampf(width, 0.000001f, 0.999999f);
    float v = phase < width ? 1.0f : -1.0f;
    v += polyBlep(phase, dt);
    float t = phase - width;
    if (t < 0.0f)
        t += 1.0f;
    v -= polyBlep(t, dt);
    return v;
}

// Hardware ribbon travel is relative to first contact. Yamaha specifies
// approximately +1 octave over a full rightward stroke, with downward travel
// reaching sub-audio. Linear frequency scaling models those endpoints; the
// precise ribbon resistance/contact transfer still needs hardware measurement.
inline float ribbonFrequencyRatio(float displacement) noexcept
{
    return 1.0f + clampf(displacement, -1.0f, 1.0f);
}

// Original DEPTH controls a frequency envelope, not ring amplitude. This
// bounded first-order behavioral mapping preserves the documented dependence
// of peak frequency on SPEED. Its one-octave maximum excursion is provisional,
// NOT a measured CS-80 transfer function (see docs/FIDELITY_AUDIT.md).
inline constexpr float kProvisionalRingFrequencyExcursion = 1.0f;

inline float ringModulationFrequency(float baseHz, float depth, float envelope) noexcept
{
    return std::max(0.0f, baseHz)
           * (1.0f + kProvisionalRingFrequencyExcursion
                     * clampf(depth, 0.0f, 1.0f) * clampf(envelope, 0.0f, 1.0f));
}

class Oscillator
{
public:
    void reset(float startPhase = 0.0f) noexcept
    {
        phase = startPhase - std::floor(startPhase);
    }

    struct Output
    {
        float saw = 0.0f;
        float pulse = 0.0f;
        float sine = 0.0f;
        float narrow = 0.0f;
    };

    Output tick(float frequency, float sampleRate, float pulseWidth,
                bool sawOn, bool pulseOn, float noiseAmount, Rng& rng) noexcept
    {
        const float dt = clampf(frequency / sampleRate, 0.0f, 0.45f);
        phase += dt;
        if (phase >= 1.0f)
            phase -= std::floor(phase);

        Output o;

        // The IG00153 is a current-source ramp with a constant ~30 us reset
        // pulse. The pulse is fixed in time, so its duty cycle grows with pitch.
        const float narrowPhase = clampf(30.0e-6f * frequency, 0.0001f, 0.45f);
        o.narrow = blepPulse(phase, narrowPhase, dt) * 0.55f;

        if (sawOn)
        {
            // Falling saw, with the reset pulse added as a narrow edge.
            o.saw = (1.0f - 2.0f * phase) + polyBlep(phase, dt) + o.narrow * 0.70f;
        }
        if (pulseOn)
        {
            // IG00158 PI9 -> comparator -> PO7; M41 has no separate
            // reset-pulse summing branch here. Keep an ideal bandlimited
            // two-level comparator approximation. Actual reset-dependent
            // crossing times/clamps remain uncalibrated; see oscillator evidence.
            o.pulse = blepPulse(phase, pulseWidth, dt);
        }

        // Provisional sine-shaper approximation. This fixed 3.5% third harmonic
        // is not derived from an identified circuit or calibrated hardware data.
        const float s = std::sin(kTwoPi * phase);
        o.sine = s + 0.035f * std::sin(3.0f * kTwoPi * phase);

        if (noiseAmount > 0.0f)
            o.saw += rng.nextBipolar() * noiseAmount;

        return o;
    }

    float getPhase() const noexcept { return phase; }

private:
    float phase = 0.0f;
};

// IG00156 circuit-engine constants (docs/IG00156_REALTIME_MODEL.md).
// Source-derived: the recovered graph places 4.7 MOhm feedforward resistors
// across both 1.5 nF integrators (src/reference/IG00156Linear.h), adding
// 1 / (2 pi * 4.7 MOhm * 1.5 nF) = 22.58 Hz to each integrator's unity-gain
// frequency regardless of the gm control, a natural lower cutoff limit.
// The paths are feedforward into the integrator summing nodes, not leakage
// (docs/IG00156_MODEL_EVIDENCE.md); the documented control law is taken as
// the gm part, so the floor adds to it. It is an ideal-model contribution,
// not a measured minimum.
// INFERRED: the transconductor saturation level in the engine's signal units.
// 2.5 keeps a full oscillator mix (about +/-1.8 at the HP node) mildly
// nonlinear (tanh(0.72) = 0.62) while resonant peaks compress strongly.
inline constexpr float kIg00156FeedforwardHz = 22.575f;
inline constexpr double kIg00156OtaLevel = 2.5;

// Lambert continued-fraction (7/6) tanh, clamped where it meets +/-1.
// Max abs error below 1e-4 (largest near the 4.97 clamp); below 1e-6 for
// |x| < 3. Odd, monotonic, cheap. Used only by the
// circuit-engine filter, never by legacy paths.
inline double fastTanh(double x) noexcept
{
    if (x > 4.97) return 1.0;
    if (x < -4.97) return -1.0;
    const double x2 = x * x;
    return x * (135135.0 + x2 * (17325.0 + x2 * (378.0 + x2)))
             / (135135.0 + x2 * (62370.0 + x2 * (3150.0 + x2 * 28.0)));
}

// Topology-preserving state-variable filter. One instance per HP or LP stage.
class Svf
{
public:
    // Numerical integrator memories, not calibrated capacitor voltages.
    struct State { float ic1 = 0.0f, ic2 = 0.0f; };
    State getState() const noexcept { return { ic1, ic2 }; }
    // State transfer for the circuit-engine free-running idle path, which
    // re-references these trapezoidal memories when its step rate changes
    // (src/experimental/ContinuousVoiceCircuit.h). No effect on any filter law.
    void setState(State s) noexcept { ic1 = s.ic1; ic2 = s.ic2; }
    void reset() noexcept
    {
        ic1 = ic2 = 0.0f;
        slope1 = slope2 = 1.0f;
    }

    void set(float cutoffHz, float q, float sampleRate) noexcept
    {
        cutoffHz = clampf(cutoffHz, 18.0f, sampleRate * 0.45f);
        q = clampf(q, 0.35f, 9.0f);
        // Exact-input memo: identical coefficients, no tan() while cutoff,
        // Q and rate hold (common once an envelope settles).
        if (cutoffHz == lastCutoff && q == lastQ && sampleRate == lastRate)
            return;
        lastCutoff = cutoffHz; lastQ = q; lastRate = sampleRate;
        const float g = std::tan(kPi * cutoffHz / sampleRate);
        const float k = 1.0f / q;
        gc = g;
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
        kk = k;
    }

    struct Output
    {
        float lp = 0.0f;
        float hp = 0.0f;
        float bp = 0.0f;
    };

    Output process(float x) noexcept
    {
        // Provisional pre-filter saturation, not an identified IG00156 nonlinearity.
        // See docs/IG00156_MODEL_EVIDENCE.md before substituting a circuit model.
        x = softClip(x * 1.1f);
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        return { v2, x - kk * v1 - v2, v1 };
    }

    // Circuit-engine IG00156 stage (docs/IG00156_REALTIME_MODEL.md). Same
    // integrator memories and coefficients as process(), but the transconductor
    // saturation sits INSIDE the loop: each integrator is driven through
    // kIg00156OtaLevel * tanh(u / kIg00156OtaLevel) (INFERRED OTA law), and the
    // implicit trapezoidal equations are solved per sample by Newton iteration
    // on the HP node. No input clip. Small signals reproduce process() without
    // its pre-clip gain.
    Output processCircuit(float input) noexcept
    {
        const double x = input, g = gc, k = kk, s1 = ic1, s2 = ic2;
        const double v = kIg00156OtaLevel, inv = 1.0 / v;
        // Starting point: the linear solution using each OTA's effective gain
        // from the previous sample (cached slope). The evaluation that meets the
        // tolerance (well below float output resolution) is reused, so most
        // samples cost one or two Newton evaluations of two tanh calls each.
        const double g1 = g * slope1, g2 = g * slope2;
        double hp = (x - (k + g2) * s1 - s2) / (1.0 + g1 * (k + g2));
        double bp = 0.0, lp = 0.0, t1 = 0.0, t2 = 0.0;
        for (int iteration = 0;; ++iteration)
        {
            t1 = fastTanh(hp * inv);
            bp = s1 + g * v * t1;
            t2 = fastTanh(bp * inv);
            lp = s2 + g * v * t2;
            const double f = hp + k * bp + lp - x;
            if (std::abs(f) < 1.0e-7 || iteration == 4)
                break;
            const double d1 = 1.0 - t1 * t1, d2 = 1.0 - t2 * t2;
            hp -= f / (1.0 + g * d1 * (k + g * d2));
        }
        slope1 = std::abs(hp * inv) > 1.0e-9 ? static_cast<float>(t1 / (hp * inv)) : 1.0f;
        slope2 = std::abs(bp * inv) > 1.0e-9 ? static_cast<float>(t2 / (bp * inv)) : 1.0f;
        ic1 = static_cast<float>(2.0 * bp - s1);
        ic2 = static_cast<float>(2.0 * lp - s2);
        return { static_cast<float>(lp), static_cast<float>(hp), static_cast<float>(bp) };
    }

private:
    float gc = 0.0f;
    float slope1 = 1.0f, slope2 = 1.0f; // circuit path: previous OTA gain (Newton start only)
    float a1 = 0.0f, a2 = 0.0f, a3 = 0.0f, kk = 1.0f;
    float ic1 = 0.0f, ic2 = 0.0f;
    float lastCutoff = -1.0f, lastQ = -1.0f, lastRate = -1.0f;
};

class OnePole
{
public:
    void set(float cutoffHz, float sampleRate) noexcept
    {
        const float x = std::exp(-kTwoPi * clampf(cutoffHz, 1.0f, sampleRate * 0.49f) / sampleRate);
        a = 1.0f - x;
        b = x;
    }

    float process(float x) noexcept
    {
        z = a * x + b * z;
        return z;
    }

    void reset() noexcept { z = 0.0f; }

private:
    float a = 1.0f, b = 0.0f, z = 0.0f;
};

// Exact-argument memo for per-sample transcendental calls whose argument is
// usually unchanged between samples. The result is bit-identical to calling
// the function directly; it only skips recomputation while the argument holds.
// NaN initial state forces the first evaluation.
template <typename T>
struct ExpMemo
{
    T arg = std::numeric_limits<T>::quiet_NaN();
    T value = T(0);
    T operator()(T x) noexcept
    {
        if (!(x == arg))
        {
            arg = x;
            value = std::exp(x);
        }
        return value;
    }
};

inline float coefficientExponent(float seconds, float sampleRate, float multiplier = 1.0f) noexcept
{
    seconds = std::max(seconds, 0.00005f);
    return -6.0f * multiplier / (seconds * sampleRate);
}

inline float coefficientForTime(float seconds, float sampleRate, float multiplier = 1.0f) noexcept
{
    // Calibrate the control as a musical time-to-rest (-60 dB), not a raw RC
    // time constant. Without this, a 200 ms release would still be audible a
    // second later.
    return std::exp(coefficientExponent(seconds, sampleRate, multiplier));
}

// Circuit-engine envelope law (docs/ENVELOPE_RC_MODEL.md, IMPROVEMENT_PLAN 1.3).
// INFERRED single-RC capacitor model, not a measured IG00152/IG00159 law:
// - decay/release: one exponential whose panel time T is time-to--60 dB of the
//   remaining span, tau = T / ln(1000);
// - attack: charge toward an overshoot target start + k*(peak - start); a
//   comparator ends the stage at the peak. With k = 1.5 the peak is reached at
//   exactly T from any start level (tau = T / ln(k / (k - 1))).
// The legacy float paths keep their historical -6 exponent and x5 bends.
inline constexpr double kRcSettleLog = 6.907755278982137; // ln(1000)
inline constexpr double kRcAttackOvershoot = 1.5;
inline constexpr double kRcAttackLog = 1.0986122886681098; // ln(1.5 / 0.5)

// Provisional bent-envelope approximation. The fast bend and slower tail
// remain uncalibrated; this shape is not proof of original envelope circuitry.
class Adsr
{
public:
    enum class Stage { Idle, Attack, Decay, Sustain, Release };

    void reset() noexcept
    {
        preciseValue = 0.0;
        value = 0.0f;
        stage = Stage::Idle;
        stageTime = 0.0f;
        preciseTime = 0.0;
        liveSustain = liveDecay = false;
    }

    void noteOn(float attack, float decay, float sustain, float release) noexcept
    {
        a = attack; d = decay; s = clampf(sustain, 0.0f, 1.0f); r = release;
        liveSustain = liveDecay = false;
        stage = Stage::Attack;
        stageTime = 0.0f;
        preciseTime = 0.0;
        if (a <= 0.0002f)
            value = 1.0f;
        preciseValue = value;
        attackFrom = preciseValue;
    }

    void noteOff() noexcept
    {
        if (stage != Stage::Idle)
        {
            stage = Stage::Release;
            stageTime = 0.0f;
            preciseTime = 0.0;
        }
    }

    // reconstruction is fixed for a note lifetime; Engine resets on mode changes.
    float tick(float sampleRate, bool bent = true, bool reconstruction = false) noexcept
    {
        if (reconstruction) return tickReconstruction(sampleRate, bent);
        stageTime += 1.0f / sampleRate;
        switch (stage)
        {
            case Stage::Idle:
                value = 0.0f;
                break;
            case Stage::Attack:
            {
                const float c = expF(coefficientExponent(a, sampleRate));
                value = 1.0f + (value - 1.0f) * c;
                if (value >= 0.999f || stageTime > a * 1.2f + 0.001f)
                {
                    value = 1.0f;
                    stage = Stage::Decay;
                    stageTime = 0.0f;
                    if (liveDecay) decayState = value;
                }
                break;
            }
            case Stage::Decay:
            {
                const float fast = (bent && stageTime < 0.035f) ? 5.0f : 1.0f;
                if (liveDecay)
                {
                    const double liveC = expD(-6.0 * fast / (std::max(d, 0.00005f) * static_cast<double>(sampleRate)));
                    decayState = s + (decayState - s) * liveC;
                    value = static_cast<float>(decayState);
                }
                else
                {
                    const float c = expF(coefficientExponent(d, sampleRate, fast));
                    value = s + (value - s) * c;
                }
                if (std::abs(value - s) < 0.0005f)
                {
                    value = s;
                    stage = Stage::Sustain;
                }
                break;
            }
            case Stage::Sustain:
                if (liveSustain)
                {
                    // Inferred retained decay-port tracking, without re-entering
                    // Decay or restarting its bend window. Double accumulation
                    // avoids float quantization stalling a long, slow transition.
                    const double c = expD(-6.0 / (std::max(d, 0.00005f) * static_cast<double>(sampleRate)));
                    sustainState = s + (sustainState - s) * c;
                    value = static_cast<float>(sustainState);
                    if (std::abs(sustainState - s) < 0.0005)
                    {
                        value = s;
                        liveSustain = false;
                    }
                }
                else value = s;
                break;
            case Stage::Release:
            {
                const float fast = (bent && stageTime < 0.045f) ? 5.0f : 1.0f;
                const float c = expF(coefficientExponent(r, sampleRate, fast));
                value *= c;
                if (value < 0.0002f)
                {
                    value = 0.0f;
                    stage = Stage::Idle;
                }
                break;
            }
        }
        return value;
    }

    // Live timing controls only: preserve the capacitor-state proxy, stage and
    // elapsed time. Target policies below are explicitly engineering-inferred.
    void setTimes(float attack, float decay, float release) noexcept
    {
        a = attack;
        d = decay;
        r = release;
    }
    // Selected target changes do not replace stored state or restart a stage.
    // Attack stores S for Decay; Release/Idle do not respond to S. A held
    // Sustain follows S with the existing decay-time law (inferred, unmeasured).
    void setSustain(float sustain) noexcept
    {
        sustain = clampf(sustain, 0.0f, 1.0f);
        if (sustain != s && stage == Stage::Sustain)
        {
            if (!liveSustain) sustainState = value;
            liveSustain = true;
        }
        if (sustain != s && (stage == Stage::Attack || stage == Stage::Decay))
        {
            if (!liveDecay) decayState = value;
            liveDecay = true;
        }
        s = sustain;
    }
    struct State { Stage stage = Stage::Idle; float value = 0.0f, elapsed = 0.0f; };
    State getState() const noexcept { return { stage, value, stageTime }; }

    void setRelease(float seconds) noexcept { r = seconds; }
    bool isIdle() const noexcept { return stage == Stage::Idle; }
    bool isReleased() const noexcept { return stage == Stage::Release || stage == Stage::Idle; }
    float getValue() const noexcept { return value; }

private:
    // Never round this state through the public float diagnostic/output. The
    // legacy path remains separate to preserve existing patches byte-for-byte.
    double preciseValue = 0.0, preciseTime = 0.0;
    float tickReconstruction(float sampleRate, bool) noexcept
    {
        preciseTime += 1.0 / static_cast<double>(sampleRate);
        // Single-RC law; `log` selects the settle (decay/release) or attack constant.
        const auto coefficient = [this, sampleRate](float time, double log = kRcSettleLog) {
            return expD(-log / (std::max(time, 0.00005f) * static_cast<double>(sampleRate)));
        };
        switch (stage)
        {
            case Stage::Idle: preciseValue = 0.0; break;
            case Stage::Attack:
            {
                // Charge toward the overshoot target; the comparator ends at 1.
                const double target = 1.0 + (kRcAttackOvershoot - 1.0) * (1.0 - attackFrom);
                if (attackFrom < 1.0)
                    preciseValue = target + (preciseValue - target) * coefficient(a, kRcAttackLog);
                if (preciseValue >= 1.0 || attackFrom >= 1.0)
                {
                    preciseValue = 1.0; stage = Stage::Decay; preciseTime = 0.0;
                }
                break;
            }
            case Stage::Decay:
                preciseValue = s + (preciseValue - s) * coefficient(d);
                if (std::abs(preciseValue - s) < 0.0005f)
                {
                    preciseValue = s; stage = Stage::Sustain;
                }
                break;
            case Stage::Sustain:
                if (liveSustain)
                {
                    preciseValue = s + (preciseValue - s) * coefficient(d);
                    if (std::abs(preciseValue - s) < 0.0005f)
                    {
                        preciseValue = s; liveSustain = false;
                    }
                }
                else preciseValue = s;
                break;
            case Stage::Release:
                preciseValue *= coefficient(r);
                if (preciseValue < 0.0002f) { preciseValue = 0.0; stage = Stage::Idle; }
                break;
        }
        stageTime = static_cast<float>(preciseTime);
        return value = static_cast<float>(preciseValue);
    }
    Stage stage = Stage::Idle;
    float value = 0.0f;
    float stageTime = 0.0f;
    bool liveSustain = false, liveDecay = false;
    double sustainState = 0.0, decayState = 0.0;
    double attackFrom = 0.0; // circuit-engine attack start level
    float a = 0.0f, d = 0.0f, s = 0.0f, r = 0.0f;
    ExpMemo<float> expF;
    ExpMemo<double> expD;
};

// Per-voice tuning scatter at VOICE DISPERSION = 1, in cents. Arbitrary software
// scatter, not a measured tolerance of any instrument. It was formerly added to a
// semitone pitch without the cents conversion, detuning default voices by over two
// semitones; see docs/PARITY_ROADMAP.md (voice variation).
inline constexpr float kProvisionalVoiceTuneSpreadCents = 7.0f;

// The original IL is below the steady cutoff; AL is above it. Preserve the
// existing three-octave model scale, but expose its uncalibrated nature instead
// of treating printed -5/+5 panel markings as octave measurements.
inline constexpr float kProvisionalFilterEnvelopeOctaves = 3.0f;
// M-board EG summing resistors differ (HP100k, LP47k). Retain the prior
// approximate half-depth HP path until loaded-node gains are fully derived.
inline constexpr float kProvisionalHpfEnvelopeGain = 0.5f;
inline float filterInitialOffset(float control) noexcept
{
    return -kProvisionalFilterEnvelopeOctaves * clampf(control, 0.0f, 1.0f);
}
inline float filterAttackOffset(float control) noexcept
{
    return kProvisionalFilterEnvelopeOctaves * clampf(control, 0.0f, 1.0f);
}

// Yamaha IL -> AL -> steady cutoff, then release towards IL. A held steady
// state is distinct from Idle so noteOff still releases a settled contour.
class FilterEnv
{
public:
    enum class Stage { Idle, Attack, Decay, Sustain, Release };

    void reset() noexcept
    {
        preciseValue = 0.0;
        value = 0.0f;
        stage = Stage::Idle;
        stageTime = 0.0f;
        preciseTime = 0.0;
        liveAttack = liveRelease = releaseSelected = false;
    }

    // Circuit-engine released TR selects IL even before the first note.
    // This changes a drive selector only: no capacitor value is assigned.
    // Existing release timing supplies the bounded evolution in Idle.
    // M41/IC12 and guide p33; docs/COLD_FILTER_ENVELOPE.md.
    void selectInitialWhileIdle() noexcept
    {
        if (stage == Stage::Idle)
        {
            releaseSelected = liveRelease = true;
            releaseState = value;
        }
    }

    // fromCurrentLevel: circuit-engine assignment retains the capacitor's
    // present level, including a first note before idle IL settling.
    void noteOn(float initialLevel, float attackLevel, float attack, float decay,
                float release, float sustainLevel, bool fromCurrentLevel = false) noexcept
    {
        liveAttack = liveRelease = releaseSelected = false;
        il = initialLevel;
        al = attackLevel;
        a = attack;
        d = decay;
        r = release;
        sustain = sustainLevel;
        if (!fromCurrentLevel)
            value = il;
        stage = Stage::Attack;
        stageTime = 0.0f;
        preciseTime = 0.0;
        if (a <= 0.0002f)
            value = al;
        if (!fromCurrentLevel || a <= 0.0002f)
            preciseValue = value;
        attackFrom = preciseValue;
    }

    void noteOff() noexcept
    {
        if (stage != Stage::Idle)
        {
            stage = Stage::Release;
            releaseSelected = true;
            stageTime = 0.0f;
            preciseTime = 0.0;
            if (liveRelease) releaseState = value;
        }
    }

    // reconstruction is fixed for a note lifetime; Engine resets on mode changes.
    float tick(float sampleRate, bool bent = true, bool reconstruction = false) noexcept
    {
        if (reconstruction) return tickReconstruction(sampleRate, bent);
        stageTime += 1.0f / sampleRate;
        switch (stage)
        {
            case Stage::Idle:
                // Completed release is still an IL selection while another
                // envelope keeps the voice audible. Track without reactivating
                // the stage; an unselected reset Idle remains inert.
                if (releaseSelected && liveRelease && value != il)
                {
                    const double c = expD(-6.0 / (std::max(r, 0.00005f) * static_cast<double>(sampleRate)));
                    releaseState += (il - releaseState) * (1.0 - c);
                    value = static_cast<float>(releaseState);
                    if (std::abs(value - il) < 0.0008f)
                        value = static_cast<float>(releaseState = il);
                }
                break;
            case Stage::Attack:
            {
                if (liveAttack)
                {
                    const double liveC = expD(-6.0 / (std::max(a, 0.00005f) * static_cast<double>(sampleRate)));
                    attackState = al + (attackState - al) * liveC;
                    value = static_cast<float>(attackState);
                }
                else
                {
                    const float c = expF(coefficientExponent(a, sampleRate));
                    value = al + (value - al) * c;
                }
                // A moved target invalidates the old onset-based timeout. Retain
                // elapsed state and finish only near the new target; no big snap.
                if (std::abs(value - al) < 0.001f || (!liveAttack && stageTime > a * 1.2f + 0.001f))
                {
                    value = al;
                    stage = Stage::Decay;
                    stageTime = 0.0f;
                }
                break;
            }
            case Stage::Decay:
            {
                const float fast = (bent && stageTime < 0.035f) ? 5.0f : 1.0f;
                const float c = expF(coefficientExponent(d, sampleRate, fast));
                value = sustain + (value - sustain) * c;
                if (std::abs(value - sustain) < 0.0008f)
                {
                    value = sustain;
                    stage = Stage::Sustain;
                }
                break;
            }
            case Stage::Sustain:
                value = sustain;
                break;
            case Stage::Release:
            {
                const float fast = (bent && stageTime < 0.04f) ? 5.0f : 1.0f;
                if (liveRelease)
                {
                    const double liveC = expD(-6.0 * fast / (std::max(r, 0.00005f) * static_cast<double>(sampleRate)));
                    releaseState += (il - releaseState) * (1.0 - liveC);
                    value = static_cast<float>(releaseState);
                }
                else
                {
                    const float c = expF(coefficientExponent(r, sampleRate, fast));
                    value += (il - value) * (1.0f - c);
                }
                if (std::abs(value - il) < 0.0008f)
                {
                    value = il;
                    releaseState = il;
                    stage = Stage::Idle;
                }
                break;
            }
        }
        return value;
    }

    // Live timing controls only: preserve the capacitor-state proxy, stage and
    // elapsed time. Target policies below are explicitly engineering-inferred.
    void setTimes(float attack, float decay, float release) noexcept
    {
        a = attack;
        d = decay;
        r = release;
    }
    // Inferred phase selection: AL acts in Attack, IL in Release (including
    // completed-release Idle); other
    // selections are stored, not treated as a trigger. Decay/Sustain keep their
    // grounded steady target. Preserve all state, including elapsed bend time.
    void setLevels(float initialLevel, float attackLevel) noexcept
    {
        if (attackLevel != al && stage == Stage::Attack)
        {
            if (!liveAttack) attackState = value;
            liveAttack = true;
            // Circuit engine: a moved AL restarts the charge from the present level.
            attackFrom = preciseValue;
            preciseTime = 0.0;
        }
        if (initialLevel != il)
        {
            if (!liveRelease) releaseState = value;
            liveRelease = true;
        }
        il = initialLevel;
        al = attackLevel;
    }
    struct State { Stage stage = Stage::Idle; float value = 0.0f, elapsed = 0.0f; };
    State getState() const noexcept { return { stage, value, stageTime }; }

    void setRelease(float seconds) noexcept { r = seconds; }
    bool isIdle() const noexcept { return stage == Stage::Idle; }
    float getValue() const noexcept { return value; }

private:
    // Never round this state through the public float diagnostic/output. The
    // legacy path remains separate to preserve existing patches byte-for-byte.
    double preciseValue = 0.0, preciseTime = 0.0;
    float tickReconstruction(float sampleRate, bool) noexcept
    {
        preciseTime += 1.0 / static_cast<double>(sampleRate);
        // Single-RC law; `log` selects the settle (decay/release) or attack constant.
        const auto coefficient = [this, sampleRate](float time, double log = kRcSettleLog) {
            return expD(-log / (std::max(time, 0.00005f) * static_cast<double>(sampleRate)));
        };
        switch (stage)
        {
            case Stage::Idle:
                if (releaseSelected && liveRelease && preciseValue != il)
                {
                    preciseValue += (il - preciseValue) * (1.0 - coefficient(r));
                    if (std::abs(preciseValue - il) < 0.0008f) preciseValue = il;
                }
                break;
            case Stage::Attack:
            {
                // Charge IL -> AL toward an overshoot target; comparator at AL.
                const double span = static_cast<double>(al) - attackFrom;
                const double target = attackFrom + kRcAttackOvershoot * span;
                if (span != 0.0)
                    preciseValue = target + (preciseValue - target) * coefficient(a, kRcAttackLog);
                const bool reached = span > 0.0 ? preciseValue >= al
                                   : span < 0.0 ? preciseValue <= al : true;
                if (reached)
                {
                    preciseValue = al; stage = Stage::Decay; preciseTime = 0.0;
                }
                break;
            }
            case Stage::Decay:
                preciseValue = sustain + (preciseValue - sustain) * coefficient(d);
                if (std::abs(preciseValue - sustain) < 0.0008f)
                {
                    preciseValue = sustain; stage = Stage::Sustain;
                }
                break;
            case Stage::Sustain: preciseValue = sustain; break;
            case Stage::Release:
                preciseValue += (il - preciseValue) * (1.0 - coefficient(r));
                if (std::abs(preciseValue - il) < 0.0008f)
                {
                    preciseValue = il; stage = Stage::Idle;
                }
                break;
        }
        stageTime = static_cast<float>(preciseTime);
        return value = static_cast<float>(preciseValue);
    }
    Stage stage = Stage::Idle;
    float value = 0.0f, stageTime = 0.0f;
    bool liveAttack = false, liveRelease = false, releaseSelected = false;
    double attackState = 0.0, releaseState = 0.0;
    double attackFrom = 0.0; // circuit-engine attack start level
    float il = 0.0f, al = 0.0f, a = 0.0f, d = 0.0f, r = 0.0f, sustain = 0.0f;
    ExpMemo<float> expF;
    ExpMemo<double> expD;
};

class Lfo
{
public:
    void reset(float p = 0.0f) noexcept
    {
        phase = p;
        heldValue = 0.0f;
        sampleHoldStarted = false;
    }

    float tick(float rate, float sampleRate, int waveform, Rng& rng) noexcept
    {
        const float previousPhase = phase;
        phase += rate / sampleRate;
        phase -= std::floor(phase);
        const bool wrapped = phase < previousPhase;
        switch (waveform)
        {
            case 0: return std::sin(kTwoPi * phase);
            case 1: return 1.0f - 4.0f * std::abs(phase - 0.5f); // triangle
            case 2: return 2.0f * phase - 1.0f;                  // saw up
            case 3: return phase < 0.5f ? 1.0f : -1.0f;          // square
            case 4:                                             // sample & hold
                if (! sampleHoldStarted || wrapped)
                {
                    heldValue = rng.nextBipolar();
                    sampleHoldStarted = true;
                }
                return heldValue;
            case 5: return rng.nextBipolar();                   // random
            default: return 0.0f;
        }
    }

    float getPhase() const noexcept { return phase; }

private:
    float phase = 0.0f;
    float heldValue = 0.0f;
    bool sampleHoldStarted = false;
};

class DelayLine
{
public:
    void prepare(float maxSeconds, float sampleRate)
    {
        size = std::max(4, (int) std::ceil(maxSeconds * sampleRate) + 2);
        buffer.assign((size_t) size, 0.0f);
        writePos = 0;
    }

    void reset()
    {
        std::fill(buffer.begin(), buffer.end(), 0.0f);
        writePos = 0;
    }

    void write(float x) noexcept
    {
        buffer[(size_t) writePos] = x;
        if (++writePos >= size)
            writePos = 0;
    }

    float read(float delaySamples) const noexcept
    {
        delaySamples = clampf(delaySamples, 1.0f, (float) (size - 2));
        float pos = (float) writePos - delaySamples;
        while (pos < 0.0f)
            pos += (float) size;
        const int i0 = (int) pos;
        const float frac = pos - (float) i0;
        const int i1 = (i0 + 1) % size;
        return lerp(buffer[(size_t) i0], buffer[(size_t) i1], frac);
    }

    bool isPrepared() const noexcept { return ! buffer.empty(); }

    // Modern FX: 4-point third-order Hermite read for modulated taps. The position is
    // computed in double so a long delay (a tape echo at 384 kHz holds ~5e5 samples)
    // keeps sub-sample precision. Existing read() above is unchanged.
    float readHermite(double delaySamples) const noexcept
    {
        delaySamples = std::clamp(delaySamples, 2.0, (double) (size - 3));
        double pos = (double) writePos - delaySamples;
        if (pos < 0.0)
            pos += (double) size;
        const int i0 = std::min((int) pos, size - 1);
        const float t = (float) (pos - (double) i0);
        const int im1 = i0 == 0 ? size - 1 : i0 - 1;
        const int i1 = i0 + 1 >= size ? 0 : i0 + 1;
        const int i2 = i1 + 1 >= size ? 0 : i1 + 1;
        const float xm1 = buffer[(size_t) im1], x0 = buffer[(size_t) i0];
        const float x1 = buffer[(size_t) i1], x2 = buffer[(size_t) i2];
        const float c1 = 0.5f * (x1 - xm1);
        const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
        const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * t + c2) * t + c1) * t + x0;
    }

    // Exact integer tap (no interpolation), 1 = the most recent sample.
    float tap(int delaySamples) const noexcept
    {
        int i = writePos - std::clamp(delaySamples, 1, size - 1);
        if (i < 0)
            i += size;
        return buffer[(size_t) i];
    }

    int capacity() const noexcept { return size; }

private:
    std::vector<float> buffer;
    int size = 0;
    int writePos = 0;
};

class Chorus
{
public:
    void prepare(float sampleRate)
    {
        sr = sampleRate;
        dl.prepare(0.08f, sampleRate);
        dr.prepare(0.08f, sampleRate);
        lpL.set(7200.0f, sampleRate);
        lpR.set(7200.0f, sampleRate);
        reset();
    }

    void reset()
    {
        dl.reset();
        dr.reset();
        lpL.reset();
        lpR.reset();
        phase = 0.0f;
    }

    struct Stereo
    {
        float l = 0.0f;
        float r = 0.0f;
    };

    Stereo process(float inL, float inR, float speed, float depth, bool on, bool tremolo) noexcept
    {
        if (! on)
            return { inL, inR };

        phase += speed / sr;
        phase -= std::floor(phase);
        const float mod = std::sin(kTwoPi * phase);
        const float mod2 = std::sin(kTwoPi * phase + kPi * 0.95f);

        if (tremolo)
        {
            const float gainL = 1.0f - depth * 0.45f * (0.5f + 0.5f * mod);
            const float gainR = 1.0f - depth * 0.45f * (0.5f + 0.5f * mod2);
            return { inL * gainL, inR * gainR };
        }

        dl.write(inL);
        dr.write(inR);
        const float base = 0.0135f * sr;
        const float swing = 0.0045f * depth * sr;
        const float dL = base + swing * (0.5f + 0.5f * mod);
        const float dR = base + swing * (0.5f + 0.5f * mod2);
        float wetL = lpL.process(dl.read(dL));
        float wetR = lpR.process(dr.read(dR));
        const float mix = 0.22f + 0.32f * depth;
        // Cross the taps a little; the hardware's four VCAs do not produce a
        // perfectly dry/wet pair.
        return { lerp(inL, wetL, mix) + wetR * 0.06f,
                 lerp(inR, wetR, mix) + wetL * 0.06f };
    }

private:
    DelayLine dl, dr;
    OnePole lpL, lpR;
    float sr = 48000.0f;
    float phase = 0.0f;
};

// Six-stage OTA phaser for the modern FX slot. The all-pass stages are
// modulated by a triangle LFO, and the mixed output keeps the dry path intact
// so the notch pattern stays audible at moderate depth.
class Phaser
{
public:
    void prepare(float sampleRate)
    {
        sr = sampleRate;
        reset();
    }

    void reset() noexcept
    {
        for (auto& stage : state)
            stage.fill(0.0f);
        phase = 0.0f;
    }

    struct Stereo
    {
        float l = 0.0f;
        float r = 0.0f;
    };

    Stereo process(float inL, float inR, float rate, float depth) noexcept
    {
        phase += rate / sr;
        phase -= std::floor(phase);
        const float tri = 1.0f - 4.0f * std::abs(phase - 0.5f);
        const float mod = tri * clampf(depth, 0.0f, 1.0f);
        const float cutoff = clampf(680.0f * std::pow(2.0f, 2.9f * mod), 90.0f, sr * 0.42f);
        const float t = std::tan(kPi * cutoff / sr);
        // The stage below is y = -a x + s, s = x + a y, i.e. H = (-a + z^-1) / (1 - a z^-1).
        // Quarter-wave (90 degree) phase at `cutoff` needs the coefficient c = (t - 1) / (t + 1)
        // in (c + z^-1) / (1 + c z^-1), so a = -c. The earlier a = c mirrored the sweep about
        // fs/4 and put the notches near Nyquist.
        const float a = (1.0f - t) / (1.0f + t);

        float yl = inL;
        float yr = inR;
        for (size_t i = 0; i < state[0].size(); ++i)
        {
            const float nl = -a * yl + state[0][i];
            state[0][i] = yl + a * nl;
            yl = nl;
            const float nr = -a * yr + state[1][i];
            state[1][i] = yr + a * nr;
            yr = nr;
        }
        return { yl, yr };
    }

private:
    std::array<std::array<float, 6>, 2> state {};
    float sr = 48000.0f;
    float phase = 0.0f;
};

// Numerical anti-alias reconstruction filter, not an original CS-80 circuit.
// 255-tap Kaiser beta8.6: <=0.1dB through20kHz at44.1kHz host,
// >=80dB rejection from host Nyquist (internal normalized frequency .25).
class Decimator
{
public:
    static constexpr int kTaps = 255;
    static constexpr int kCenter = (kTaps - 1) / 2;
    static constexpr int kRetainedPhase = 1;
    static constexpr int kHostLatency = (kCenter - kRetainedPhase) / 2;

    void prepare(float internalRate)
    {
        constexpr double pi = 3.14159265358979323846;
        constexpr double fc = (20000.0 / 88200.0 + 0.25) * 0.5;
        constexpr double beta = 8.6;
        const double denominator = besselI0(beta);
        std::array<double, kTaps> design {};
        double sum = 0.0;
        for (int i = 0; i <= kCenter; ++i)
        {
            const double m = (double) i - kCenter;
            const double sinc = i == kCenter ? 2.0 * fc : std::sin(2.0 * pi * fc * m) / (pi * m);
            const double r = m / kCenter;
            const double window = besselI0(beta * std::sqrt(std::max(0.0, 1.0 - r*r))) / denominator;
            design[(size_t) i] = design[(size_t) (kTaps - 1 - i)] = sinc * window;
            sum += sinc * window * (i == kCenter ? 1.0 : 2.0);
        }
        for (int i = 0; i <= kCenter; ++i)
            coeffs[(size_t) i] = coeffs[(size_t) (kTaps - 1 - i)] = (float) (design[(size_t) i] / sum);
        reset();
        (void) internalRate; // normalized design; verified host rates >=44.1kHz
    }

    void reset() noexcept { hist.fill(0.0f); idx = 0; }

    // History is mirrored (each sample stored at p and p + kTaps) so taps are
    // read contiguously without a wrap test. Summation order is unchanged.
    void push(float x) noexcept
    {
        hist[(size_t) idx] = x;
        hist[(size_t) idx + kTaps] = x;
        if (++idx >= kTaps) idx = 0;
    }

    float output() const noexcept
    {
        const float* newest = hist.data() + idx - 1 + kTaps;
        auto delayed = [newest](int tap) { return newest[-tap]; };
        double y = (double) coeffs[kCenter] * delayed(kCenter);
        for (int i = 0; i < kCenter; ++i)
            y += (double) coeffs[(size_t) i]
                 * ((double) delayed(i) + (double) delayed(kTaps - 1 - i));
        return (float) y;
    }

    // Compatibility/reference path for clients needing every internal output.
    float process(float x) noexcept { push(x); return output(); }
    const std::array<float, kTaps>& getCoefficients() const noexcept { return coeffs; }

private:
    static double besselI0(double x) noexcept
    {
        double sum = 1.0, term = 1.0;
        for (int k = 1; k < 100; ++k)
        {
            term *= (x*x) / (4.0*k*k);
            sum += term;
            if (term < 1.e-17 * sum) break;
        }
        return sum;
    }
    std::array<float, kTaps> coeffs {};
    std::array<float, 2 * kTaps> hist {};
    int idx = 0;
};

// Per-factor reconstruction filter for the oversampling quality modes
// (IMPROVEMENT_PLAN 1.6); numerical, not an original circuit. Factor 2 is
// Decimator exactly (same taps, coefficients and summation order). Factors 4
// and 8 sample the same Kaiser beta 8.6 prototype more finely: taps
// 128 * factor - 1 and cutoff scaled by 2 / factor, so the host-relative
// response is unchanged: <=0.1 dB through 20 kHz at a 44.1 kHz host and
// >=80 dB rejection above host Nyquist. The transition band is fixed in Hz by
// the host rate, so the filter's duration, and with the last internal sample of
// each frame retained its delay, is 63 host samples at every factor.
class OversamplingDecimator
{
public:
    static constexpr int kMaxFactor = 8;
    static constexpr int tapsFor(int factor) noexcept { return 128 * factor - 1; }
    static constexpr int centerFor(int factor) noexcept { return (tapsFor(factor) - 1) / 2; }
    static constexpr int retainedPhaseFor(int factor) noexcept { return factor - 1; }
    static constexpr int hostLatencyFor(int factor) noexcept
    { return (centerFor(factor) - retainedPhaseFor(factor)) / factor; }
    static constexpr bool validFactor(int factor) noexcept
    { return factor == 2 || factor == 4 || factor == 8; }
    static constexpr int kMaxTaps = 128 * kMaxFactor - 1;

    // Bounded work and no allocation: safe to repeat on the render thread.
    void prepare(int newFactor) noexcept
    {
        factor = validFactor(newFactor) ? newFactor : 2;
        taps = tapsFor(factor);
        center = centerFor(factor);
        constexpr double pi = 3.14159265358979323846;
        // Same expression as Decimator for factor 2 (the scale is then exactly 1).
        const double fc = (20000.0 / 88200.0 + 0.25) * 0.5 * (2.0 / (double) factor);
        constexpr double beta = 8.6;
        const double denominator = besselI0(beta);
        std::array<double, kMaxTaps> design {};
        double sum = 0.0;
        for (int i = 0; i <= center; ++i)
        {
            const double m = (double) i - center;
            const double sinc = i == center ? 2.0 * fc : std::sin(2.0 * pi * fc * m) / (pi * m);
            const double r = m / center;
            const double window = besselI0(beta * std::sqrt(std::max(0.0, 1.0 - r*r))) / denominator;
            design[(size_t) i] = design[(size_t) (taps - 1 - i)] = sinc * window;
            sum += sinc * window * (i == center ? 1.0 : 2.0);
        }
        for (int i = 0; i <= center; ++i)
            coeffs[(size_t) i] = coeffs[(size_t) (taps - 1 - i)] = (float) (design[(size_t) i] / sum);
        reset();
    }

    void reset() noexcept { hist.fill(0.0f); idx = 0; nonzeroHistory = 0; }

    void push(float x) noexcept
    {
        // Raw magnitude bits retain subnormal history across FTZ/DAZ changes.
        // Only complete exact-zero history permits skipping the convolution.
        nonzeroHistory -= hasMagnitude(hist[(size_t) idx]) ? 1u : 0u;
        nonzeroHistory += hasMagnitude(x) ? 1u : 0u;
        hist[(size_t) idx] = x;
        hist[(size_t) (idx + taps)] = x;
        if (++idx >= taps) idx = 0;
    }

    float output() const noexcept
    {
        const float* newest = hist.data() + idx - 1 + taps;
        // With numeric-zero history, the positive-zero center starts the ordered
        // sum at +0 (the center coefficient is positive, or +0 before prepare).
        // At nearest rounding, adding signed-zero terms then stays +0. A -0
        // center may instead yield an all-negative-zero sum; keep its full FIR.
        // Directed rounding can also make +0 plus -0 become -0. Query only
        // after the zero-history predicate; every other case keeps the original
        // ordered convolution. Reading the environment changes no caller flags.
        if (nonzeroHistory == 0 && std::bit_cast<std::uint32_t>(newest[-center]) == 0u
            && nearestOutputRounding())
            return 0.0f;
        double y = (double) coeffs[(size_t) center] * newest[-center];
        for (int i = 0; i < center; ++i)
            y += (double) coeffs[(size_t) i]
                 * ((double) newest[-i] + (double) newest[-(taps - 1 - i)]);
        return (float) y;
    }

    float process(float x) noexcept { push(x); return output(); }

    unsigned getNonzeroHistoryCount() const noexcept { return nonzeroHistory; }
    int getFactor() const noexcept { return factor; }
    int getTaps() const noexcept { return taps; }
    int getCenter() const noexcept { return center; }
    int getRetainedPhase() const noexcept { return retainedPhaseFor(factor); }
    int getHostLatency() const noexcept { return hostLatencyFor(factor); }
    float getCoefficient(int tap) const noexcept
    { return tap >= 0 && tap < taps ? coeffs[(size_t) tap] : 0.0f; }

private:
    static bool nearestOutputRounding() noexcept
    {
        if (std::fegetround() != FE_TONEAREST) return false;
#if defined(__SSE__) || defined(_M_X64) || defined(_M_IX86)
        // Some x86 fenv implementations report the x87 direction while SSE
        // has a separately selected direction. Both must admit the proof.
        return (_mm_getcsr() & 0x6000u) == 0u;
#else
        return true;
#endif
    }
    static bool hasMagnitude(float value) noexcept
    {
        // Preserve raw subnormal ownership under DAZ. AppleClang can otherwise
        // lower the bit test to an FP compare that treats a subnormal as zero.
        const volatile std::uint32_t bits = std::bit_cast<std::uint32_t>(value);
        return (bits & 0x7fffffffu) != 0;
    }
    static double besselI0(double x) noexcept
    {
        double sum = 1.0, term = 1.0;
        for (int k = 1; k < 100; ++k)
        {
            term *= (x*x) / (4.0*k*k);
            sum += term;
            if (term < 1.e-17 * sum) break;
        }
        return sum;
    }
    std::array<float, kMaxTaps> coeffs {};
    std::array<float, 2 * kMaxTaps> hist {};
    int factor = 2, taps = 255, center = 127, idx = 0;
    unsigned nonzeroHistory = 0;
};
static_assert(OversamplingDecimator::hostLatencyFor(2) == Decimator::kHostLatency
              && OversamplingDecimator::tapsFor(2) == Decimator::kTaps
              && OversamplingDecimator::retainedPhaseFor(2) == Decimator::kRetainedPhase,
              "factor 2 is the established decimator");
static_assert(OversamplingDecimator::hostLatencyFor(4) == 63 && OversamplingDecimator::hostLatencyFor(8) == 63);
static_assert((OversamplingDecimator::centerFor(4) - OversamplingDecimator::retainedPhaseFor(4)) % 4 == 0
              && (OversamplingDecimator::centerFor(8) - OversamplingDecimator::retainedPhaseFor(8)) % 8 == 0,
              "integer host latency at every factor");

// Schroeder/plate hybrid for the modern FX section. Left and right have
// independent delay networks, so the tail is genuinely stereo and the plate
// does not collapse when the effect is panned.
//
// Delay times are in seconds and independent of the sample rate. The base times
// below are multiplied by kReverbTimeScale, the factor that the previous code
// applied at a 48 kHz host (96 kHz internal rate), so that configuration is
// bit-identical. The previous code applied the factor as sampleRate / 44100 on
// top of the seconds-to-samples conversion, so every delay (and the reverb time)
// grew with the sample rate: first comb echo 59 ms at a 44.1 kHz host, 65 ms at
// 48 kHz, 129 ms at 96 kHz. These are generic modern-FX constants, not CS-80
// circuit values.
inline constexpr float kReverbTimeScale = 96000.0f / 44100.0f;

class Reverb
{
public:
    void prepare(float sampleRate)
    {
        sr = sampleRate;
        const float scale = kReverbTimeScale;
        for (int ch = 0; ch < 2; ++ch)
        {
            for (int i = 0; i < 4; ++i)
            {
                const float spread = 1.0f + (ch == 0 ? 0.0f : 0.071f) + 0.031f * (float) i;
                combo[ch][i].prepare((0.0297f + 0.011f * (float) i) * scale * spread, sampleRate);
                combo[ch][i].reset();
            }
            for (int i = 0; i < 2; ++i)
            {
                allpass[ch][i].prepare((0.005f + 0.0017f * (float) i) * scale, sampleRate);
                allpass[ch][i].reset();
            }
            for (int i = 0; i < 4; ++i)
            {
                damp[ch][i].set(5200.0f + (float) i * 420.0f, sampleRate);
                damp[ch][i].reset();
            }
        }
    }

    void reset()
    {
        for (auto& bank : combo)
            for (auto& line : bank)
                line.reset();
        for (auto& bank : allpass)
            for (auto& line : bank)
                line.reset();
        for (auto& bank : damp)
            for (auto& d : bank)
                d.reset();
    }

    float process(float in, int channel) noexcept
    {
        const int ch = channel == 0 ? 0 : 1;
        const float x = in * 0.25f;
        float wet = 0.0f;
        for (int i = 0; i < 4; ++i)
        {
            const float spread = 1.0f + (ch == 0 ? 0.0f : 0.071f) + 0.031f * (float) i;
            const float d = (0.0297f + 0.011f * (float) i) * kReverbTimeScale * spread * sr;
            const float y = combo[ch][i].read(d);
            const float fed = x + damp[ch][i].process(y) * 0.78f;
            combo[ch][i].write(fed);
            wet += y;
        }
        wet *= 0.25f;
        for (int i = 0; i < 2; ++i)
        {
            const float d = (0.005f + 0.0017f * (float) i) * kReverbTimeScale * sr;
            const float y = allpass[ch][i].read(d);
            const float z = wet + y * 0.5f;
            allpass[ch][i].write(z);
            wet = y - z * 0.5f;
        }
        return wet;
    }

private:
    DelayLine combo[2][4];
    DelayLine allpass[2][2];
    OnePole damp[2][4];
    float sr = 48000.0f;
};

} // namespace cs80
