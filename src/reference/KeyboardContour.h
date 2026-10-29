#pragma once
#include <array>
#include <cmath>
#include <stdexcept>

namespace cs80::reference::keyboard_contour
{
// Conditional, nominal settled-DC reference only. NOT used by Engine.
// No physical-key/raw-code-to-OC mapping is asserted here.
inline constexpr double referenceVolts = 225.0 / 56.0;
struct Inputs
{
    bool q1 = false, q2 = false, q3 = false; // physical CMOS states
    int selectedOc = 0; // externally supplied OC0..5; NOT inferred from Q
    int noteTap = 0;    // externally supplied C#=0 .. C=11
    double lowTravel = 0.0, highTravel = 0.0; // electrical travel, -1..1
    double vr1Ohms = 0.0, vr2Ohms = 0.0;     // supplied calibration, 0..5000
};
struct Result
{
    double lowWiperVolts, highWiperVolts;
    double lowReferenceVolts, highReferenceVolts;
    std::array<double, 6> octaveVolts;
    double ooVolts, sampledVolts, outputVolts;
};
inline void requireRange(double x, double lo, double hi)
{
    if (!std::isfinite(x) || x < lo || x > hi)
        throw std::invalid_argument("keyboard contour input outside physical range");
}
inline double loadedWiperVolts(double travel)
{
    requireRange(travel, -1.0, 1.0);
    // Ideal B10k with grounded center tap; each half is 5k.
    const double a = std::abs(travel);
    const double rth = 5000.0 * a * (1.0 - a);
    return 15.0 * travel * 56000.0 / (56000.0 + rth);
}
inline Result evaluate(const Inputs& in)
{
    if (in.selectedOc < 0 || in.selectedOc > 5 || in.noteTap < 0 || in.noteTap > 11)
        throw std::invalid_argument("keyboard contour OC/tap outside physical range");
    requireRange(in.vr1Ohms, 0.0, 5000.0);
    requireRange(in.vr2Ohms, 0.0, 5000.0);
    Result out {};
    out.lowWiperVolts = loadedWiperVolts(in.lowTravel);
    out.highWiperVolts = loadedWiperVolts(in.highTravel);
    const double r = referenceVolts;
    const double l = r - (15.0 / 56.0) * out.lowWiperVolts;
    const double h = r - (15.0 / 56.0) * out.highWiperVolts;
    out.lowReferenceVolts = l;
    out.highReferenceVolts = h;
    out.octaveVolts = {l, l, (2.0*l+r)/3.0, r, (r+2.0*h)/3.0, h};
    const double o = out.octaveVolts[static_cast<std::size_t>(in.selectedOc)];
    out.ooVolts = o;
    const double m = static_cast<double>(in.noteTap);
    // Q1Q2Q3 notation: Q1 is the first/MSB here, not a claimed binary octave.
    const unsigned state = (in.q1 ? 4u : 0u) | (in.q2 ? 2u : 0u) | (in.q3 ? 1u : 0u);
    double v = o;
    switch (state)
    {
        case 1: v = r + (m/11.0)*(o-r); break;
        case 2: v = o + (m/11.0)*(r-o); break;
        case 5: v = o - (11.0-m)*1000.0*(o-r)/(31000.0+in.vr2Ohms); break;
        case 6: v = o + m*1000.0*(r-o)/(33000.0+in.vr1Ohms); break;
        default: break; // one OO endpoint, other endpoint open
    }
    out.sampledVolts = v;
    out.outputVolts = r-v;
    return out;
}
} // namespace cs80::reference::keyboard_contour
