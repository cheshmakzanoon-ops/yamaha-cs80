#pragma once
// Circuit-engine noise sources (IMPROVEMENT_PLAN Phase 3, noise). Realtime
// counterpart of src/reference/NoiseSpectrum.h, which holds the declared PSDs,
// the component values they come from and the alternatives considered.
//
// Shared versus per voice (declared model, docs/workstreams/ws2-engine.md):
// - Panel WHITE NOISE: ONE generator on the SUB board feeds the rank I and II
//   level VCAs and from there all eight voice cards (C1/C2 -> NO1/NO2 -> M-board
//   NI43), so every voice of the circuit engine receives the same noise sample,
//   scaled by its rank's NOISE level. Source-derived fan-out.
// - Background hiss (ANALOG NOISE overlay): mix-bus / output op-amp noise. One
//   independent source per output channel, shared by all voices.
// - Not modelled: per-voice device noise of the IG00156/IG00151 circuits
//   (independent per card, gated by each VCA); declared below the bus floor.
// Legacy keeps its historical independent uniform draws from the shared RNG.
#include "Dsp.h"

#include <array>
#include <cmath>
#include <cstdint>

namespace cs80
{
namespace noise_design
{
inline constexpr double pi = 3.14159265358979323846;
// Corners of the SUB29 generator chain from its component values; the
// gain-bandwidth product is an INFERRED NJM4558-class typical (3 MHz).
inline constexpr double kGainBandwidthHz = 3.0e6;
inline constexpr double kCouplingHz = 1.0 / (2.0 * pi * 0.047e-6 * 100.0e3);   // 33.86 Hz
inline constexpr double kStage1Hz = kGainBandwidthHz / (1.0 + 1.0e6 / 4.7e3);  // 14.03 kHz
inline constexpr double kInterstageHz = 1.0 / (2.0 * pi * 10.0e-6 * 10.0e3);   // 1.59 Hz
inline constexpr double kStage2Hz = kGainBandwidthHz / (1.0 + 10.0e6 / 10.0e3); // 3.00 kHz
// Mix-bus hiss: white with a ~100 Hz flicker corner, two low shelves.
inline constexpr double kHissZero1Hz = 15.0, kHissPole1Hz = 5.0;
inline constexpr double kHissZero2Hz = 160.0, kHissPole2Hz = 80.0;
// Both sources keep the audio-band power of the previous circuit engine at a
// 48 kHz host: variance 1/3 white over 0..48 kHz, measured 20 Hz..20 kHz.
inline constexpr double kBandLowHz = 20.0, kBandHighHz = 20000.0;
inline constexpr double kReferenceBandPower = (1.0 / 3.0) * (kBandHighHz - kBandLowHz) / 48000.0;

inline double panelShape(double f) noexcept
{
    const auto hp = [f](double c) { const double r = (f / c) * (f / c); return r / (1.0 + r); };
    const auto lp = [f](double c) { return 1.0 / (1.0 + (f / c) * (f / c)); };
    return hp(kCouplingHz) * lp(kStage1Hz) * hp(kInterstageHz) * lp(kStage2Hz);
}
inline double hissShape(double f) noexcept
{
    const auto shelf = [f](double z, double p) { return (f * f + z * z) / (f * f + p * p); };
    return shelf(kHissZero1Hz, kHissPole1Hz) * shelf(kHissZero2Hz, kHissPole2Hz);
}
// One-sided white density N0 that gives the reference band power through shape.
template <class Shape>
double whiteDensity(Shape shape) noexcept
{
    constexpr int n = 4000;
    const double a = std::log(kBandLowHz), b = std::log(kBandHighHz), h = (b - a) / n;
    double sum = 0.0;
    for (int i = 0; i <= n; ++i)
    {
        const double f = std::exp(a + i * h);
        sum += (i == 0 || i == n ? 1.0 : (i % 2 ? 4.0 : 2.0)) * shape(f) * f;
    }
    return kReferenceBandPower / (sum * h / 3.0);
}
} // namespace noise_design

// y = b0 x + b1 x[-1] + a1 y[-1]; pole matched exactly (z = exp(-2 pi fc / fs)).
struct FirstOrderSection
{
    double b0 = 1.0, b1 = 0.0, a1 = 0.0, x1 = 0.0, y1 = 0.0;
    double process(double x) noexcept
    {
        const double y = b0 * x + b1 * x1 + a1 * y1;
        x1 = x;
        y1 = y;
        return y;
    }
    void reset() noexcept { x1 = y1 = 0.0; }

    // s / (s + w): zero at DC, unity at Nyquist (corners far below Nyquist).
    static FirstOrderSection highPass(double fc, double fs) noexcept
    {
        const double a = std::exp(-2.0 * noise_design::pi * fc / fs), k = 0.5 * (1.0 + a);
        return { k, -k, a };
    }
    // (s + wz) / (s + wp): unity at Nyquist, wz / wp at DC.
    static FirstOrderSection shelf(double fz, double fp, double fs) noexcept
    {
        const double a = std::exp(-2.0 * noise_design::pi * fp / fs);
        const double b = std::exp(-2.0 * noise_design::pi * fz / fs);
        const double k = (1.0 + a) / (1.0 + b);
        return { k, -k * b, a };
    }
    // w / (s + w): unity at DC, and a zero placed so the magnitude also equals
    // the analog one at fm = min(20 kHz, 0.4 fs). Within 0.03 dB of the analog
    // pole over 20 Hz..20 kHz at every internal rate from 88.2 kHz.
    static FirstOrderSection lowPass(double fc, double fs) noexcept
    {
        const double a = std::exp(-2.0 * noise_design::pi * fc / fs);
        const double fm = std::min(20000.0, 0.4 * fs);
        const double c = std::cos(2.0 * noise_design::pi * fm / fs);
        const double target = 1.0 / (1.0 + (fm / fc) * (fm / fc));
        const double q = target * (1.0 + a * a - 2.0 * a * c) / ((1.0 - a) * (1.0 - a));
        // |H(fm)|^2 = target  <=>  (1 - q) b^2 + 2 (c - q) b + (1 - q) = 0
        double b = 0.0;
        const double A = 1.0 - q, B = 2.0 * (c - q), disc = B * B - 4.0 * A * A;
        if (std::abs(A) > 1.0e-15 && disc >= 0.0)
        {
            const double r1 = (-B + std::sqrt(disc)) / (2.0 * A), r2 = (-B - std::sqrt(disc)) / (2.0 * A);
            b = std::abs(r1) < 1.0 ? r1 : (std::abs(r2) < 1.0 ? r2 : 0.0);
        }
        const double g = (1.0 - a) / (1.0 + b);
        return { g, g * b, a };
    }
};

// Deterministic Gaussian samples (Marsaglia polar method on a 32-bit xorshift).
class GaussianNoise
{
public:
    explicit GaussianNoise(std::uint32_t seed) noexcept : initialSeed(seed), rng(seed) {}
    void reset() noexcept { rng = Rng(initialSeed); hasSpare = false; }
    double next() noexcept
    {
        if (hasSpare) { hasSpare = false; return spare; }
        double u = 0.0, v = 0.0, s = 0.0;
        do
        {
            u = 2.0 * unit() - 1.0;
            v = 2.0 * unit() - 1.0;
            s = u * u + v * v;
        } while (s >= 1.0 || s == 0.0);
        const double m = std::sqrt(-2.0 * std::log(s) / s);
        spare = v * m;
        hasSpare = true;
        return u * m;
    }

private:
    double unit() noexcept { return ((double) rng.nextUInt() + 0.5) * (1.0 / 4294967296.0); }
    std::uint32_t initialSeed;
    Rng rng;
    double spare = 0.0;
    bool hasSpare = false;
};

class CircuitNoiseSources
{
public:
    CircuitNoiseSources() noexcept { prepare(96000.0); }

    // Bounded work, no allocation: safe on the render thread (quality change).
    void prepare(double sampleRate) noexcept
    {
        using namespace noise_design;
        rate = sampleRate;
        panelChain = { FirstOrderSection::highPass(kCouplingHz, rate), FirstOrderSection::lowPass(kStage1Hz, rate),
                       FirstOrderSection::highPass(kInterstageHz, rate), FirstOrderSection::lowPass(kStage2Hz, rate) };
        for (auto* chain : { &hissLeft, &hissRight })
            *chain = { FirstOrderSection::shelf(kHissZero1Hz, kHissPole1Hz, rate),
                       FirstOrderSection::shelf(kHissZero2Hz, kHissPole2Hz, rate) };
        // White samples of variance N0 * fs / 2 carry one-sided density N0 up to
        // fs / 2, so the in-band PSD does not depend on host rate or factor.
        panelSigma = std::sqrt(panelDensity() * rate * 0.5);
        hissSigma = std::sqrt(hissDensity() * rate * 0.5);
        reset();
    }
    void reset() noexcept
    {
        for (auto* g : { &panelGen, &hissGenL, &hissGenR }) g->reset();
        for (auto& s : panelChain) s.reset();
        for (auto& s : hissLeft) s.reset();
        for (auto& s : hissRight) s.reset();
    }
    // One sample of the shared SUB29 generator (every voice reads the same one).
    float panel() noexcept
    {
        double y = panelSigma * panelGen.next();
        for (auto& s : panelChain) y = s.process(y);
        return (float) y;
    }
    // One sample of each channel's independent mix-bus hiss.
    void hiss(float& left, float& right) noexcept
    {
        double l = hissSigma * hissGenL.next(), r = hissSigma * hissGenR.next();
        for (auto& s : hissLeft) l = s.process(l);
        for (auto& s : hissRight) r = s.process(r);
        left = (float) l;
        right = (float) r;
    }
    double sampleRate() const noexcept { return rate; }

    static double panelDensity() noexcept
    {
        static const double n0 = noise_design::whiteDensity(noise_design::panelShape);
        return n0;
    }
    static double hissDensity() noexcept
    {
        static const double n0 = noise_design::whiteDensity(noise_design::hissShape);
        return n0;
    }

private:
    GaussianNoise panelGen { 0x5ee2b0a1u }, hissGenL { 0x71c3d2e5u }, hissGenR { 0x2a9f6b17u };
    std::array<FirstOrderSection, 4> panelChain {};
    std::array<FirstOrderSection, 2> hissLeft {}, hissRight {};
    double rate = 96000.0, panelSigma = 0.0, hissSigma = 0.0;
};
} // namespace cs80
