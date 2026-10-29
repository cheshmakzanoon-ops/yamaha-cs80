#pragma once
// OFFLINE conditional passive AC reference. Not an installed component revision.
// See docs/OUTPUT_LEVEL_SOURCE_REVIEW.md; no production audio dependencies.
#include <cmath>
#include <complex>
#include <limits>
#include <numbers>
#include <stdexcept>

namespace cs80::reference::output_taps {
using Complex = std::complex<double>;
inline constexpr double unknown = std::numeric_limits<double>::quiet_NaN();
inline constexpr double couplingFarads = 10e-6;
enum class Mode { unknown, high, low };
enum class LoadKind { unknown, open, shorted, finite };
struct Load {
    LoadKind kind = LoadKind::unknown;
    Complex ohms {unknown, unknown}; // Used only for finite; zero uses shorted.
};
struct Inputs {
    Mode mode = Mode::unknown;
    double frequencyHz = unknown;
    Complex sourceVolts {unknown, unknown};
    Complex sourceOhms {unknown, unknown}; // Finite, nonnegative real part.
    double highSeriesOhms = unknown;
    double lowSeriesOhms = unknown;
    double lowShuntOhms = unknown;
    Load load;
};
struct Result {
    Complex capacitorInputVolts {}, capacitorVolts {}, commonVolts {};
    Complex highTapVolts {}, lowTapVolts {}, jackVolts {};
    // Source/capacitor current flows toward common node; branch currents toward taps.
    Complex sourceAmps {}, highSeriesAmps {}, lowSeriesAmps {};
    Complex shuntAmps {}, loadAmps {}; // Toward reference / external load.
};
inline bool finite(Complex z) { return std::isfinite(z.real()) && std::isfinite(z.imag()); }
inline Result solve(const Inputs& p) {
    if ((p.mode != Mode::high && p.mode != Mode::low)
        || (p.load.kind != LoadKind::open && p.load.kind != LoadKind::shorted
            && p.load.kind != LoadKind::finite)
        || !std::isfinite(p.frequencyHz) || p.frequencyHz < 0
        || !finite(p.sourceVolts) || !finite(p.sourceOhms) || p.sourceOhms.real() < 0)
        throw std::invalid_argument("Output taps: explicit valid mode/source/load required");
    for (double r : {p.highSeriesOhms, p.lowSeriesOhms, p.lowShuntOhms})
        if (!std::isfinite(r) || r <= 0)
            throw std::invalid_argument("Output taps: explicit positive branch resistance required");
    if (p.load.kind == LoadKind::finite && (!finite(p.load.ohms)
        || p.load.ohms.real() < 0 || p.load.ohms == Complex {}))
        throw std::invalid_argument("Output taps: finite nonzero passive load required");
    Result result;
    // Settled DC: capacitor open, all downstream nodes grounded through resistors.
    if (p.frequencyHz == 0) {
        result.capacitorInputVolts = result.capacitorVolts = p.sourceVolts;
        return result;
    }
    using Wide = std::complex<long double>;
    const auto wide = [](Complex z) { return Wide(z.real(), z.imag()); };
    const auto checked = [](Wide z) {
        constexpr long double limit = std::numeric_limits<double>::max();
        if (!std::isfinite(z.real()) || !std::isfinite(z.imag())
            || std::abs(z.real()) > limit || std::abs(z.imag()) > limit)
            throw std::overflow_error("Output taps: unrepresentable result");
        const Complex v(static_cast<double>(z.real()), static_cast<double>(z.imag()));
        if (!finite(v) || (z.real() != 0 && v.real() == 0)
            || (z.imag() != 0 && v.imag() == 0))
            throw std::overflow_error("Output taps: unrepresentable result");
        return v;
    };
    const auto arithmetic = [](Wide value, bool nonzeroExpected) {
        if (!std::isfinite(value.real()) || !std::isfinite(value.imag())
            || (nonzeroExpected && value == Wide {}))
            throw std::overflow_error("Output taps: unrepresentable intermediate");
        return value;
    };
    const auto divide = [&](Wide a, Wide b) {
        return arithmetic(a / b, a != Wide {});
    };
    const auto multiply = [&](Wide a, Wide b) {
        return arithmetic(a * b, a != Wide {} && b != Wide {});
    };
    const auto parallel = [&](Wide a, Wide b) {
        return multiply(divide(a, arithmetic(a+b, true)), b);
    };
    const long double rh = p.highSeriesOhms, ra = p.lowSeriesOhms, rb = p.lowShuntOhms;
    const Wide vs = wide(p.sourceVolts), zs = wide(p.sourceOhms);
    // Reciprocal first avoids overflowing 2*pi*f before multiplying by C on
    // platforms where long double has the same range as double.
    const Wide zc = arithmetic(Wide(0, -(1 / static_cast<long double>(p.frequencyHz))
        / (2 * std::numbers::pi_v<long double> * couplingFarads)), true);
    const bool high = p.mode == Mode::high;
    const bool open = p.load.kind == LoadKind::open;
    const bool shorted = p.load.kind == LoadKind::shorted;
    const Wide zl = p.load.kind == LoadKind::finite ? wide(p.load.ohms) : Wide {};
    const Wide q = high || open ? Wide(rb) : shorted ? Wide {} : parallel(Wide(rb), zl);
    const Wide lowBranch = ra + q;
    const Wide zp = high && !open ? parallel(lowBranch, rh + zl) : lowBranch;
    const Wide denominator = zs + zc + zp;
    const long double scale = std::abs(zs) + std::abs(zc) + std::abs(zp);
    // Conservative arithmetic policy, not a physical impedance or resonance limit.
    if (!std::isfinite(scale) || !(std::abs(denominator) > 0)
        || scale / std::abs(denominator) > 1e8L)
        throw std::overflow_error("Output taps: unresolved impedance cancellation");
    const Wide current = divide(vs, denominator);
    const Wide x = multiply(current, zp);
    const Wide lowCurrent = divide(x, lowBranch);
    const Wide lo = multiply(lowCurrent, q);
    const Wide hiCurrent = high && !open ? divide(x, rh + zl) : Wide {};
    const Wide hi = high && !open ? multiply(hiCurrent, zl) : x;
    const Wide jack = high ? hi : lo;
    const Wide loadCurrent = open ? Wide {} : shorted ? (high ? hiCurrent : lowCurrent)
                                                         : divide(jack, zl);
    result.capacitorInputVolts = checked(multiply(current, zc + zp));
    result.capacitorVolts = checked(multiply(current, zc));
    result.commonVolts = checked(x);
    result.highTapVolts = checked(hi);
    result.lowTapVolts = checked(lo);
    result.jackVolts = checked(jack);
    result.sourceAmps = checked(current);
    result.highSeriesAmps = checked(hiCurrent);
    result.lowSeriesAmps = checked(lowCurrent);
    result.shuntAmps = checked(divide(lo, rb));
    result.loadAmps = checked(loadCurrent);
    return result;
}
} // namespace cs80::reference::output_taps
