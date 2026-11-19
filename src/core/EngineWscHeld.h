#pragma once
#if defined(CS80_ENABLE_BORROWED_HELD) && CS80_ENABLE_BORROWED_HELD && defined(__OPTIMIZE__) && !defined(__OPTIMIZE_SIZE__) \
    && defined(__linux__) && defined(__x86_64__) && defined(__GNUC__) && defined(__SSE2_MATH__) \
    && !defined(__clang__) && !defined(__INTEL_COMPILER) && !defined(__INTEL_LLVM_COMPILER) \
    && !defined(__NVCOMPILER) && !defined(__PGI) && !defined(__ibmxl__) && !defined(_MSC_VER) \
    && !defined(__AVX__) && !defined(__FMA__) && !defined(__FMA4__) && !defined(__FP_FAST_FMA) \
    && !defined(__FP_FAST_FMAF) && !defined(__FAST_MATH__) && !defined(__ASSOCIATIVE_MATH__) \
    && !defined(__RECIPROCAL_MATH__) && !defined(__NO_SIGNED_ZEROS__) && !defined(__NO_TRAPPING_MATH__) \
    && !defined(__NO_MATH_ERRNO__) && !defined(__ROUNDING_MATH__) && !defined(__SUPPORT_SNAN__) \
    && (!defined(__FINITE_MATH_ONLY__) || __FINITE_MATH_ONLY__ == 0) \
    && defined(__FLT_EVAL_METHOD__) && __FLT_EVAL_METHOD__ == 0 && __GNUC__ == 14 && __GNUC_MINOR__ == 2 && !defined(CS80_DISABLE_BORROWED_HELD) && !defined(CS80_DISABLE_AVX2_FILTER_BATCH)
#define CS80_WSC_BORROW_HELD 1
#else
#define CS80_WSC_BORROW_HELD 0
#endif

#if CS80_WSC_BORROW_HELD
#include "ReconstructedOscillator.h"
#include <bit>
#include <xmmintrin.h>
namespace cs80 {
struct EngineWscHeldAccess {
    static bool canBorrow(const ReconstructedOscillator& o, double f, const WscControls& c) noexcept {
        const auto hz=std::bit_cast<std::uint64_t>(f), t=std::bit_cast<std::uint64_t>(c.pulseThresholdVolts), mag=t & std::uint64_t{0x7fffffffffffffff};
        if (!o.endNodesValid || !o.basis || hz!=std::bit_cast<std::uint64_t>(o.state.heldFrequencyHz)
            || t!=std::bit_cast<std::uint64_t>(o.state.heldThresholdVolts) || c.sawEnabled!=o.state.sawEnabled || c.pulseEnabled!=o.state.pulseEnabled
            || hz<0x0010000000000000ull || hz>0x40a0000000000000ull || (mag && (mag<0x0010000000000000ull || mag>=0x7ff0000000000000ull))) return false;
        const unsigned mx=_mm_getcsr(); unsigned short cw; asm volatile("fnstcw %0":"=m"(cw)::"memory");
        if ((mx&0x7f80u)!=0x1f80u || (cw&0x0f3fu)!=0x033fu) return false;
        // 8 kHz minimum => .45*rate >= 3600; reset <= 100us => .95/reset >= 9500.
        // Both operations can raise inexact; execute the same bound when the flag is not already set.
        if (!(mx&0x20u)) { const double bound=o.maximumFrequencyHz(); asm volatile(""::"x"(bound)); }
        return true;
    }
    static WscOutputVolts run(ReconstructedOscillator& o, double f) noexcept { return o.tickBorrowedHeld(f); }
};
}
#endif
