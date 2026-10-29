#pragma once
// OFFLINE, conditional small-signal AC reference ONLY. Not WO or pedal transfer.
// See docs/PRA_WAH_REFERENCE_DESIGN.md. No production audio dependencies.
#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <numbers>
#include <limits>
#include <stdexcept>

namespace cs80::reference::pra_wah
{
using Complex = std::complex<double>;
inline constexpr double unknown = std::numeric_limits<double>::quiet_NaN();
struct Boundary
{
    double opticalOhms = unknown;
    double unresolvedParallelOhms = unknown; // Mandatory: PRA53 unlabelled; no nominal default.
    double vr1Ohms = unknown; // Effective rheostat resistance, NOT knob/pedal position.
    double vr2Ohms = unknown;
    double sourceOhms = unknown; // Explicit Thevenin fixture, not installed source impedance.
    Complex dryLoadSiemens {unknown, unknown}; // Omitted FET1 branch at X, supplied for this frequency.
    Complex outputLoadSiemens {unknown, unknown}; // Explicit ideal-output load: current accounting only.
};
struct Response
{
    Complex x, n, a, p, b, q, c, d, y; // Y is IC2 pin13 BEFORE FET2, never WO.
    Complex sourceCurrent, dryLoadCurrent, outputLoadCurrent;
    // Current supplied by final ideal op amp into feedback AND external load.
    Complex outputCurrent;
};
inline bool finite(Complex v)
{
    return std::isfinite(v.real()) && std::isfinite(v.imag());
}
inline void validate(const Boundary& b, double frequencyHz, Complex sourceVolts)
{
    for (double v : {b.opticalOhms, b.unresolvedParallelOhms})
        if (!std::isfinite(v) || v <= 0)
            throw std::invalid_argument("PRA wah: explicit finite positive resistance required");
    for (double v : {b.vr1Ohms, b.vr2Ohms, b.sourceOhms, frequencyHz})
        if (!std::isfinite(v) || v < 0)
            throw std::invalid_argument("PRA wah: finite nonnegative boundary required");
    if (b.vr1Ohms > 25000 || b.vr2Ohms > 100000 || !finite(sourceVolts))
        throw std::invalid_argument("PRA wah: trim range or source voltage invalid");
    for (Complex y : {b.dryLoadSiemens, b.outputLoadSiemens})
        if (!finite(y) || y.real() < 0)
            throw std::invalid_argument("PRA wah: finite passive load admittance required");
}
inline Response solve(const Boundary& boundary, double frequencyHz, Complex sourceVolts)
{
    validate(boundary, frequencyHz, sourceVolts);
    const double low = std::min(boundary.opticalOhms, boundary.unresolvedParallelOhms);
    const double high = std::max(boundary.opticalOhms, boundary.unresolvedParallelOhms);
    const double rt = boundary.vr1Ohms + low / (1 + low / high);
    const double tau1 = rt * 2.2e-9;
    const Complex s(0, 2 * std::numbers::pi * frequencyHz);
    const Complex u1 = s * tau1, u2 = s * (10000 * 33e-9);
    const Complex cin = s * 10e-6, cout = s * 1e-6;
    if (!(tau1 > 0) || !std::isfinite(tau1) || !finite(s) || !finite(u1)
        || !finite(u2) || !finite(cin) || !finite(cout)
        || !finite(boundary.sourceOhms * cin))
        throw std::overflow_error("PRA wah: unrepresentable derived coefficient");
    // DC coupling capacitors open: every audio node and current is exactly zero.
    if (frequencyHz == 0) return {};
    const Complex f = ((1.0-u1)/(1.0+u1))*((1.0-u2)/(1.0+u2));
    const Complex nx = 1.0 / (2.0 + (10000.0/470.0)*(1.0+f));
    const Complex ys = cin / (1.0 + boundary.sourceOhms * cin);
    const Complex denominator = ys + 1e-4 + (1.0-nx)*1e-4 + boundary.dryLoadSiemens;
    if (!finite(denominator) || std::abs(denominator) == 0)
        throw std::overflow_error("PRA wah: invalid derived input admittance");
    Response r {};
    r.x = sourceVolts * (ys / denominator);
    r.n = nx*r.x;
    r.a = -r.n;
    r.p = r.a/(1.0+u1);
    r.b = 2.0*r.p-r.a;
    r.q = r.b/(1.0+u2);
    r.c = 2.0*r.q-r.b;
    r.d = r.c*(cout/(cout+2e-4));
    r.y = -(boundary.vr2Ohms/10000.0)*r.d; // Includes exact VR2=0 limit.
    r.sourceCurrent = ys*(sourceVolts-r.x);
    r.dryLoadCurrent = boundary.dryLoadSiemens*r.x;
    r.outputLoadCurrent = boundary.outputLoadSiemens*r.y;
    // Feedback current Y→virtual ground is −D/10k, including zero-ohm limit.
    r.outputCurrent = r.outputLoadCurrent-r.d/10000.0;
    for (Complex v : {r.x,r.n,r.a,r.p,r.b,r.q,r.c,r.d,r.y,r.sourceCurrent,
                       r.dryLoadCurrent,r.outputLoadCurrent,r.outputCurrent})
        if (!finite(v)) throw std::overflow_error("PRA wah: nonfinite derived result");
    return r;
}
} // namespace cs80::reference::pra_wah
