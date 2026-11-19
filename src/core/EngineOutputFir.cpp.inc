// Implementation included exactly once by Engine.cpp, not by public headers.
// One TU owns every mutation; its optimizer can inline the retained scalar
// path even on unsupported targets. Never vary EngineOutputFir's class layout.
#include "EngineOutputFir.h"
#include <bit>
#if defined(__x86_64__)
#include <xmmintrin.h>
#endif
#if defined(__linux__) && defined(__x86_64__) && defined(__GNUC__) && defined(__SSE2_MATH__) && !defined(__clang__) \
    && !defined(__INTEL_COMPILER) && !defined(__INTEL_LLVM_COMPILER) && !defined(__NVCOMPILER) \
    && !defined(__PGI) && !defined(__ibmxl__) && !defined(_MSC_VER) && !defined(__FMA__) \
    && !defined(__AVX__) && !defined(__FMA4__) && !defined(__FP_FAST_FMA) && !defined(__FP_FAST_FMAF) \
    && !defined(__NO_TRAPPING_MATH__) && !defined(__FAST_MATH__) && !defined(__ASSOCIATIVE_MATH__) && !defined(__RECIPROCAL_MATH__) \
    && !defined(__NO_SIGNED_ZEROS__) && (!defined(__FINITE_MATH_ONLY__) || !__FINITE_MATH_ONLY__) \
    && (!defined(__FLT_EVAL_METHOD__) || __FLT_EVAL_METHOD__ == 0) && !defined(CS80_DISALLOW_EXACT_ZERO_FIR)
#define CS80_ENGINE_TRACKS_EXACT_FIR_ZERO 1
#else
#define CS80_ENGINE_TRACKS_EXACT_FIR_ZERO 0
#endif

#if CS80_ENGINE_TRACKS_EXACT_FIR_ZERO && defined(__OPTIMIZE__) && !defined(__OPTIMIZE_SIZE__) \
    && !defined(__NO_MATH_ERRNO__) && !defined(__ROUNDING_MATH__) && !defined(__SUPPORT_SNAN__) \
    && ((__GNUC__ == 14 && __GNUC_MINOR__ == 2) || (__GNUC__ == 13 && __GNUC_MINOR__ == 3 && __GNUC_PATCHLEVEL__ == 0)) \
    && !defined(CS80_DISABLE_WIDENED_FIR)
#define CS80_ENGINE_WIDENS_FIR 1
#else
#define CS80_ENGINE_WIDENS_FIR 0
#endif
#ifdef CS80_FIR_ZERO_REACH_ONLY
extern "C" void cs80_observe_fir_reach(bool) noexcept;
#endif
namespace cs80
{
void EngineOutputFir::prepare(int factor) noexcept
{
    scalar.prepare(factor);
#if CS80_ENGINE_TRACKS_EXACT_FIR_ZERO
    uniformZeroRun = scalar.getTaps();
#if CS80_ENGINE_WIDENS_FIR
    samplesToWidened = 0;
#endif
    safeZeroCoefficients = true;
    hasNegativeWitness = false;
    const int center = scalar.getCenter();
    for (int i = 0; i <= center; ++i)
    {
        // The coefficient design writes each pair from the same stored float.
        // Examine its actual IEEE bits, including after directed-rounding setup.
        const auto bits = std::bit_cast<std::uint32_t>(scalar.getCoefficient(i));
        const auto mag = bits & 0x7fffffffu;
        if ((mag != 0 && mag < 0x00800000u) || mag >= 0x7f800000u)
            safeZeroCoefficients = false;
        if ((bits >> 31) != 0)
        {
            if (i == center) safeZeroCoefficients = false;
            else hasNegativeWitness = true;
        }
    }
#if CS80_ENGINE_WIDENS_FIR
    // A signed zero or finite normal binary32 coefficient widens exactly.
    // The two private shadow mirrors must fit entirely outside the raw ring.
    static_assert(sizeof(double) == 2 * sizeof(float) && std::numeric_limits<double>::is_iec559
                  && std::numeric_limits<double>::digits == 53);
    static_assert(3 * OversamplingDecimator::tapsFor(2) <= OversamplingDecimator::kMaxTaps);
    if (safeZeroCoefficients && scalar.hasWideningScratch())
        scalar.cacheWidenedCoefficients();
#endif
#endif
}

void EngineOutputFir::reset() noexcept
{
    scalar.reset();
#if CS80_ENGINE_TRACKS_EXACT_FIR_ZERO
    uniformZeroRun = scalar.getTaps();
#if CS80_ENGINE_WIDENS_FIR
    samplesToWidened = 0;
#endif
#endif
}

void EngineOutputFir::push(float x) noexcept
{
#if !CS80_ENGINE_WIDENS_FIR
    scalar.push(x); // Unsupported translation units retain the original ordering.
#endif
#if CS80_ENGINE_TRACKS_EXACT_FIR_ZERO
    const auto bits = std::bit_cast<std::uint32_t>(x);
    const int taps = scalar.getTaps();
#if CS80_ENGINE_WIDENS_FIR
    // An already uniform raw window and its widened copy are unchanged by
    // another zero of that exact sign. Only the common ring cursor moves.
    if ((bits == 0u && uniformZeroRun == taps) || (bits == 0x80000000u && uniformZeroRun == -taps))
    {
        scalar.advanceUnchangedHistory();
        return;
    }
    if (scalar.getFactor() == 2)
    {
        const auto mag = bits & 0x7fffffffu;
        const bool zero = mag == 0u;
        // Normal or signed zero widens exactly with all rounding and DAZ/FTZ
        // modes. Signalling NaNs and subnormals never move exception timing.
        if (zero || (mag - 0x00800000u < 0x7f800000u - 0x00800000u))
        {
            scalar.rememberWidenedPush(x);
            if (samplesToWidened != 0) --samplesToWidened;
        }
        else samplesToWidened = static_cast<std::uint16_t>(taps);
    }
#endif
    if ((bits & 0x7fffffffu) != 0u) uniformZeroRun = 0;
    else if (bits == 0u)
    {
        if (uniformZeroRun != taps) uniformZeroRun = std::max(0, uniformZeroRun) + 1;
    }
    else if (uniformZeroRun != -taps)
        uniformZeroRun = std::min(0, uniformZeroRun) - 1;
#endif
#if CS80_ENGINE_WIDENS_FIR
    scalar.push(x);
#endif
}

bool EngineOutputFir::qualifies() const noexcept
{
#if CS80_ENGINE_TRACKS_EXACT_FIR_ZERO
    static_assert(std::numeric_limits<float>::is_iec559 && sizeof(float) == sizeof(std::uint32_t));
    const int taps = scalar.getTaps();
    // Uniform +0 starts with a +0 center term. Uniform -0 starts with -0;
    // a demonstrated negative coefficient contributes +0. With nearest sums
    // and normal/zero coefficients the final result is exactly +0; no flags.
    // Otherwise preserve the complete scalar arithmetic, including signed zero.
    return (uniformZeroRun == taps || (uniformZeroRun == -taps && hasNegativeWitness))
        && safeZeroCoefficients && (_mm_getcsr() & 0x7f80u) == 0x1f80u;
#else
    return false;
#endif
}

float EngineOutputFir::output() const noexcept
{
#if CS80_ENGINE_TRACKS_EXACT_FIR_ZERO || defined(CS80_FIR_ZERO_REACH_ONLY)
    const bool fast = qualifies();
#ifdef CS80_FIR_ZERO_REACH_ONLY
    cs80_observe_fir_reach(fast);
#endif
    if (fast) return 0.0f;
#endif
#if CS80_ENGINE_WIDENS_FIR
    // Nonstandard or trapping FP retains the literal original arithmetic.
    // 8x has no scratch tail; it always uses the ordinary coefficient source.
    if (scalar.hasWideningScratch() && safeZeroCoefficients && (_mm_getcsr() & 0x7f80u) == 0x1f80u)
    {
        if (scalar.getFactor() == 2 && samplesToWidened == 0)
            return scalar.outputWithWidenedHistory();
        return scalar.outputWithWidenedCoefficients();
    }
#endif
    return scalar.output();
}
} // namespace cs80
#undef CS80_ENGINE_WIDENS_FIR
#undef CS80_ENGINE_TRACKS_EXACT_FIR_ZERO
