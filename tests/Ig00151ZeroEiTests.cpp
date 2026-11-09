// Exact-output regression for the zero EI fast path.
#include "core/Vca.h"
#include "Ig00151VcaBaseline.h"
#include <bit>
#include <cstdint>
#include <cstdio>
#include <limits>

int main()
{
    std::size_t checks = 0, errors = 0;
    const auto same = [&](float a, float b)
    {
        ++checks;
        if (std::bit_cast<std::uint32_t>(a) != std::bit_cast<std::uint32_t>(b)) ++errors;
    };
    for (double rate : { 16000., 88200., 96000., 192000., 384000., 1536000. })
    {
        cs80::Ig00151Vca candidate;
        baseline::Ig00151Vca reference;
        candidate.prepare(rate); reference.prepare(rate);
        candidate.reset(.3f); reference.reset(.3f);
        for (int n = 0; n < 100000; ++n)
        {
            const float input = std::sin(n * .013f);
            const float cv = .8f + .4f * std::cos(n * .002f);
            const float envelope = .5f + .5f * std::sin(n * .006f);
            const float ei = n < 20000 ? 0.f : n < 40000 ? -0.f
                : n < 60000 ? .7f * std::sin(n * .017f)
                : n < 80000 ? std::numeric_limits<float>::denorm_min()
                : -std::numeric_limits<float>::denorm_min();
            if (n % 7000 == 0) { candidate.reset(cv); reference.reset(cv); }
            if (n % 137 == 0) { candidate.relaxIdle(.001); reference.relaxIdle(.001); }
            same(candidate.process(input, cv, ei, envelope), reference.process(input, cv, ei, envelope));
            same(candidate.levelNode(), reference.levelNode());
        }
        // Exceptional values retain the original path, including the resulting
        // NaN bit pattern; the shortcut is not a general sanitization policy.
        for (float ei : { 0.f, -0.f, 1.f, -1.f, 100.f, -100.f,
                          std::numeric_limits<float>::infinity(),
                          -std::numeric_limits<float>::infinity(),
                          std::numeric_limits<float>::quiet_NaN() })
        {
            candidate.reset(.3f); reference.reset(.3f);
            same(candidate.process(.2f, .7f, ei, .6f), reference.process(.2f, .7f, ei, .6f));
        }
    }
    std::printf("%zu checks, %zu failures\n", checks, errors);
    return errors ? 1 : 0;
}
