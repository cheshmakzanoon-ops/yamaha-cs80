#pragma once
// OFFLINE truth model of the circuit engine's two noise sources: declared
// one-sided power spectral densities in double precision, straight from the
// component values. No production audio dependencies. See
// docs/workstreams/ws2-engine.md (Phase 3 noise) for evidence and alternatives.
#include <cmath>
#include <numbers>

namespace cs80::reference::noise
{
// Panel WHITE NOISE generator, SUB board (service manual p29, as recorded in
// docs/ORIGINAL_SUB_OSCILLATOR.md): Zener ZD 1S1715P biased through 150 k from
// +15 V, AC-coupled 0.047 uF into 100 k and IC1 (1 M / 4.7 k feedback), then
// 10 uF NP into a 10 k / 10 M inverting stage. The same generator drives the
// rank I/II level VCAs (C1/C2 -> NO1/NO2 -> M-board NI43; docs/
// FACTORY_TONE_INTEGRATION_PROPOSAL.md route 6) and the SUB noise function NO3.
struct PanelGenerator
{
    double couplingFarads = 0.047e-6;     // source-derived (SUB29)
    double inputOhms = 100.0e3;           // source-derived
    double stage1FeedbackOhms = 1.0e6;    // source-derived, non-inverting
    double stage1GroundOhms = 4.7e3;      // source-derived
    double interstageFarads = 10.0e-6;    // source-derived (NP)
    double stage2InputOhms = 10.0e3;      // source-derived, inverting
    double stage2FeedbackOhms = 10.0e6;   // source-derived
    // INFERRED: IC1/IC2 taken as NJM4558-class (the type SUB30 names for
    // IC6/IC12 on the same board), datasheet typical gain-bandwidth product.
    // Alternatives: a faster device (higher corners), or a feedback capacitor
    // across 10 M (lower corner). The Zener source itself is taken as white.
    double gainBandwidthHz = 3.0e6;
};

inline double highPassHz(double farads, double ohms) noexcept
{ return 1.0 / (2.0 * std::numbers::pi * farads * ohms); }

// Single-dominant-pole op amp: closed-loop corner = GBW / noise gain.
inline double stage1CornerHz(const PanelGenerator& g) noexcept
{ return g.gainBandwidthHz / (1.0 + g.stage1FeedbackOhms / g.stage1GroundOhms); }
inline double stage2CornerHz(const PanelGenerator& g) noexcept
{ return g.gainBandwidthHz / (1.0 + g.stage2FeedbackOhms / g.stage2InputOhms); }

// |H(f)|^2 of the generator chain, unit passband: two RC high-passes and two
// gain-bandwidth-limited stages. Gaussian, unclipped (declared; the chain's
// absolute level and any rail clipping are not documented).
inline double panelShape(double f, const PanelGenerator& g = {}) noexcept
{
    const auto hp = [f](double fc) { const double r = (f / fc) * (f / fc); return r / (1.0 + r); };
    const auto lp = [f](double fc) { return 1.0 / (1.0 + (f / fc) * (f / fc)); };
    return hp(highPassHz(g.couplingFarads, g.inputOhms)) * lp(stage1CornerHz(g))
         * hp(highPassHz(g.interstageFarads, g.stage2InputOhms)) * lp(stage2CornerHz(g));
}

// Background hiss (the ANALOG NOISE overlay): mix-bus/output op-amp noise,
// white with a flicker corner, declared as two first-order low shelves
// (f^2 + z^2) / (f^2 + p^2). It approximates the usual op-amp model
// 1 + 100 Hz / f within 0.64 dB over 20 Hz .. 20 kHz. INFERRED: 100 Hz is a
// typical bipolar (4558-class) 1/f corner; alternatives 50 or 200 Hz.
struct HissShape
{
    double zero1Hz = 15.0, pole1Hz = 5.0;
    double zero2Hz = 160.0, pole2Hz = 80.0;
};
inline double hissShape(double f, const HissShape& h = {}) noexcept
{
    const auto shelf = [f](double z, double p) { return (f * f + z * z) / (f * f + p * p); };
    return shelf(h.zero1Hz, h.pole1Hz) * shelf(h.zero2Hz, h.pole2Hz);
}

// Level reference: both sources keep the audio-band power (20 Hz .. 20 kHz)
// the previous circuit engine had at a 48 kHz host (2x): uniform white noise of
// variance 1/3 spread over 0 .. 48 kHz. A software calibration, not volts.
inline constexpr double kBandLowHz = 20.0, kBandHighHz = 20000.0;
inline constexpr double kReferenceBandPower = (1.0 / 3.0) * (kBandHighHz - kBandLowHz) / 48000.0;

template <class Shape>
double bandIntegral(Shape&& shape, double lo = kBandLowHz, double hi = kBandHighHz)
{
    // Log-spaced Simpson rule; the shapes are smooth rational functions.
    constexpr int n = 4000;
    const double a = std::log(lo), b = std::log(hi), h = (b - a) / n;
    double sum = 0.0;
    for (int i = 0; i <= n; ++i)
    {
        const double f = std::exp(a + i * h);
        const double w = i == 0 || i == n ? 1.0 : (i % 2 ? 4.0 : 2.0);
        sum += w * shape(f) * f; // df = f dln f
    }
    return sum * h / 3.0;
}

// One-sided PSDs (units^2 / Hz) normalized to the reference band power.
struct Spectrum
{
    PanelGenerator panel {};
    HissShape hiss {};
    double panelScale = kReferenceBandPower / bandIntegral([this](double f) { return panelShape(f, panel); });
    double hissScale = kReferenceBandPower / bandIntegral([this](double f) { return hissShape(f, hiss); });
    double panelPsd(double f) const noexcept { return panelScale * panelShape(f, panel); }
    double hissPsd(double f) const noexcept { return hissScale * hissShape(f, hiss); }
};
} // namespace cs80::reference::noise
