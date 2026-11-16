#pragma once
#include "Dsp.h"
#if defined(__x86_64__)
#include <xmmintrin.h>
#endif
namespace cs80 {
// Unconditional layout/access definitions, identical across all caller TUs.
struct StereoFirView { const float* newest; const float* coeffs; int center,taps; };
struct StereoFirAccess {
    static StereoFirView view(const OversamplingDecimator& d) noexcept {
        return {d.hist.data()+d.idx-1+d.taps,d.coeffs.data(),d.center,d.taps};
    }
};
struct StereoFirResult { std::array<float,2> samples; bool accelerated; };
// Internal linkage deliberately isolates caller FP/ISA build contracts.
static inline StereoFirResult stereoFirOutput(const OversamplingDecimator& l,
                                              const OversamplingDecimator& r) noexcept {
#if defined(__x86_64__) && defined(__GNUC__) && !defined(__clang__) \
 && !defined(__INTEL_COMPILER) && !defined(__INTEL_LLVM_COMPILER) && !defined(__NVCOMPILER) \
 && !defined(__PGI) && !defined(__ibmxl__) && !defined(_MSC_VER) \
 && !defined(__FMA__) && !defined(__FAST_MATH__) && !defined(CS80_DISABLE_AVX2_FILTER_BATCH) \
 && !defined(__ASSOCIATIVE_MATH__) && !defined(__RECIPROCAL_MATH__) && !defined(__NO_SIGNED_ZEROS__) \
 && (!defined(__FINITE_MATH_ONLY__) || !__FINITE_MATH_ONLY__) \
 && (!defined(__FLT_EVAL_METHOD__) || __FLT_EVAL_METHOD__ == 0)
    const auto a=StereoFirAccess::view(l),b=StereoFirAccess::view(r);
    if(a.center==b.center && (_mm_getcsr()&0x1f80u)==0x1f80u) {
        std::array<double,2> y{(double)a.coeffs[a.center]*a.newest[-a.center],
                               (double)b.coeffs[b.center]*b.newest[-b.center]};
        for(int i=0;i<a.center;++i) {
            y[0]+=(double)a.coeffs[i]*((double)a.newest[-i]+(double)a.newest[-(a.taps-1-i)]);
            y[1]+=(double)b.coeffs[i]*((double)b.newest[-i]+(double)b.newest[-(b.taps-1-i)]);
        }
        return {{(float)y[0],(float)y[1]},true};
    }
#endif
    return {{l.output(),r.output()},false};
}
}
