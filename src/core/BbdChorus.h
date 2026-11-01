#pragma once

#include "Dsp.h"

#include <array>
#include <bit>
#include <cstdint>
#include <cmath>
#include <complex>
#include <vector>

// OE1/OE2 chorus and tremolo for the source policies (Source Performance and Isolation).
// Evidence, assumptions and the audible change are in docs/BBD_CHORUS_MODEL.md and
// docs/workstreams/ws4-circuits.md (Step 3). In short, following the printed OE2 graph:
//
//   mono I(53) -> IC3 input low-pass -> MN3001 (two tied-input 512-stage chains, shared
//   clock; per-transfer loss, two-slot output hold, independent noise per chain, clock
//   feedthrough) -> per chain: OUT1/OUT2 recombination (2 x 2.2 k) -> 1 uF / 100 k
//   coupling -> IC5 (upper) / IC6 (lower) low-pass -> 0.1 uF / 10 k interstage -> the
//   IG00151 VCAs O1,O2 (upper, left) and O3,O4 (lower, right) -> OE1 sum with the dry path.
//
// OE1 DO1/DO2 move the BBD clock in CHORUS and the four VCA gains in TREMOLO. Every value
// tagged INFERRED below is an engineering choice, not a recovered CS-80 law. Truth models:
// src/reference/Mn3001Packet.h (NonidealChannel), Oe2Filters.h, Oe1Modulation.h.
namespace cs80
{
namespace bbd_detail
{
// Count stored representations, not the caller's current floating-point mode.
// Both signed zeros interpolate to zero; subnormals and nonfinite values must
// keep the full path even if DAZ happens to make a floating comparison see zero.
inline bool hasNonzeroMagnitude(float sample) noexcept
{
    // Apple Clang 17 folds a non-volatile bit test back into fcmp/cmpneqss.
    // DAZ then treats a stored subnormal as zero, incorrectly dropping history.
    // Keep the classification in the integer domain; no floating law changes.
    const volatile std::uint32_t bits = std::bit_cast<std::uint32_t>(sample);
    return (bits & 0x7fffffffu) != 0u;
}
} // namespace bbd_detail

class BbdChorus
{
public:
    // MN3001: 512 stages per chain, two clock phases per period (Panasonic BBD handbook).
    // A packet acquired on CP2 reaches upper512 256 periods later (Mn3001Packet.h).
    static constexpr double kClockPeriodsPerDelay = 256.0;
    // OUT1 carries it until the next CP1 and OUT2 until the next CP2, so the recombined
    // output holds each packet for one full period: the effective delay centre is
    // 256.5 periods, and the hold is a one-period box (realized with Simpson weights).
    static constexpr double kHoldPeriods = 1.0;

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

    // INFERRED: charge-transfer inefficiency per transfer (typical of PMOS bucket brigades,
    // not an MN3001 datasheet value). 512 transfers from front storage to upper512.
    static constexpr int kTransfers = 512;
    static constexpr double kTransferInefficiency = 5.0e-4;
    // INFERRED: equivalent per-packet noise in engine units, independent in each chain
    // (about -79 dB below a 0.5-peak sine in the wet path).
    static constexpr double kPacketNoiseRms = 1.5e-4;
    // INFERRED: residual clock square-wave amplitude at the recombined node after the
    // complementary OUT1/OUT2 pedestals cancel. Ultrasonic: only the harmonics below the
    // internal band edge are synthesized, band-limited.
    static constexpr double kClockFeedthrough = 0.01;
    // OE57: OUT1/OUT2 recombined through equal 2.2 k; Thevenin from stiff pins (INFERRED
    // boundary), loading the 1 uF / 100 k coupling and the IC5/IC6 input resistor.
    static constexpr double kRecombinationOhms = 1100.0;

    // OE1, adjustment procedure printed 21: DO1 56 ms and DO2 0.55 s at maximum SPEED.
    // INFERRED: SPEED (Hz) is DO1's rate and DO2 tracks it at the documented ratio.
    static constexpr double kDo2PerDo1 = 0.056 / 0.55;
    // Same table: O1..O4 VCA amplitudes 0.65/0.58/0.58/0.65 Vpp. O1,O2 = upper chain (left),
    // O3,O4 = lower chain (right). INFERRED: trims referenced to a 1 kHz tone.
    static constexpr std::array<double, 4> kBranchVpp { 0.65, 0.58, 0.58, 0.65 };
    static constexpr double kTrimHz = 1000.0;

    // Printed OE2 values (OE2_FILTER_REFERENCE_DESIGN.md): R1 = R2 = 10 k, Cf = 1.8 nF,
    // Cg = 680 pF; 0.1 uF into 10 k; 1 uF into 100 k.
    static double lowPassHz() noexcept { return sallenKeyHz(1.0e4); }
    static double lowPassQ() noexcept { return sallenKeyQ(1.0e4); }
    // IC5/IC6 see the recombination Thevenin in series with R1 (through the coupling
    // capacitor, a short at audio frequencies).
    static double reconstructionR1() noexcept
    {
        return 1.0e4 + kRecombinationOhms * 1.0e5 / (kRecombinationOhms + 1.0e5);
    }
    static double reconstructionHz() noexcept { return sallenKeyHz(reconstructionR1()); }
    static double reconstructionQ() noexcept { return sallenKeyQ(reconstructionR1()); }
    static constexpr double kInterstageHighPassHz = 159.1549;   // 1 / (2 pi 0.1 uF 10 k)
    static double outputCouplingHz() noexcept                   // 1 uF into 100 k + 1.1 k
    {
        return 1.0 / (kTwoPi * (1.0e5 + kRecombinationOhms) * 1.0e-6);
    }

    // Delay-tap offsets (clock periods behind the write count) and weights: per-transfer
    // loss (negative binomial over kTransfers, first three terms renormalized) convolved
    // with the one-period output hold (Simpson 1/6, 2/3, 1/6 at 256, 256.5, 257 periods).
    static constexpr int kLossTerms = 3;
    static constexpr int kTaps = 2 * kLossTerms + 1;
    static double tapOffset(int tap) noexcept { return kClockPeriodsPerDelay + 0.5 * tap; }
    static std::array<double, kTaps> tapWeights() noexcept
    {
        std::array<double, kLossTerms> loss {};
        double term = std::pow(1.0 - kTransferInefficiency, kTransfers), total = 0.0;
        for (int k = 0; k < kLossTerms; ++k)
        {
            loss[(std::size_t) k] = term;
            total += term;
            term *= (double) (kTransfers + k) / (double) (k + 1) * kTransferInefficiency;
        }
        std::array<double, kTaps> w {};
        for (int k = 0; k < kLossTerms; ++k)
        {
            const double a = loss[(std::size_t) k] / total;
            w[(std::size_t) (2 * k)] += a / 6.0;
            w[(std::size_t) (2 * k + 1)] += a * 2.0 / 3.0;
            w[(std::size_t) (2 * k + 2)] += a / 6.0;
        }
        return w;
    }

    void prepare(float sampleRate)
    {
        sr = sampleRate;
        // The unmodulated clock has the same noise normalization on every sample.
        // Cache that exact expression; a moving CHORUS clock still evaluates it below.
        nominalNoiseScale = kPacketNoiseRms * 1.7320508075688772 * std::sqrt((double) sr / kNominalClockHz);
        // Longest tap is 259 / (nominal clock * 2^-0.5) = 3.4 ms; keep ample headroom.
        std::size_t size = 64;
        while ((double) size < 0.01 * (double) sr + 8.0)
            size <<= 1;
        samples.assign(size, 0.0f);
        stamps.assign(size, 0.0);
        mask = size - 1;
        weights = tapWeights();
        inputLp.set((float) lowPassHz(), (float) lowPassQ(), sr);
        for (auto& c : chains)
        {
            c.reconLp.set((float) reconstructionHz(), (float) reconstructionQ(), sr);
            c.couplingHp.set((float) outputCouplingHz(), sr);
            c.interstageHp.set((float) kInterstageHighPassHz, sr);
        }
        trim = (float) (1.0 / std::abs(declaredWetResponse(kTrimHz)));
        reset();
    }

    void reset()
    {
        inputLp.reset();
        for (auto& c : chains)
        {
            c.reconLp.reset();
            c.couplingHp.reset();
            c.interstageHp.reset();
        }
        noise = { Rng(0x4F453241u), Rng(0x4F453242u) };
        do1Phase = do2Phase = 0.0;
        clockPeriods = 0.0;
        nonzeroSamples = 0;
        if (samples.empty())
            return;
        // Empty chain: zeros, with history stamped at the nominal clock so each read
        // pointer starts where its tap delay ago would be.
        const double perSample = kNominalClockHz / (double) sr;
        const std::size_t size = samples.size();
        for (std::size_t k = 0; k < size; ++k)
        {
            samples[k] = 0.0f;
            stamps[k] = -(double) (size - 1 - k) * perSample;
        }
        newest = (long long) size - 1;
        for (int tap = 0; tap < kTaps; ++tap)
            cursors[(std::size_t) tap] = newest - (long long) std::ceil(tapOffset(tap) / perSample);
    }

    using Stereo = Chorus::Stereo;

    // Same contract as Chorus::process. The delay path, its noise and both oscillators keep
    // running while the effect is off, as the OE1/OE2 circuits do.
    Stereo process(float inL, float inR, float speed, float depth, bool on, bool tremolo) noexcept
    {
        return processIndependent(inL, inR, speed, depth, on && !tremolo, on && tremolo);
    }

    Stereo processIndependent(float inL, float inR, float speed, float depth,
                              bool chorus, bool tremolo) noexcept
    {
        const bool on = chorus || tremolo;
        do1Phase += (double) speed / (double) sr;
        do1Phase -= std::floor(do1Phase);
        do2Phase += (double) speed * kDo2PerDo1 / (double) sr;
        do2Phase -= std::floor(do2Phase);
        // Off tabs do not use the waveform values. Their exact phase recurrences
        // above still run; enabling either tab sees the same phase on that sample.
        const double do1 = on ? std::sin(2.0 * 3.14159265358979323846 * do1Phase) : 0.0;
        const double do2 = on ? std::sin(2.0 * 3.14159265358979323846 * do2Phase) : 0.0;
        const double d = (double) clampf(depth, 0.0f, 1.0f);

        // CHORUS: DO1 and DO2 reach the BA617 clock node through its two equal 100 k feeds.
        // With the tabs off or in TREMOLO the clock stays at its nominal rate.
        double octaves = 0.0;
        if (chorus)
            octaves = d * (double) kClockModulationOctaves * 0.5 * (do1 + do2);
        const double clockHz = chorus ? kNominalClockHz * std::exp2(octaves) : kNominalClockHz;
        clockPeriods += clockHz / (double) sr;

        // Both MN3001 chains share input and clock on OE57: one packet history serves both.
        ++newest;
        const auto write = (std::size_t) newest & mask;
        nonzeroSamples -= bbd_detail::hasNonzeroMagnitude(samples[write]) ? 1u : 0u;
        samples[write] = inputLp.process(0.5f * (inL + inR));
        nonzeroSamples += bbd_detail::hasNonzeroMagnitude(samples[write]) ? 1u : 0u;
        stamps[write] = clockPeriods;

        // A packet leaving the chain now entered 256 clock periods earlier; a moving clock is
        // integrated, not treated as an instantaneous delay. An exactly zero history has an
        // exactly zero interpolated sum. Still advance every cursor, so wake-up work stays
        // bounded and the same packet is read as soon as any nonzero sample enters.
        double packets = 0.0;
        if (nonzeroSamples == 0)
        {
            for (int tap = 0; tap < kTaps; ++tap)
                advanceCursor(clockPeriods - tapOffset(tap), cursors[(std::size_t) tap]);
        }
        else
            for (int tap = 0; tap < kTaps; ++tap)
                packets += weights[(std::size_t) tap] * readAt(clockPeriods - tapOffset(tap), cursors[(std::size_t) tap]);

        // Clock feedthrough residual: odd harmonics of a square at the clock, band-limited to
        // what the internal rate can carry (nothing at the usual 88.2-96 kHz internal rates).
        double feedthrough = 0.0;
        const double clockPhase = clockPeriods - std::floor(clockPeriods);
        for (int h = 1; h <= 3; h += 2)
        {
            const double taper = std::clamp((0.45 * sr - h * clockHz) / (0.05 * sr), 0.0, 1.0);
            if (taper > 0.0)
                feedthrough += taper * kClockFeedthrough * 4.0 / (3.14159265358979323846 * h)
                             * std::sin(2.0 * 3.14159265358979323846 * h * clockPhase);
        }

        // Each chain: its own noise, recombination, coupling and IC5/IC6 reconstruction.
        // Packet noise of variance s^2 at the clock rate has in-band density s^2 / clock.
        const double noiseScale = chorus
            ? kPacketNoiseRms * 1.7320508075688772 * std::sqrt((double) sr / clockHz)
            : nominalNoiseScale;
        std::array<float, 2> wet {};
        for (std::size_t c = 0; c < 2; ++c)
        {
            float x = (float) (packets + feedthrough + noiseScale * (double) noise[c].nextBipolar());
            x -= chains[c].couplingHp.process(x);  // output coupling: high-pass = x - low-pass
            x = chains[c].reconLp.process(x);
            x -= chains[c].interstageHp.process(x);
            wet[c] = x * trim;
        }

        if (! on)
            return { inL, inR };
        // IG00151 VCAs. TREMOLO: linear control, LO1 = +DO1, LO2 = +DO2 (left), LO3 = -DO2,
        // LO4 = -DO1 (right); index 1 at full DEPTH. CHORUS: trimmed static gains.
        const double m = tremolo ? d : 0.0;
        const double pair = kBranchVpp[0] + kBranchVpp[1];
        const double gainL = kWetGain / pair * (kBranchVpp[0] * (1.0 + m * do1) + kBranchVpp[1] * (1.0 + m * do2));
        const double gainR = kWetGain / pair * (kBranchVpp[2] * (1.0 - m * do2) + kBranchVpp[3] * (1.0 - m * do1));
        return { inL + kWetPolarity * (float) gainL * wet[0], inR + kWetPolarity * (float) gainR * wet[1] };
    }

    // Analog response of the declared wet path at the nominal clock, per unit VCA gain,
    // used for the 1 kHz trim. Also the "own specification" oracle for tests.
    static std::complex<double> declaredWetResponse(double hz) noexcept
    {
        const auto lowPass = [hz](double f0, double q)
        {
            const double w = hz / f0;
            return 1.0 / std::complex<double>(1.0 - w * w, w / q);
        };
        const auto highPass = [hz](double corner)
        {
            const std::complex<double> x(0.0, hz / corner);
            return x / (1.0 + x);
        };
        const auto w = tapWeights();
        std::complex<double> taps;
        for (int tap = 0; tap < kTaps; ++tap)
            taps += w[(std::size_t) tap] * std::polar(1.0, -2.0 * 3.14159265358979323846 * hz * tapOffset(tap) / kNominalClockHz);
        return lowPass(lowPassHz(), lowPassQ()) * taps * highPass(outputCouplingHz())
             * lowPass(reconstructionHz(), reconstructionQ()) * highPass(kInterstageHighPassHz);
    }
    float getTrim() const noexcept { return trim; }

private:
    static double sallenKeyHz(double r1) noexcept
    {
        return 1.0 / (kTwoPi * std::sqrt(r1 * 1.0e4 * 1.8e-9 * 680.0e-12));
    }
    static double sallenKeyQ(double r1) noexcept
    {
        return std::sqrt(r1 * 1.0e4 * 1.8e-9 * 680.0e-12) / (680.0e-12 * (r1 + 1.0e4));
    }

    void advanceCursor(double target, long long& cursor) noexcept
    {
        const long long oldest = newest - (long long) samples.size() + 2;
        if (cursor < oldest)
            cursor = oldest;
        while (cursor + 2 < newest && stamps[(std::size_t) (cursor + 1) & mask] <= target)
            ++cursor;
    }

    // Catmull-Rom between the two stored samples whose clock stamps straddle the target.
    float readAt(double target, long long& cursor) noexcept
    {
        advanceCursor(target, cursor);
        const double s0 = stamps[(std::size_t) cursor & mask];
        const double s1 = stamps[(std::size_t) (cursor + 1) & mask];
        const float t = clampf((float) ((target - s0) / (s1 - s0)), 0.0f, 1.0f);
        const float y0 = samples[(std::size_t) (cursor - 1) & mask];
        const float y1 = samples[(std::size_t) cursor & mask];
        const float y2 = samples[(std::size_t) (cursor + 1) & mask];
        const float y3 = samples[(std::size_t) (cursor + 2) & mask];
        const float c1 = 0.5f * (y2 - y0);
        const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * t + c2) * t + c1) * t + y1;
    }

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
    struct Chain
    {
        LowPass2 reconLp;
        OnePole couplingHp, interstageHp;      // used as x - lowpass(x)
    };

    float sr = 48000.0f;
    float trim = 1.0f;
    double do1Phase = 0.0, do2Phase = 0.0;
    double clockPeriods = 0.0;                 // BBD clock periods elapsed
    double nominalNoiseScale = 0.0;
    std::size_t nonzeroSamples = 0;            // exact history predicate, never a threshold
    std::vector<float> samples;                // input packets, newest at `newest`
    std::vector<double> stamps;                // clock count when each packet entered
    std::size_t mask = 0;
    long long newest = 0;
    std::array<long long, kTaps> cursors {};
    std::array<double, kTaps> weights {};
    LowPass2 inputLp;
    std::array<Chain, 2> chains;               // [0] upper / IC5 / left, [1] lower / IC6 / right
    std::array<Rng, 2> noise { Rng(0x4F453241u), Rng(0x4F453242u) };
};

} // namespace cs80
