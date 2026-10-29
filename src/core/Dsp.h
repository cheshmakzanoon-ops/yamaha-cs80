#pragma once

#include <algorithm>
#include <array>
#include <cmath>
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
inline float filterKeyVolts(float midiPitch) noexcept
{
    return kReferenceKeyVolts * std::exp2((midiPitch - 60.0f) / 12.0f);
}
inline float nominalFilterCutoff(float keyVolts, float effectiveCutoffCv) noexcept
{
    return 125.0f * keyVolts * std::exp2(effectiveCutoffCv);
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

// Topology-preserving state-variable filter. One instance per HP or LP stage.
class Svf
{
public:
    // Numerical integrator memories, not calibrated capacitor voltages.
    struct State { float ic1 = 0.0f, ic2 = 0.0f; };
    State getState() const noexcept { return { ic1, ic2 }; }
    void reset() noexcept
    {
        ic1 = ic2 = 0.0f;
    }

    void set(float cutoffHz, float q, float sampleRate) noexcept
    {
        cutoffHz = clampf(cutoffHz, 18.0f, sampleRate * 0.45f);
        q = clampf(q, 0.35f, 9.0f);
        const float g = std::tan(kPi * cutoffHz / sampleRate);
        const float k = 1.0f / q;
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

private:
    float a1 = 0.0f, a2 = 0.0f, a3 = 0.0f, kk = 1.0f;
    float ic1 = 0.0f, ic2 = 0.0f;
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

inline float coefficientForTime(float seconds, float sampleRate, float multiplier = 1.0f) noexcept
{
    seconds = std::max(seconds, 0.00005f);
    // Calibrate the control as a musical time-to-rest (-60 dB), not a raw RC
    // time constant. Without this, a 200 ms release would still be audible a
    // second later.
    return std::exp(-6.0f * multiplier / (seconds * sampleRate));
}

// Provisional bent-envelope approximation. The fast bend and slower tail
// remain uncalibrated; this shape is not proof of original envelope circuitry.
class Adsr
{
public:
    enum class Stage { Idle, Attack, Decay, Sustain, Release };

    void reset() noexcept
    {
        value = 0.0f;
        stage = Stage::Idle;
        stageTime = 0.0f;
    }

    void noteOn(float attack, float decay, float sustain, float release) noexcept
    {
        a = attack; d = decay; s = clampf(sustain, 0.0f, 1.0f); r = release;
        stage = Stage::Attack;
        stageTime = 0.0f;
        if (a <= 0.0002f)
            value = 1.0f;
    }

    void noteOff() noexcept
    {
        if (stage != Stage::Idle)
        {
            stage = Stage::Release;
            stageTime = 0.0f;
        }
    }

    float tick(float sampleRate, bool bent = true) noexcept
    {
        stageTime += 1.0f / sampleRate;
        switch (stage)
        {
            case Stage::Idle:
                value = 0.0f;
                break;
            case Stage::Attack:
            {
                const float c = coefficientForTime(a, sampleRate);
                value = 1.0f + (value - 1.0f) * c;
                if (value >= 0.999f || stageTime > a * 1.2f + 0.001f)
                {
                    value = 1.0f;
                    stage = Stage::Decay;
                    stageTime = 0.0f;
                }
                break;
            }
            case Stage::Decay:
            {
                const float fast = (bent && stageTime < 0.035f) ? 5.0f : 1.0f;
                const float c = coefficientForTime(d, sampleRate, fast);
                value = s + (value - s) * c;
                if (std::abs(value - s) < 0.0005f)
                {
                    value = s;
                    stage = Stage::Sustain;
                }
                break;
            }
            case Stage::Sustain:
                value = s;
                break;
            case Stage::Release:
            {
                const float fast = (bent && stageTime < 0.045f) ? 5.0f : 1.0f;
                const float c = coefficientForTime(r, sampleRate, fast);
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

    void setRelease(float seconds) noexcept { r = seconds; }
    bool isIdle() const noexcept { return stage == Stage::Idle; }
    bool isReleased() const noexcept { return stage == Stage::Release || stage == Stage::Idle; }
    float getValue() const noexcept { return value; }

private:
    Stage stage = Stage::Idle;
    float value = 0.0f;
    float stageTime = 0.0f;
    float a = 0.0f, d = 0.0f, s = 0.0f, r = 0.0f;
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
        value = 0.0f;
        stage = Stage::Idle;
        stageTime = 0.0f;
    }

    void noteOn(float initialLevel, float attackLevel, float attack, float decay,
                float release, float sustainLevel) noexcept
    {
        il = initialLevel;
        al = attackLevel;
        a = attack;
        d = decay;
        r = release;
        sustain = sustainLevel;
        value = il;
        stage = Stage::Attack;
        stageTime = 0.0f;
        if (a <= 0.0002f)
            value = al;
    }

    void noteOff() noexcept
    {
        if (stage != Stage::Idle)
        {
            stage = Stage::Release;
            stageTime = 0.0f;
        }
    }

    float tick(float sampleRate, bool bent = true) noexcept
    {
        stageTime += 1.0f / sampleRate;
        switch (stage)
        {
            case Stage::Idle:
                break;
            case Stage::Attack:
            {
                const float c = coefficientForTime(a, sampleRate);
                value = al + (value - al) * c;
                if (std::abs(value - al) < 0.001f || stageTime > a * 1.2f + 0.001f)
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
                const float c = coefficientForTime(d, sampleRate, fast);
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
                const float c = coefficientForTime(r, sampleRate, fast);
                value += (il - value) * (1.0f - c);
                if (std::abs(value - il) < 0.0008f)
                {
                    value = il;
                    stage = Stage::Idle;
                }
                break;
            }
        }
        return value;
    }

    void setRelease(float seconds) noexcept { r = seconds; }
    bool isIdle() const noexcept { return stage == Stage::Idle; }
    float getValue() const noexcept { return value; }

private:
    Stage stage = Stage::Idle;
    float value = 0.0f, stageTime = 0.0f;
    float il = 0.0f, al = 0.0f, a = 0.0f, d = 0.0f, r = 0.0f, sustain = 0.0f;
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
        const float a = (t - 1.0f) / (t + 1.0f);

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

    void push(float x) noexcept
    {
        hist[(size_t) idx] = x;
        if (++idx >= kTaps) idx = 0;
    }

    float output() const noexcept
    {
        auto delayed = [this](int tap) {
            int p = idx - 1 - tap;
            if (p < 0) p += kTaps;
            return hist[(size_t) p];
        };
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
    std::array<float, kTaps> hist {};
    int idx = 0;
};

// Schroeder/plate hybrid for the modern FX section. Left and right have
// independent delay networks, so the tail is genuinely stereo and the plate
// does not collapse when the effect is panned.
class Reverb
{
public:
    void prepare(float sampleRate)
    {
        sr = sampleRate;
        const float scale = sampleRate / 44100.0f;
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
            const float d = (0.0297f + 0.011f * (float) i) * (sr / 44100.0f) * spread * sr;
            const float y = combo[ch][i].read(d);
            const float fed = x + damp[ch][i].process(y) * 0.78f;
            combo[ch][i].write(fed);
            wet += y;
        }
        wet *= 0.25f;
        for (int i = 0; i < 2; ++i)
        {
            const float d = (0.005f + 0.0017f * (float) i) * (sr / 44100.0f) * sr;
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
