#pragma once
// OFFLINE conditional small-signal reference, never production audio.
// See docs/OE2_FILTER_REFERENCE_DESIGN.md for physical boundaries/exclusions.
#include <cmath>
#include <complex>
#include <limits>
#include <numbers>
#include <stdexcept>

namespace cs80::reference::oe2_filters {
using Complex = std::complex<double>;
inline constexpr double unknown = std::numeric_limits<double>::quiet_NaN();
enum class Branch { unknown, upper, lower };
struct InputBoundary {
    double sourceOhms = unknown;
    Complex outputLoadSiemens {unknown, unknown};
};
struct ReconstructionBoundary {
    double sourceOhms = unknown; // Already-combined tap Thevenin fixture, NOT packet units.
    Complex followerLoadSiemens {unknown, unknown}; // Extra load, excluding known Ci/Ri.
    Complex outputLoadSiemens {unknown, unknown}; // Before VCA coupling capacitors.
    Branch branch = Branch::unknown;
};
struct InputResponse {
    Complex u, a, b, y;
    Complex sourceCurrent; // From source through Rs/R1, toward U/A.
    Complex outputLoadCurrent; // From Y into the explicitly supplied load.
    Complex followerCurrent; // Supplied by Y into Cf and the output load.
};
struct ReconstructionResponse {
    Complex u, a, b, y, z, v;
    Complex sourceCurrent; // From source through Cc toward U.
    Complex shuntCurrent; // From U through 100k toward AC reference.
    Complex followerLoadCurrent; // From Y into the supplied EXTRA load.
    Complex outputLoadCurrent; // From V into the supplied final load.
    Complex followerCurrent; // Supplied by Y into Cf, Ci/Ri and extra load.
    Complex amplifierCurrent; // Supplied by V into Rf and final load.
};
inline bool finite(Complex z) {
    return std::isfinite(z.real()) && std::isfinite(z.imag());
}
inline void requireFinite(Complex z) {
    if (!finite(z)) throw std::overflow_error("OE2 filters: unrepresentable derived value");
}
inline void validateLoad(Complex y) {
    if (!finite(y) || y.real() < 0)
        throw std::invalid_argument("OE2 filters: explicit passive finite load required");
}
inline Complex validateSource(double ohms, double frequencyHz, Complex volts) {
    if (!std::isfinite(ohms) || ohms < 0 || !std::isfinite(frequencyHz)
        || frequencyHz < 0 || !finite(volts))
        throw std::invalid_argument("OE2 filters: invalid/unspecified source or frequency");
    const Complex s(0, 2 * std::numbers::pi * frequencyHz);
    requireFinite(s);
    return s;
}
inline void requireDenominator(Complex d) {
    requireFinite(d);
    if (d == Complex{}) throw std::overflow_error("OE2 filters: zero derived denominator");
}
// Nominal documented components, in ohms and farads.
inline constexpr double r1 = 10000;
inline constexpr double r2 = 10000;
inline constexpr double cf = 1.8e-9;
inline constexpr double cg = 680e-12;
inline constexpr double coupling = 1e-6, shuntOhms = 100000;
inline constexpr double interstageCap = 0.1e-6, inputOhms = 10000;

inline InputResponse solveInput(const InputBoundary& boundary,
                                double frequencyHz, Complex sourceVolts) {
    const Complex s = validateSource(boundary.sourceOhms, frequencyHz, sourceVolts);
    validateLoad(boundary.outputLoadSiemens);
    InputResponse out {};
    if (frequencyHz == 0) {
        out.u = out.a = out.b = out.y = sourceVolts;
        out.outputLoadCurrent = boundary.outputLoadSiemens * out.y;
        out.followerCurrent = out.outputLoadCurrent;
        requireFinite(out.followerCurrent);
        return out;
    }
    const double effectiveR1 = r1 + boundary.sourceOhms;
    if (!std::isfinite(effectiveR1))
        throw std::overflow_error("OE2 filters: unrepresentable source resistance");
    const Complex t = 1.0 + s * (r2 * cg);
    const Complex denominator = 1.0 + s * (cg * (effectiveR1 + r2))
        + (s * (effectiveR1 * cf)) * (s * (r2 * cg));
    requireFinite(t);
    requireDenominator(denominator);
    out.b = out.y = sourceVolts / denominator;
    out.a = t * out.b;
    // Capacitor currents avoid cancellation in (Vs-A)/(Rs+R1) near DC.
    out.sourceCurrent = s * cg * out.b + s * cf * (out.a - out.y);
    // Reconstruct from local drops, avoiding cancellation at very large Rs.
    out.u = out.a + r1 * out.sourceCurrent;
    out.outputLoadCurrent = boundary.outputLoadSiemens * out.y;
    // Y and B are equal in voltage, but Cf is driven by Y, not input B.
    out.followerCurrent = s * cf * (out.y - out.a) + out.outputLoadCurrent;
    for (Complex z : {out.u,out.a,out.b,out.y,out.sourceCurrent,
                      out.outputLoadCurrent,out.followerCurrent}) requireFinite(z);
    return out;
}

inline ReconstructionResponse solveReconstruction(const ReconstructionBoundary& boundary,
                                                   double frequencyHz, Complex sourceVolts) {
    const Complex s = validateSource(boundary.sourceOhms, frequencyHz, sourceVolts);
    validateLoad(boundary.followerLoadSiemens);
    validateLoad(boundary.outputLoadSiemens);
    double feedbackOhms;
    switch (boundary.branch) {
        case Branch::upper:
            feedbackOhms = 39000;
            break;
        case Branch::lower:
            feedbackOhms = 100000;
            break;
        default:
            throw std::invalid_argument("OE2 filters: explicit reconstruction branch required");
    }
    if (frequencyHz == 0) return {}; // Both coupling capacitors are open.
    const Complex cc = s * coupling, ci = s * interstageCap;
    const Complex sourceDenominator = 1.0 + boundary.sourceOhms * cc;
    requireDenominator(sourceDenominator);
    const Complex ys = cc / sourceDenominator;
    const Complex t = 1.0 + s * (r2 * cg);
    const Complex d = 1.0 + s * (cg * (r1 + r2)) + (s * (r1 * cf)) * (s * (r2 * cg));
    // (d - t) / R1, expressed without subtraction of nearly equal numbers.
    const Complex inputCurrentPerB = s * cg * (1.0 + s * (r2 * cf));
    const Complex denominator = (ys + 1.0 / shuntOhms) * d + inputCurrentPerB;
    const Complex interstageDenominator = 1.0 + inputOhms * ci;
    for (Complex z : {cc, ci, ys, t, d, inputCurrentPerB}) requireFinite(z);
    requireDenominator(denominator);
    requireDenominator(interstageDenominator);
    ReconstructionResponse out {};
    out.b = out.y = (ys / denominator) * sourceVolts;
    out.a = t * out.b;
    out.u = d * out.b;
    out.z = (inputOhms * ci / interstageDenominator) * out.y;
    out.v = -(feedbackOhms / inputOhms) * out.z;
    out.shuntCurrent = out.u / shuntOhms;
    // KCL preserves small quadrature current when Vs and U nearly coincide.
    out.sourceCurrent = out.shuntCurrent + inputCurrentPerB * out.b;
    out.followerLoadCurrent = boundary.followerLoadSiemens * out.y;
    out.outputLoadCurrent = boundary.outputLoadSiemens * out.v;
    out.followerCurrent = s * cf * (out.y - out.a) + out.z / inputOhms + out.followerLoadCurrent;
    out.amplifierCurrent = out.v / feedbackOhms + out.outputLoadCurrent;
    for (Complex z : {out.u, out.a, out.b, out.y, out.z, out.v, out.sourceCurrent,
                      out.shuntCurrent, out.followerLoadCurrent, out.outputLoadCurrent,
                      out.followerCurrent, out.amplifierCurrent}) requireFinite(z);
    return out;
}

// ws4-circuits Step 3: one complete OE2 wet branch, from I(53) to the IC5 (upper) or IC6
// (lower) amplifier output, for a supplied MN3001 baseband transfer that already includes
// the output-pair hold (see NonidealChannel in Mn3001Packet.h). The input stage is driven
// by the OE1 IC1 buffer (stiff source, no extra load). recombinationOhms is the Thevenin
// resistance of the two equal 2.2 k OUT1/OUT2 resistors from stiff pins; that boundary is
// a DECLARED fixture, not a measured source impedance. The recombined node is the average
// of the two pins, i.e. 0.5 x the normalized packet sum; the caller owns that scale.
inline Complex branchTransfer(double frequencyHz, Branch branch, Complex bbdTransfer,
                              double recombinationOhms = 1100.0) {
    const InputBoundary input {0.0, {0.0, 0.0}};
    const ReconstructionBoundary reconstruction {recombinationOhms, {0.0, 0.0}, {0.0, 0.0}, branch};
    return solveInput(input, frequencyHz, 1.0).y * bbdTransfer
         * solveReconstruction(reconstruction, frequencyHz, 1.0).v;
}
} // namespace cs80::reference::oe2_filters
