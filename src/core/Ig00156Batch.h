#pragma once

#include "Ig00156Filter.h"
#include <array>
#include <cassert>

// Exactness is a build contract, not merely a CPU capability. In particular,
// an FMA-enabled caller may contract the existing inline scalar expressions.
// Conservatively keep its scalar path rather than change that audio contract.
// Clang does not expose several individually enabled unsafe-math options via
// these GNU macros. No header-level strict-build proof is available there:
// keep ALL Clang callers/backends scalar, including nominally strict builds.
// Do not compensate by overriding the caller's floating-point flags.
#if defined(__x86_64__) && defined(__GNUC__) && !defined(__clang__) \
    && !defined(__INTEL_COMPILER) && !defined(__INTEL_LLVM_COMPILER) \
    && !defined(__NVCOMPILER) && !defined(__PGI) && !defined(__ibmxl__) \
    && !defined(_MSC_VER) && !defined(__FMA__) && !defined(__FAST_MATH__) \
    && !defined(__ASSOCIATIVE_MATH__) && !defined(__RECIPROCAL_MATH__) \
    && !defined(__NO_SIGNED_ZEROS__) \
    && (!defined(__FINITE_MATH_ONLY__) || !__FINITE_MATH_ONLY__) \
    && (!defined(__FLT_EVAL_METHOD__) || __FLT_EVAL_METHOD__ == 0) \
    && !defined(CS80_DISABLE_AVX2_FILTER_BATCH)
#define CS80_IG00156_EXACT_BATCH_BUILD 1
#else
#define CS80_IG00156_EXACT_BATCH_BUILD 0
#endif

namespace cs80
{
// A bounded four-lane operation, not a replacement for Stage::set or its
// lifecycle API. Call ordinary set/reReference first. Every processed field is
// written back to the actual scalar objects before returning.
struct Ig00156Batch
{
    using Stages = std::array<Ig00156Stage*, 4>;
    using Inputs = std::array<float, 4>;
    using Outputs = std::array<Ig00156Stage::Output, 4>;
    using Kernel = bool (*)(const Stages&, const Inputs&, Outputs&) noexcept;

    // Setup-thread CPU/OS feature selection. No initialization, CPUID, locks or
    // allocation are performed by processIg00156Four. A null kernel means scalar.
    static Kernel selectKernel(bool allowAcceleration = true) noexcept;
    // Cheap baseline-ISA check. A host may change its thread FP environment
    // after setup, so CPU selection alone does not authorize packed arithmetic.
    static bool environmentSupported() noexcept;
};

// This caller-specific wrapper has internal linkage: a library and its client
// may use different target/FMA flags without giving one external inline
// function inconsistent definitions (or letting its fallback be interposed).
// Inputs are deliberately copied by value before any stage/output update.
// Each pointer must be non-null. Duplicate stage pointers preserve their
// sequential scalar meaning. Partial groups belong on the scalar path.
// Returns true only when the selected SIMD kernel actually ran.
static inline bool processIg00156Four(const Ig00156Batch::Stages& stages, Ig00156Batch::Inputs input,
                                     Ig00156Batch::Outputs& output, Ig00156Batch::Kernel kernel) noexcept
{
    for (auto* stage : stages) { assert(stage != nullptr); (void) stage; }
#if CS80_IG00156_EXACT_BATCH_BUILD
    bool distinct = true;
    for (std::size_t i = 0; i < stages.size(); ++i)
        for (std::size_t j = 0; j < i; ++j)
            distinct = distinct && stages[i] != stages[j];
    if (kernel != nullptr && distinct && kernel(stages, input, output))
        return true;
#else
    (void) kernel;
#endif
    // Keep fallback inline in the caller's original arithmetic contract.
    for (std::size_t i = 0; i < stages.size(); ++i)
        output[i] = stages[i]->process(input[i]);
    return false;
}
} // namespace cs80
