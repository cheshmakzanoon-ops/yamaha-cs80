#pragma once
// OFFLINE truth model for the original SUB oscillator (IG00150), never production audio.
// Double precision, straight from the documented node equation; see
// docs/workstreams/ws4-circuits.md (Step 1) for the evidence class of every constant.
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace cs80::reference::ig00150
{
// Service SUB29: VC3 (panel SPEED wiper) and VC4 (TRG5 pressure bus through the AT SPEED
// wiper) each reach IC5 pin 1 through 120 kOhm. Source-derived.
inline constexpr double kSummingOhms = 120000.0;

// Anchors. minHz/maxBaseHz: owner's manual printed p49 (SPEED 0.7-60 Hz), source-derived.
// maxCombinedHz: service p52 lists a 200 Hz IG00150 calibration point. Taking it as the
// frequency at full SPEED plus full pressure at full AT SPEED is a DECLARED INFERENCE.
// speedSpanVolts: the SPEED wiper is taken to span 0..10 V like the other 0-10 V panel
// controls (IG00152 service p42); DECLARED INFERENCE. Only ratios of currents matter
// below, so this choice does not change any frequency.
struct Anchors
{
    double minHz = 0.7;
    double maxBaseHz = 60.0;
    double maxCombinedHz = 200.0;
    double speedSpanVolts = 10.0;
};

inline void validate(const Anchors& a)
{
    if (!(a.minHz > 0) || !(a.maxBaseHz > a.minHz) || !(a.maxCombinedHz >= a.maxBaseHz)
        || !(a.speedSpanVolts > 0) || !std::isfinite(a.maxCombinedHz))
        throw std::invalid_argument("IG00150 reference: invalid anchors");
}

// Pin-1 summing-node current from both wipers (ideal virtual-ground input).
inline double nodeCurrent(double speedVolts, double touchVolts)
{
    return speedVolts / kSummingOhms + touchVolts / kSummingOhms;
}

// Exponential converter (IC guide PDF29, printed 2-1): f = minHz * exp(I / Ie), with Ie
// fixed by the documented SPEED span: I = 0 -> minHz, I = span/120k -> maxBaseHz.
inline double currentPerNeper(const Anchors& a)
{
    validate(a);
    return (a.speedSpanVolts / kSummingOhms) / std::log(a.maxBaseHz / a.minHz);
}
inline double frequency(double currentAmps, const Anchors& a)
{
    return a.minHz * std::exp(currentAmps / currentPerNeper(a));
}

// SPEED wiper voltage that produces a panel speed in Hz with no touch (exact inverse).
inline double speedVolts(double panelHz, const Anchors& a)
{
    if (!(panelHz >= a.minHz && panelHz <= a.maxBaseHz))
        throw std::invalid_argument("IG00150 reference: panel speed outside 0.7-60 Hz");
    return kSummingOhms * currentPerNeper(a) * std::log(panelHz / a.minHz);
}

// Touch voltage at full pressure and full AT SPEED that reaches maxCombinedHz from full
// SPEED: solved from the node equation, not assumed.
inline double fullTouchVolts(const Anchors& a)
{
    const double needed = currentPerNeper(a) * std::log(a.maxCombinedHz / a.minHz);
    return (needed - a.speedSpanVolts / kSummingOhms) * kSummingOhms;
}

// Complete speed law: panel SPEED in Hz, AT SPEED wiper fraction and normalized TRG5
// pressure (both 0..1, linear wipers: DECLARED INFERENCE).
inline double speedHz(double panelHz, double atSpeedDepth, double pressure, const Anchors& a = {})
{
    if (!(atSpeedDepth >= 0 && atSpeedDepth <= 1 && pressure >= 0 && pressure <= 1))
        throw std::invalid_argument("IG00150 reference: depth/pressure outside 0..1");
    const double touch = fullTouchVolts(a) * atSpeedDepth * pressure;
    return frequency(nodeCurrent(speedVolts(panelHz, a), touch), a);
}

// Ideal waveform shapes in the physical SW1 order (SUB30), with the nominal amplitude
// ratios the realtime model uses: sine peak 1, saws peak 3.5/3.2, square peak 1.
inline constexpr double kSawAmplitude = 3.5 / 3.2;

// Fourier magnitude of harmonic h (h >= 1) of each ideal waveform.
inline double harmonicAmplitude(int function, int h)
{
    if (h < 1) throw std::invalid_argument("IG00150 reference: harmonic index");
    switch (function)
    {
        case 0: return h == 1 ? 1.0 : 0.0;
        case 1:
        case 2: return kSawAmplitude * 2.0 / (std::numbers::pi * h);
        case 3: return h % 2 ? 4.0 / (std::numbers::pi * h) : 0.0;
        default: throw std::invalid_argument("IG00150 reference: function without a line spectrum");
    }
}

// Band-limited ideal waveform at phase in [0,1): additive synthesis of every harmonic
// below the supplied limit. Signs follow 1-2p (falling), 2p-1 (rising), +1 then -1 (square).
inline double bandLimited(int function, double phase, double f0, double limitHz)
{
    const double w = 2.0 * std::numbers::pi * phase;
    double sum = 0.0;
    for (int h = 1; h * f0 < limitHz; ++h)
    {
        const double a = harmonicAmplitude(function, h);
        if (a == 0.0) continue;
        const double s = std::sin(h * w);
        sum += function == 2 ? -a * s : a * s;
    }
    return sum;
}
} // namespace cs80::reference::ig00150
