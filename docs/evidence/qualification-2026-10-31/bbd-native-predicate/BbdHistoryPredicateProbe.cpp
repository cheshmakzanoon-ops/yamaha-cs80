// Diagnostic for the already-registered BBD zero_history_representation contract.
// Includes the real production predicate. No engine law or tolerance is changed.
#include "core/BbdChorus.h"
#include <bit>
#include <cstdint>
#include <cstdio>
#include <xmmintrin.h>

#if defined(__clang__) || defined(__GNUC__)
#define PROBE_NOINLINE __attribute__((noinline))
#else
#define PROBE_NOINLINE __declspec(noinline)
#endif

extern "C" PROBE_NOINLINE bool production_probe(float sample) noexcept
{
    return cs80::bbd_detail::hasNonzeroMagnitude(sample);
}

// Candidate under review: integer volatile storage breaks the compiler's
// floating-dataflow identity without performing any floating comparison.
extern "C" PROBE_NOINLINE bool integer_observation_probe(float sample) noexcept
{
    const volatile std::uint32_t bits = std::bit_cast<std::uint32_t>(sample);
    return (bits & 0x7fffffffu) != 0u;
}

int main()
{
    const unsigned saved = _mm_getcsr();
    unsigned failed = 0;
    for (unsigned mode : {0u, 0x0040u, 0x8000u, 0x8040u})
    {
        _mm_setcsr((saved & ~0x8040u) | mode);
        volatile float positive = std::bit_cast<float>(std::uint32_t{1});
        volatile float negative = std::bit_cast<float>(std::uint32_t{0x80000001u});
        volatile float positiveZero = 0.0f;
        volatile float negativeZero = -0.0f;
        const bool actual[] = {production_probe(positive), production_probe(negative),
                               production_probe(positiveZero), production_probe(negativeZero)};
        const bool candidate[] = {integer_observation_probe(positive), integer_observation_probe(negative),
                                  integer_observation_probe(positiveZero), integer_observation_probe(negativeZero)};
        const bool expected[] = {true, true, false, false};
        for (unsigned item = 0; item < 4; ++item)
        {
            std::printf("mxcsr=%08x item=%u expected=%u production=%u candidate=%u\n",
                        _mm_getcsr(), item, unsigned(expected[item]), unsigned(actual[item]), unsigned(candidate[item]));
            failed += actual[item] != expected[item];
        }
    }
    _mm_setcsr(saved);
    std::printf("Original production predicate failures=%u; original MXCSR restored=%u\n", failed, _mm_getcsr() == saved);
    return failed ? 1 : 0;
}

