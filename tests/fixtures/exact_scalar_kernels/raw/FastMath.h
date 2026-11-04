#pragma once
// Bounded-error transcendental approximations for the circuit engine's
// per-sample paths (IMPROVEMENT_PLAN 0.1, CPU pass). Legacy never calls them, so
// its audio stays byte-identical. Each function states its error bound over its
// domain; tests/FastMathTests.cpp checks it against std:: on a dense grid. These
// are numerical tools, not circuit laws.
//
// CPU pass record (2026-10-31): a polynomial exp2 (relative 2.4e-7) was also
// tried for the pitch and cutoff laws and measured no faster than glibc's FMA
// exp2f/powf under callgrind (both about 25-30 instructions a call), while it
// broke the exact cross-policy equalities several gates rely on; it was not
// kept. tan is the one libm call per stage clock that pays (about 60
// instructions with its fenv guard).

#include <cmath>

namespace cs80::fastmath
{
// tan(theta) for 0 < theta <= 1.45 (0.46 pi), in double: the series of the
// quarter angle q = theta/4 (<= 0.3625) to q^17, then two double-angle steps
// tan(2a) = 2t / (1 - t^2). Relative error <= 1e-10 over the domain (checked on
// a dense grid), far inside the nodal stage's 2e-6 truth-equivalence gate.
inline double tan(double theta) noexcept
{
    const double h = 0.25 * theta;
    const double h2 = h * h;
    double t = h * (1.0 + h2 * (1.0 / 3.0 + h2 * (2.0 / 15.0 + h2 * (17.0 / 315.0 + h2 * (62.0 / 2835.0
             + h2 * (1382.0 / 155925.0 + h2 * (21844.0 / 6081075.0 + h2 * (929569.0 / 638512875.0
             + h2 * (6404582.0 / 10854718875.0)))))))));
    t = 2.0 * t / (1.0 - t * t);
    return 2.0 * t / (1.0 - t * t);
}
} // namespace cs80::fastmath
