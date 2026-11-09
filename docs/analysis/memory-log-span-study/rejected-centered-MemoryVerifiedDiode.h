#pragma once

// OFFLINE research certificate primitive, not an Engine/audio-thread path.
// Exact real operations on the supplied finite binary64 coefficients are the
// target model. This does not enclose uncertainty in how those coefficients
// were prepared. The caller must separately enclose that preparation or declare
// the stored rounded coefficients to be its model.
//
// No libm logarithm/exponential decides acceptance. A rational atanh expansion
// encloses log, and a monotonic Shockley+series relation encloses current.
// Arithmetic contract: IEEE binary64, round-to-nearest, gradual underflow, no
// fast-math/reassociation; each basic operation is stored through volatile.
// supportedArithmetic() rejects incompatible runtime rounding/denormal modes.

#include <algorithm>
#include <bit>
#include <cfenv>
#include <cmath>
#include <cstdint>
#include <limits>

#if defined(__FAST_MATH__) || (defined(__FINITE_MATH_ONLY__) && __FINITE_MATH_ONLY__)
#error "MemoryVerifiedDiode requires strict floating-point arithmetic"
#endif

namespace cs80::experimental::memory_sparse::verified
{
static_assert(sizeof(double) == sizeof(std::uint64_t)
              && std::numeric_limits<double>::is_iec559
              && std::numeric_limits<double>::radix == 2
              && std::numeric_limits<double>::digits == 53
              && std::numeric_limits<double>::max_exponent == 1024);

struct Interval { double lo = 0.0, hi = 0.0; };
inline bool valid(Interval x) noexcept
{
    return std::isfinite(x.lo) && std::isfinite(x.hi) && x.lo <= x.hi;
}
inline Interval invalid() noexcept
{
    return {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::quiet_NaN()};
}
inline Interval point(double x) noexcept { return std::isfinite(x) ? Interval{x, x} : invalid(); }
namespace detail
{
// Storing every operation separately prohibits contraction/retention of a wider
// intermediate across operations, even if these small functions are inlined.
inline double rnAdd(double a, double b) noexcept { volatile double x = a + b; return x; }
inline double rnSub(double a, double b) noexcept { volatile double x = a - b; return x; }
inline double rnMul(double a, double b) noexcept { volatile double x = a * b; return x; }
inline double rnDiv(double a, double b) noexcept { volatile double x = a / b; return x; }
inline double up(double x) noexcept
{
    if (!std::isfinite(x)) return x;
    if (x == 0.0) return std::bit_cast<double>(std::uint64_t{1});
    auto bits = std::bit_cast<std::uint64_t>(x);
    bits = x > 0.0 ? bits + 1 : bits - 1;
    return std::bit_cast<double>(bits);
}
inline double down(double x) noexcept
{
    if (!std::isfinite(x)) return x;
    if (x == 0.0) return -std::bit_cast<double>(std::uint64_t{1});
    auto bits = std::bit_cast<std::uint64_t>(x);
    bits = x > 0.0 ? bits - 1 : bits + 1;
    return std::bit_cast<double>(bits);
}
inline Interval checked(double lo, double hi) noexcept
{
    const Interval out{lo, hi}; return valid(out) ? out : invalid();
}
} // namespace detail

// These low-level helpers assume supportedArithmetic() remains true throughout
// the call. The public logarithm/diode entry points perform that check themselves.
inline Interval add(Interval a, Interval b) noexcept
{
    if (!valid(a) || !valid(b)) return invalid();
    if (a.lo == a.hi && b.lo == b.hi)
    {
        const double value = detail::rnAdd(a.lo,b.lo);
        return detail::checked(detail::down(value),detail::up(value));
    }
    return detail::checked(detail::down(detail::rnAdd(a.lo, b.lo)), detail::up(detail::rnAdd(a.hi, b.hi)));
}
inline Interval sub(Interval a, Interval b) noexcept
{
    if (!valid(a) || !valid(b)) return invalid();
    if (a.lo == a.hi && b.lo == b.hi)
    {
        const double value = detail::rnSub(a.lo,b.lo);
        return detail::checked(detail::down(value),detail::up(value));
    }
    return detail::checked(detail::down(detail::rnSub(a.lo, b.hi)), detail::up(detail::rnSub(a.hi, b.lo)));
}
inline Interval mul(Interval a, Interval b) noexcept
{
    if (!valid(a) || !valid(b)) return invalid();
    // Exact point operands duplicate endpoint products in the general case.
    // Evaluate each distinct RN operation once; keep the same outward ULPs.
    if (a.lo == a.hi && b.lo == b.hi)
    {
        const double value = detail::rnMul(a.lo,b.lo);
        return detail::checked(detail::down(value),detail::up(value));
    }
    if (a.lo == a.hi || b.lo == b.hi)
    {
        const double scalar = a.lo == a.hi ? a.lo : b.lo;
        const Interval range = a.lo == a.hi ? b : a;
        const double low = detail::rnMul(scalar,scalar < 0.0 ? range.hi : range.lo);
        const double high = detail::rnMul(scalar,scalar < 0.0 ? range.lo : range.hi);
        return detail::checked(detail::down(low),detail::up(high));
    }
    const double values[] {detail::rnMul(a.lo,b.lo), detail::rnMul(a.lo,b.hi),
                           detail::rnMul(a.hi,b.lo), detail::rnMul(a.hi,b.hi)};
    double lo = std::numeric_limits<double>::infinity(), hi = -lo;
    for (double x : values)
    {
        if (!std::isfinite(x)) return invalid();
        lo = std::min(lo, detail::down(x)); hi = std::max(hi, detail::up(x));
    }
    return detail::checked(lo,hi);
}
inline Interval div(Interval a, Interval b) noexcept
{
    if (!valid(a) || !valid(b) || (b.lo <= 0.0 && b.hi >= 0.0)) return invalid();
    if (a.lo == a.hi && b.lo == b.hi)
    {
        const double value = detail::rnDiv(a.lo,b.lo);
        return detail::checked(detail::down(value),detail::up(value));
    }
    if (b.lo == b.hi)
    {
        const double low = detail::rnDiv(b.lo < 0.0 ? a.hi : a.lo,b.lo);
        const double high = detail::rnDiv(b.lo < 0.0 ? a.lo : a.hi,b.lo);
        return detail::checked(detail::down(low),detail::up(high));
    }
    if (a.lo == a.hi)
    {
        // d(a/x)/dx has sign -a for either permitted denominator sign.
        const double low = detail::rnDiv(a.lo,a.lo < 0.0 ? b.lo : b.hi);
        const double high = detail::rnDiv(a.lo,a.lo < 0.0 ? b.hi : b.lo);
        return detail::checked(detail::down(low),detail::up(high));
    }
    const double values[] {detail::rnDiv(a.lo,b.lo), detail::rnDiv(a.lo,b.hi),
                           detail::rnDiv(a.hi,b.lo), detail::rnDiv(a.hi,b.hi)};
    double lo = std::numeric_limits<double>::infinity(), hi = -lo;
    for (double x : values)
    {
        if (!std::isfinite(x)) return invalid();
        lo = std::min(lo, detail::down(x)); hi = std::max(hi, detail::up(x));
    }
    return detail::checked(lo,hi);
}
inline bool supportedArithmetic() noexcept
{
    if (std::fegetround() != FE_TONEAREST) return false;
    // Volatile operands make these runtime arithmetic checks, not constants.
    // Check arithmetic rounding too: fegetround need not expose a separately
    // modified SIMD control word on every supported compiler/platform.
    volatile double one = 1.0, halfUlp = 0x1p-53, threeQuarterUlp = 0x1.8p-53;
    volatile double tie = one + halfUlp, aboveHalf = one + threeQuarterUlp;
    if (std::bit_cast<std::uint64_t>(static_cast<double>(tie)) != 0x3ff0000000000000ULL
        || std::bit_cast<std::uint64_t>(static_cast<double>(aboveHalf)) != 0x3ff0000000000001ULL) return false;
    // The first denormal check catches DAZ; the inexact second catches FTZ.
    // Like ordinary floating arithmetic, these assume masked FP traps. They
    // may set exception flags; no rounding/denormal mode is changed here.
    volatile double tiny = std::bit_cast<double>(std::uint64_t{1});
    volatile double two = 2.0;
    volatile double twice = tiny * two;
    volatile double normal = std::numeric_limits<double>::min();
    volatile double three = 3.0;
    volatile double third = normal / three;
    return std::bit_cast<std::uint64_t>(static_cast<double>(twice)) == 2
        && std::bit_cast<std::uint64_t>(static_cast<double>(third)) == 0x0005555555555555ULL;
}

namespace detail
{
// These adjacent binary64 endpoints are NOT inferred from std::log(2).
// The accompanying exact-Fraction proof checks lower <= S_32 and
// S_32 + 2*(1/3)^65/(65*(1-1/9)) <= upper, where
// S_32 = 2*sum[j=0..31] (1/3)^(2j+1)/(2j+1).
inline constexpr Interval logTwo {0x1.62e42fefa39efp-1, 0x1.62e42fefa39f0p-1};

inline Interval logMantissa(double m) noexcept
{
    if (m == 1.0) return {0.0,0.0};
    if (!(m >= 0.75 && m <= 1.5)) return invalid();
    auto z = div(sub(point(m),point(1.0)),add(point(m),point(1.0)));
    if (!valid(z)) return invalid();
    auto z2 = mul(z,z);
    if (!valid(z2)) return invalid();
    z2.lo = std::max(z2.lo,0.0);
    if (!(z2.hi < 1.0)) return invalid();
    constexpr int terms = 12;
    Interval power = z, sum{0.0,0.0};
    for (int j=0; j<terms; ++j)
    {
        sum = add(sum,div(power,point(2*j+1.0)));
        power = mul(power,z2);
        if (!valid(sum) || !valid(power)) return invalid();
    }
    // Signed atanh series: the remaining tail has the sign of z and absolute
    // value <= 2*abs(z)^(2N+1)/((2N+1)*(1-z^2)). The interval power includes the
    // exact signed numerator even when its interval straddles zero.
    const auto partial = mul(point(2.0),sum);
    const double absolutePower = std::max(std::abs(power.lo),std::abs(power.hi));
    const auto tail = div(mul(point(2.0),point(absolutePower)),mul(point(2*terms+1.0),sub(point(1.0),z2)));
    if (!valid(partial) || !valid(tail) || tail.hi < 0.0) return invalid();
    const Interval remainder = z.lo >= 0.0 ? Interval{0.0,tail.hi}
                             : z.hi <= 0.0 ? Interval{-tail.hi,0.0} : Interval{-tail.hi,tail.hi};
    return add(partial,remainder);
}
inline Interval logPositive(double x) noexcept
{
    if (!std::isfinite(x) || !(x > 0.0)) return invalid();
    if (x == 1.0) return {0.0,0.0};
    // Exact binary decomposition x = 2^k*m, 1 <= m < 2, including subnormals.
    constexpr std::uint64_t fractionMask = (std::uint64_t{1} << 52) - 1;
    const auto bits = std::bit_cast<std::uint64_t>(x);
    const auto exponent = static_cast<int>((bits >> 52) & 0x7ff);
    std::uint64_t fraction = bits & fractionMask;
    int k = 0;
    if (exponent != 0) k = exponent - 1023;
    else
    {
        const int highest = 63 - std::countl_zero(fraction);
        k = highest - 1074;
        fraction = (fraction << (52-highest)) & fractionMask;
    }
    double m = std::bit_cast<double>((std::uint64_t{1023} << 52) | fraction);
    // Exact power-of-two scaling keeps the reduced coordinate in [0.75,1.5],
    // giving abs((m-1)/(m+1)) <= 0.2. This halves the verified series work
    // without discarding its analytic tail or changing the target logarithm.
    if (m > 1.5) { m *= 0.5; ++k; }
    return add(mul(point(static_cast<double>(k)),logTwo),logMantissa(m));
}
inline Interval logInterval(Interval x) noexcept
{
    if (!valid(x) || !(x.lo > 0.0)) return invalid();
    if (x.lo == x.hi) return logPositive(x.lo);
    // For 0 < a <= c <= b, integrate 1/u on either side of c:
    // log(c)-log(a) <= (c-a)/a and log(b)-log(c) <= (b-c)/c.
    // An ordinary midpoint is only a proposal; require containment before
    // using it. All error bounds themselves use outward interval arithmetic.
    const auto span = div(sub(point(x.hi),point(x.lo)),point(x.lo));
    if (valid(span) && span.hi <= 0x1p-40)
    {
        const double middle = x.lo + (x.hi-x.lo)*0.5;
        if (std::isfinite(middle) && middle >= x.lo && middle <= x.hi)
        {
            const auto center = logPositive(middle);
            const auto left = div(sub(point(middle),point(x.lo)),point(x.lo));
            const auto right = div(sub(point(x.hi),point(middle)),point(middle));
            if (valid(center) && valid(left) && valid(right))
            {
                const auto low = x.lo == middle ? center : sub(center,{0.0,left.hi});
                const auto high = x.hi == middle ? center : add(center,{0.0,right.hi});
                if (valid(low) && valid(high)) return checked(low.lo,high.hi);
            }
        }
    }
    // Wider/unsafe spans retain the independent two-endpoint construction.
    const auto a = logPositive(x.lo), b = logPositive(x.hi);
    if (!valid(a) || !valid(b)) return invalid();
    return checked(a.lo,b.hi);
}
inline Interval relation(double current, Interval across, double resistance,
                         double saturation, double nvt) noexcept
{
    if (!std::isfinite(current) || !(current > -saturation)) return invalid();
    // Sum BEFORE dividing: 1 + current/Is can round to zero near reverse
    // saturation, destroying an otherwise resolvable positive log argument.
    const auto argument = div(add(point(saturation),point(current)),point(saturation));
    const auto logarithm = logInterval(argument);
    if (!valid(logarithm)) return invalid();
    return sub(add(mul(point(current),point(resistance)),mul(point(nvt),logarithm)),across);
}
inline double widthUpper(Interval x) noexcept { return up(rnSub(x.hi,x.lo)); }
} // namespace detail

// All-or-nothing output: failure leaves the caller's interval unchanged.
inline bool encloseLog(double x, Interval& output) noexcept
{
    if (!supportedArithmetic()) return false;
    const auto next = detail::logPositive(x);
    if (!valid(next)) return false;
    output = next; return true;
}

// The exact diode current solves
//   R*i + nVt*log((Is+i)/Is) - across = 0,   i > -Is.
// It is unique since g'(i)=R+nVt/(Is+i)>R>0. across can be an interval;
// the enclosure contains the solution for EVERY voltage in that interval.
// Ordinary libm/Newton may supply candidate, but it is only an untrusted hint.
// Finite analytic brackets, interval g evaluation and a proved local derivative
// lower bound (at least R) decide acceptance.
// At most 66 verified contractions are attempted. A failure is not convergence
// and leaves output unchanged. maximumWidth is a CURRENT width in amperes,
// not a voltage certificate or permission to publish a complete circuit state.
inline bool encloseDiode(Interval across, double resistance, double saturation,
                         double nvt, double candidate, Interval& output,
                         double maximumWidth = 1e-12) noexcept
{
    if (!supportedArithmetic() || !valid(across) || !std::isfinite(resistance) || resistance <= 0.0
        || !std::isfinite(saturation) || saturation <= 0.0 || !std::isfinite(nvt) || nvt <= 0.0
        || !std::isfinite(maximumWidth) || maximumWidth <= 0.0) return false;
    if (across.lo == 0.0 && across.hi == 0.0) { output = {0.0,0.0}; return true; }
    Interval bracket{across.lo >= 0.0 ? 0.0 : -saturation,
                     across.hi > 0.0 ? detail::up(detail::rnDiv(across.hi,resistance)) : 0.0};
    if (!valid(bracket)) return false;
    const auto accept = [&]() noexcept {
        if (detail::widthUpper(bracket) > maximumWidth) return false;
        output = bracket; return true;
    };
    if (accept()) return true;
    const auto contract = [&](double i) noexcept {
        if (!std::isfinite(i) || i < bracket.lo || i > bracket.hi || !(i > -saturation)) return false;
        const auto g = detail::relation(i,across,resistance,saturation,nvt);
        if (!valid(g)) return false;
        // The candidate and every admissible root lie in bracket. Since g'
        // decreases with i, R + nVt/(Is+bracket.hi) is a lower derivative bound
        // on their connecting segment. Outward evaluation gives its safe lower
        // endpoint. R remains a valid fallback for underflow/overflow cases.
        const auto derivative = add(point(resistance),
            div(point(nvt),add(point(saturation),point(bracket.hi))));
        const double slope = valid(derivative) ? std::max(resistance,derivative.lo) : resistance;
        const double below = detail::up(detail::rnDiv(std::max(g.hi,0.0),slope));
        const double above = detail::up(detail::rnDiv(std::max(-g.lo,0.0),slope));
        double lo = detail::down(detail::rnSub(i,below));
        double hi = detail::up(detail::rnAdd(i,above));
        if (g.hi <= 0.0) lo = std::max(lo,i);
        if (g.lo >= 0.0) hi = std::min(hi,i);
        const Interval next{std::max(bracket.lo,lo),std::min(bracket.hi,hi)};
        if (!std::isfinite(below) || !std::isfinite(above) || !valid(next)) return false;
        bracket = next; return true;
    };
    // Deep reverse bias is often narrower than one representable current ULP.
    // -Is itself is an analytic limiting bound; never evaluate log(0) there.
    if (across.hi < 0.0)
    {
        (void) contract(detail::up(-saturation));
        if (accept()) return true;
    }
    if (std::isfinite(candidate))
    {
        (void) contract(candidate);
        if (accept()) return true;
    }
    for (int iteration=0; iteration<64; ++iteration)
    {
        // Only a proposal. No correctness depends on midpoint rounding.
        const double middle = bracket.lo * 0.5 + bracket.hi * 0.5;
        if (!(middle > bracket.lo && middle < bracket.hi) || !contract(middle)) return false;
        if (accept()) return true;
    }
    return false;
}
} // namespace cs80::experimental::memory_sparse::verified
