#pragma once

#include "Dsp.h"

#include <cmath>
#include <vector>

// OE2 analog-delay chorus for the source policies (Source Performance and
// Isolation). Evidence, assumptions and the audible change are in
// docs/BBD_CHORUS_MODEL.md. In short, the delay path follows the printed OE2
// graph: IC3 input low-pass -> MN3001 (two tied-input 512-stage chains, 256 clock
// periods of delay) -> 1 uF / 100 k output coupling -> IC5/IC6 low-pass ->
// 0.1 uF / 10 k interstage coupling. Every value tagged INFERRED below is an
// engineering choice, not a recovered CS-80 law.
namespace cs80
{

class BbdChorus
{
public:
    // MN3001: 512 stages per chain, two clock phases per period (Panasonic BBD handbook).
    static constexpr double kClockPeriodsPerDelay = 256.0;

    // INFERRED: nominal clock = geometric centre of the 7-12.5 us TP1 period interval
    // that the CS-80 adjustment procedure allows (BA617_SOURCE_REVIEW.md); the installed
    // VR1 setting is unknown. 1e6 / sqrt(7 * 12.5).
    static constexpr double kNominalClockHz = 106904.5;

    // INFERRED: full DEPTH moves the clock by +/- half an octave, which spans roughly the
    // same 1.8-3.2 ms delay interval as the adjustment range. The real BA617 control law
    // and LFO-to-clock summing resistors are not recovered.
    static constexpr float kClockModulationOctaves = 0.5f;

    // INFERRED: wet sum level and polarity at OE1. The IC5/IC6 inverting stage is printed;
    // the VCA and OE1 summing polarities are not, so the net sign is unresolved.
    static constexpr float kWetGain = 0.5f;
    static constexpr float kWetPolarity = -1.0f;

    // Printed OE2 values (OE2_FILTER_REFERENCE_DESIGN.md): R1 = R2 = 10 k, Cf = 1.8 nF,
    // Cg = 680 pF; 0.1 uF into 10 k; 1 uF into 100 k.
    static double lowPassHz() noexcept
    {
        return 1.0 / (kTwoPi * std::sqrt(1.0e4 * 1.0e4 * 1.8e-9 * 680.0e-12));
    }
    static double lowPassQ() noexcept
    {
        return std::sqrt(1.0e4 * 1.0e4 * 1.8e-9 * 680.0e-12) / (680.0e-12 * 2.0e4);
    }
    static constexpr double kInterstageHighPassHz = 159.1549;   // 1 / (2 pi 0.1 uF 10 k)
    static constexpr double kOutputCouplingHz = 1.591549;       // 1 / (2 pi 1 uF 100 k)

    void prepare(float sampleRate)
    {
        sr = sampleRate;
        // Longest delay is 256 / (nominal clock * 2^-0.5) = 3.4 ms; keep ample headroom.
        std::size_t size = 64;
        while ((double) size < 0.01 * (double) sr + 8.0)
            size <<= 1;
        samples.assign(size, 0.0f);
        stamps.assign(size, 0.0);
        mask = size - 1;
        inputLp.set((float) lowPassHz(), (float) lowPassQ(), sr);
        reconLp.set((float) lowPassHz(), (float) lowPassQ(), sr);
        couplingHp.set((float) kOutputCouplingHz, sr);
        interstageHp.set((float) kInterstageHighPassHz, sr);
        legacy.prepare(sr);
        reset();
    }

    void reset()
    {
        legacy.reset();
        inputLp.reset();
        reconLp.reset();
        couplingHp.reset();
        interstageHp.reset();
        lfoPhase = 0.0f;
        clockPeriods = 0.0;
        if (samples.empty())
            return;
        // Empty chain: zeros, with history stamped at the nominal clock so the read
        // pointer starts where 256 periods ago would be.
        const double perSample = kNominalClockHz / (double) sr;
        const std::size_t size = samples.size();
        for (std::size_t k = 0; k < size; ++k)
        {
            samples[k] = 0.0f;
            stamps[k] = -(double) (size - 1 - k) * perSample;
        }
        newest = (long long) size - 1;
        readIndex = newest - (long long) std::ceil(kClockPeriodsPerDelay / perSample);
    }

    using Stereo = Chorus::Stereo;

    // Same contract as Chorus::process. The delay path and its oscillator keep running
    // while the effect is off or in tremolo mode, as the OE2 clock does.
    Stereo process(float inL, float inR, float speed, float depth, bool on, bool tremolo) noexcept
    {
        lfoPhase += speed / sr;
        lfoPhase -= std::floor(lfoPhase);

        // Chorus tab: the oscillator moves the BBD clock. With the tabs off or in tremolo
        // (VCA modulation) the clock stays at its nominal rate.
        float octaves = 0.0f;
        if (on && ! tremolo)
            octaves = clampf(depth, 0.0f, 1.0f) * kClockModulationOctaves * std::sin(kTwoPi * lfoPhase);
        clockPeriods += kNominalClockHz * std::exp2((double) octaves) / (double) sr;

        // Both MN3001 chains share input and clock on OE57, so one chain stands for both.
        ++newest;
        samples[(std::size_t) newest & mask] = inputLp.process(0.5f * (inL + inR));
        stamps[(std::size_t) newest & mask] = clockPeriods;

        // The packet leaving the chain now entered when the clock count was 256 periods
        // lower. A time-varying clock is integrated, not treated as an instantaneous delay.
        const double target = clockPeriods - kClockPeriodsPerDelay;
        const long long oldest = newest - (long long) samples.size() + 2;
        if (readIndex < oldest)
            readIndex = oldest;
        while (readIndex + 2 < newest && stamps[(std::size_t) (readIndex + 1) & mask] <= target)
            ++readIndex;
        const double s0 = stamps[(std::size_t) readIndex & mask];
        const double s1 = stamps[(std::size_t) (readIndex + 1) & mask];
        const float t = (float) clampf((float) ((target - s0) / (s1 - s0)), 0.0f, 1.0f);
        const float y0 = samples[(std::size_t) (readIndex - 1) & mask];
        const float y1 = samples[(std::size_t) readIndex & mask];
        const float y2 = samples[(std::size_t) (readIndex + 1) & mask];
        const float y3 = samples[(std::size_t) (readIndex + 2) & mask];
        // Catmull-Rom between the two stored packets that straddle the target.
        const float c1 = 0.5f * (y2 - y0);
        const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        float wet = ((c3 * t + c2) * t + c1) * t + y1;

        wet -= couplingHp.process(wet);          // output coupling: high-pass = x - low-pass
        wet = reconLp.process(wet);
        wet -= interstageHp.process(wet);

        if (! on)
            return { inL, inR };
        if (tremolo)
            return legacy.process(inL, inR, speed, depth, true, true);
        wet *= kWetGain * kWetPolarity;
        return { inL + wet, inR + wet };
    }

private:
    // Second-order low-pass with the Sallen-Key transfer 1 / (1 + s/(w0 Q) + s^2/w0^2).
    // Linear: the engine's Svf adds a soft clip that the printed follower does not have.
    class LowPass2
    {
    public:
        void set(float hz, float q, float sampleRate) noexcept
        {
            const float g = std::tan(kPi * clampf(hz, 18.0f, sampleRate * 0.45f) / sampleRate);
            const float k = 1.0f / q;
            a1 = 1.0f / (1.0f + g * (g + k));
            a2 = g * a1;
            a3 = g * a2;
        }
        void reset() noexcept { ic1 = ic2 = 0.0f; }
        float process(float x) noexcept
        {
            const float v3 = x - ic2;
            const float v1 = a1 * ic1 + a2 * v3;
            const float v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = 2.0f * v1 - ic1;
            ic2 = 2.0f * v2 - ic2;
            return v2;
        }
    private:
        float a1 = 0.0f, a2 = 0.0f, a3 = 0.0f, ic1 = 0.0f, ic2 = 0.0f;
    };

    float sr = 48000.0f;
    float lfoPhase = 0.0f;
    double clockPeriods = 0.0;                 // BBD clock periods elapsed
    std::vector<float> samples;                // input packets, newest at `newest`
    std::vector<double> stamps;                // clock count when each packet entered
    std::size_t mask = 0;
    long long newest = 0;
    long long readIndex = 0;
    LowPass2 inputLp, reconLp;
    OnePole couplingHp, interstageHp;          // used as x - lowpass(x)
    Chorus legacy;                             // tremolo keeps the generic amplitude law
};

} // namespace cs80
