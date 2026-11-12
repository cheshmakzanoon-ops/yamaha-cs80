#pragma once
// OFFLINE truth model for the PRA ring modulator, never production audio. Double
// precision; the converter is solved from the two adjustment-procedure anchors and the
// multiplier spectrum is derived analytically from the offset-multiplier equation.
// Evidence classes: docs/workstreams/ws4-circuits.md, Step 2.
#include <cmath>
#include <stdexcept>

namespace cs80::reference::ring
{
// Adjustment procedure printed p19: period at VC1 = +5 V and at maximum SPEED, depth OFF.
// The +10 V maximum and the +5 V envelope peak are the declared inferences of Step 2.
struct Anchors
{
    double anchorVolts = 5.0, anchorPeriod = 0.150;
    double maxVolts = 10.0, maxPeriod = 0.005;
    double envelopePeakVolts = 5.0;
};

// Exponential converter through both anchors: log f is affine in the control voltage.
// The linear alternative f = k V through the origin cannot meet both anchors (it would need
// a 150 V maximum), and an affine linear law crosses zero at a positive voltage.
inline double hzForVolts(double volts, const Anchors& a = {})
{
    if (!(a.anchorPeriod > 0 && a.maxPeriod > 0 && a.maxVolts > a.anchorVolts))
        throw std::invalid_argument("ring reference: invalid anchors");
    const double f1 = 1.0 / a.anchorPeriod, f2 = 1.0 / a.maxPeriod;
    return f1 * std::pow(f2 / f1, (volts - a.anchorVolts) / (a.maxVolts - a.anchorVolts));
}
inline double voltsForHz(double hz, const Anchors& a = {})
{
    if (!(hz > 0)) throw std::invalid_argument("ring reference: frequency must be positive");
    const double f1 = 1.0 / a.anchorPeriod, f2 = 1.0 / a.maxPeriod;
    return a.anchorVolts + (a.maxVolts - a.anchorVolts) * std::log(hz / f1) / std::log(f2 / f1);
}
// SPEED value (Hz at depth OFF) plus DEPTH x envelope x EO peak at the converter node.
inline double carrierHz(double speedHz, double depth, double envelope, const Anchors& a = {})
{
    if (!(depth >= 0 && depth <= 1 && envelope >= 0 && envelope <= 1))
        throw std::invalid_argument("ring reference: depth/envelope outside 0..1");
    return hzForVolts(voltsForHz(speedHz, a) + depth * envelope * a.envelopePeakVolts, a);
}

// Offset multiplier y = (x + sx)(c + sc) + so, blended with the dry signal by amount m:
// out = (1 - m) x + m y. For x = A sin(wi t), c = sin(wc t), the output line amplitudes are:
struct Offsets
{
    double signal = 5.0e-4, carrier = 5.0e-3, output = 1.0e-3;
};
struct Lines
{
    double dc, carrier, signal, lowerSideband, upperSideband;
};
inline Lines lines(double inputAmplitude, double amount, const Offsets& o = {})
{
    Lines l {};
    l.dc = amount * (o.signal * o.carrier + o.output);
    l.carrier = amount * o.signal;
    l.signal = inputAmplitude * ((1.0 - amount) + amount * o.carrier);
    l.lowerSideband = l.upperSideband = amount * inputAmplitude * 0.5;
    return l;
}
} // namespace cs80::reference::ring
