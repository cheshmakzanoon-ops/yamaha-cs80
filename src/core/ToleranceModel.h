#pragma once
// Plan step 2.5: per-card component tolerances and thermal drift for the
// circuit engine (Source Performance). Legacy keeps its historical uniform
// trims and 0.031 Hz sine drift; PURE CS-80 masks VOICE DISPERSION and ANALOG
// DRIFT to zero, so every ratio below is exactly 1 and the drift exactly 0.
// Gate: tests/ToleranceDriftTests.cpp, limits pre-registered in
// tests/parity/tolerances-v2/ws1-voice.json ("tolerance_drift").
//
// Evidence classes (docs/IG00151_VCA_MODEL.md section "Tolerances and drift"):
// - DECLARED distributions: 5 % resistors and 10 % capacitors, truncated normal
//   with sigma = tolerance / 2, truncated at +/- the tolerance. The CS-80 parts
//   lists give values, not tolerance classes; these are the common classes of
//   the era's carbon-film resistors and film/ceramic capacitors.
// - Mapping laws (from the circuit topology; the components are source-derived,
//   their deviations declared):
//     IG00156 natural frequency  ~ 1 / sqrt(C1 C2)  (two-integrator loop)
//     IG00156 Q                  ~ sqrt(C1 / C2)     (damping on the BP
//                                  integrator; DECLARED, the inverse ratio is
//                                  the alternative)
//     IG00151 level              ~ Rf / Rin          (LC summing node)
//     IG00152/IG00159 times      ~ R x C             (timing R x 0.22 uF)
//   per stage / per envelope generator, independent draws per voice channel.
// - Tune: the VCOs are aligned on the bench (service manual VCO trims), so the
//   component spread is trimmed out; what remains is a DECLARED alignment
//   residual, sigma 1 cent, truncated at 2 cents.
// - VOICE DISPERSION scales every deviation: its default 0.35 is the declared
//   distribution (scale 1); 0 gives identical cards; 1 is about 2.9 x wider,
//   an unaligned or aged unit.
// - Thermal drift: each channel's exponential converter wanders as 1/f noise
//   (six first-order sections, time constants 0.5 s .. 512 s, equal variance),
//   declared 2 cents RMS at the ANALOG DRIFT default 0.25, scaled linearly; plus
//   a warm-up offset (sigma 3 cents at the default) decaying with 120 s from
//   prepare(). Nothing here is a measurement of a CS-80.
// - The historical +/- 22 cent 0.031 Hz sine stays available as the "worn unit"
//   drift model (DRIFT MODEL = 1), sample for sample.
// Deterministic: draws and noise come from fixed per-channel seeds, independent
// of host rate, oversampling and block size.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace cs80::tolerance
{
inline constexpr double kResistorTolerance = 0.05;
inline constexpr double kCapacitorTolerance = 0.10;
inline constexpr double kTuneResidualSigmaCents = 1.0;
inline constexpr float kDefaultDispersion = 0.35f;   // VOICE DISPERSION default = declared distribution
inline constexpr float kDefaultDrift = 0.25f;        // ANALOG DRIFT default = declared thermal drift
inline constexpr double kDriftRmsCentsAtDefault = 2.0;
inline constexpr double kWarmupSigmaCentsAtDefault = 3.0;
inline constexpr double kWarmupSeconds = 120.0;
inline constexpr double kDriftControlHz = 100.0;
inline constexpr int kDriftSections = 6;
inline constexpr std::array<double, kDriftSections> kDriftTauSeconds { 0.5, 2.0, 8.0, 32.0, 128.0, 512.0 };
inline constexpr std::uint64_t kCardSeed = 0xC580D157A11CE5ull;
inline constexpr std::uint64_t kDriftSeed = 0xC580D21F7B0A7Dull;

// SplitMix64: small, seedable, identical on every platform.
class SeedStream
{
public:
    explicit SeedStream(std::uint64_t seed) noexcept : state(seed) {}
    std::uint64_t next() noexcept
    {
        std::uint64_t z = (state += 0x9E3779B97F4A7C15ull);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        return z ^ (z >> 31);
    }
    // Uniform in (0, 1).
    double uniform() noexcept { return ((double) (next() >> 11) + 0.5) * (1.0 / 9007199254740992.0); }
    // Standard normal (Box-Muller, both values used in turn).
    double normal() noexcept
    {
        if (hasSpare) { hasSpare = false; return spare; }
        const double u1 = uniform(), u2 = uniform();
        const double r = std::sqrt(-2.0 * std::log(u1)), a = 6.283185307179586 * u2;
        spare = r * std::sin(a);
        hasSpare = true;
        return r * std::cos(a);
    }
    // Standard normal truncated at +/- limit standard deviations (rejection).
    double truncatedNormal(double limit) noexcept
    {
        for (;;)
        {
            const double x = normal();
            if (std::abs(x) <= limit)
                return x;
        }
    }

private:
    std::uint64_t state;
    double spare = 0.0;
    bool hasSpare = false;
};

// One voice channel's component deviations (relative, e.g. +0.03 = +3 %).
struct CardDraws
{
    double hpC1 = 0.0, hpC2 = 0.0, lpC1 = 0.0, lpC2 = 0.0;   // integrator capacitors, 10 %
    double levelRf = 0.0, levelRin = 0.0;                    // LC summing resistors, 5 %
    double filterEnvR = 0.0, filterEnvC = 0.0;               // IG00152 timing R, 0.22 uF
    double ampEnvR = 0.0, ampEnvC = 0.0;                     // IG00159 timing R, 0.22 uF
    double tune = 0.0;                                       // alignment residual, standard units
};

inline double component(SeedStream& s, double tolerance) noexcept
{
    // sigma = tol / 2, truncated at +/- tol (2 sigma).
    return 0.5 * tolerance * s.truncatedNormal(2.0);
}

inline CardDraws drawCard(int channelIndex) noexcept
{
    SeedStream s(kCardSeed + 0x1000193ull * (std::uint64_t) (channelIndex + 1));
    CardDraws d;
    d.hpC1 = component(s, kCapacitorTolerance);
    d.hpC2 = component(s, kCapacitorTolerance);
    d.lpC1 = component(s, kCapacitorTolerance);
    d.lpC2 = component(s, kCapacitorTolerance);
    d.levelRf = component(s, kResistorTolerance);
    d.levelRin = component(s, kResistorTolerance);
    d.filterEnvR = component(s, kResistorTolerance);
    d.filterEnvC = component(s, kCapacitorTolerance);
    d.ampEnvR = component(s, kResistorTolerance);
    d.ampEnvC = component(s, kCapacitorTolerance);
    d.tune = s.truncatedNormal(2.0);
    return d;
}

// The multipliers a channel applies (all exactly 1, tune exactly 0, at scale 0).
struct Ratios
{
    float hpCutoff = 1.0f, lpCutoff = 1.0f, hpQ = 1.0f, lpQ = 1.0f;
    float level = 1.0f, filterEnvTime = 1.0f, ampEnvTime = 1.0f;
    float tuneSemitones = 0.0f;
};

// Unscaled mapping laws (scale 1).
inline double cutoffRatio(double c1, double c2) noexcept { return 1.0 / std::sqrt((1.0 + c1) * (1.0 + c2)); }
inline double qRatio(double c1, double c2) noexcept { return std::sqrt((1.0 + c1) / (1.0 + c2)); }
inline double levelRatio(double rf, double rin) noexcept { return (1.0 + rf) / (1.0 + rin); }
inline double timeRatio(double r, double c) noexcept { return (1.0 + r) * (1.0 + c); }

// scale = VOICE DISPERSION / kDefaultDispersion. A ratio is raised to the scale,
// so 0 gives exactly 1 and the default gives the declared distribution.
inline Ratios ratios(const CardDraws& d, double scale) noexcept
{
    Ratios r;
    if (! (scale > 0.0))
        return r;
    const auto raise = [scale](double ratio) { return (float) std::pow(ratio, scale); };
    r.hpCutoff = raise(cutoffRatio(d.hpC1, d.hpC2));
    r.lpCutoff = raise(cutoffRatio(d.lpC1, d.lpC2));
    r.hpQ = raise(qRatio(d.hpC1, d.hpC2));
    r.lpQ = raise(qRatio(d.lpC1, d.lpC2));
    r.level = raise(levelRatio(d.levelRf, d.levelRin));
    r.filterEnvTime = raise(timeRatio(d.filterEnvR, d.filterEnvC));
    r.ampEnvTime = raise(timeRatio(d.ampEnvR, d.ampEnvC));
    r.tuneSemitones = (float) (scale * d.tune * kTuneResidualSigmaCents / 100.0);
    return r;
}

// Thermal drift of one channel, in cents at the ANALOG DRIFT default: a warm
// 1/f process (stationary from the start) plus the decaying warm-up offset.
// Advanced at kDriftControlHz by a sample counter, linearly interpolated, so
// the result does not depend on the host's block partition.
class ThermalDrift
{
public:
    void prepare(double internalRate, int channelIndex) noexcept
    {
        stepSamples = std::max(1, (int) std::lround(internalRate / kDriftControlHz));
        rng = SeedStream(kDriftSeed + 0x9E37ull * (std::uint64_t) (channelIndex + 1));
        const double sigma = kDriftRmsCentsAtDefault / std::sqrt((double) kDriftSections);
        for (int k = 0; k < kDriftSections; ++k)
        {
            rho[(size_t) k] = std::exp(-1.0 / (kDriftControlHz * kDriftTauSeconds[(size_t) k]));
            innovation[(size_t) k] = sigma * std::sqrt(1.0 - rho[(size_t) k] * rho[(size_t) k]);
            state[(size_t) k] = sigma * rng.normal();   // stationary start: a warm process
        }
        warmupStart = kWarmupSigmaCentsAtDefault * rng.normal();
        warmupDecay = std::exp(-1.0 / (kDriftControlHz * kWarmupSeconds));
        warmup = warmupStart;
        previous = current = total();
        counter = 0;
    }
    // Cents at the default drift amount for the present internal sample.
    float tick() noexcept
    {
        if (++counter >= stepSamples)
        {
            counter = 0;
            advance();
        }
        return (float) (previous + (current - previous) * ((double) counter / (double) stepSamples));
    }
    // Control-rate access for the gate: one control step, returns the new value.
    double advance() noexcept
    {
        previous = current;
        for (int k = 0; k < kDriftSections; ++k)
            state[(size_t) k] = rho[(size_t) k] * state[(size_t) k] + innovation[(size_t) k] * rng.normal();
        warmup *= warmupDecay;
        current = total();
        return current;
    }
    double warmComponent() const noexcept { double s = 0.0; for (double x : state) s += x; return s; }
    double warmupComponent() const noexcept { return warmup; }
    double warmupInitial() const noexcept { return warmupStart; }

private:
    double total() const noexcept { return warmComponent() + warmup; }
    SeedStream rng { kDriftSeed };
    std::array<double, kDriftSections> rho {}, innovation {}, state {};
    double warmupStart = 0.0, warmup = 0.0, warmupDecay = 1.0;
    double previous = 0.0, current = 0.0;
    int stepSamples = 960, counter = 0;
};
} // namespace cs80::tolerance
