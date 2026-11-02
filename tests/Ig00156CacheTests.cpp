// CPU-cache continuation and residual checks. Existing independent nodal truth
// gates (linear, nonlinear, stability) remain in Ig00156NodalTests.cpp.
#include "core/Ig00156Filter.h"
#include "Ws1Tolerances.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <iostream>

namespace {
int failures = 0;
void check(bool pass, const char* label)
{
    if (!pass) { ++failures; std::cerr << "FAIL: " << label << '\n'; }
}
std::array<float, 3> outputs(const cs80::Ig00156Stage::Output& output)
{
    return { output.hp, output.lp, output.bp };
}
void solverCache(const ws1::Tolerances& tolerance)
{
    double worstDifference = 0.0, worstResidual = 0.0;
    std::uint64_t newtonSteps = 0, startsConverged = 0;
    bool finite = true, coefficientsCached = true;
    for (const auto& network : { cs80::kIg00156HighPassNetwork, cs80::kIg00156LowPassNetwork })
        for (float rate : { 88200.0f, 96000.0f, 192000.0f, 384000.0f })
            for (float cutoff : { 60.0f, 1000.0f, 4000.0f, 12000.0f, 20000.0f, 0.45f * rate })
                for (float q : { 0.35f, 1.0f, 5.0f, 9.0f })
                    for (float amplitude : { 0.0f, 1.0e-4f, 0.5f, 2.5f, 8.0f })
                    {
                        cs80::Ig00156Stage cached(network), fresh(network);
                        for (int n = 0; n < 2048; ++n)
                        {
                            const float fc = n < 768 ? cutoff : cutoff * 0.71f;
                            const float resonance = n < 768 ? q : 0.35f + 0.9f * q;
                            if (n == 1536) { cached.reset(); fresh.reset(); }
                            const auto oldRevision = cached.getCoefficientRevision();
                            cached.set(fc, resonance, rate);
                            coefficientsCached = coefficientsCached && cached.getCoefficientRevision()
                                == oldRevision + static_cast<std::uint64_t>(n == 0 || n == 768);
                            // Memory restore forces fresh coefficient/Jacobian
                            // terms without changing capacitor/slope state.
                            fresh.restoreMemory(fresh.getMemory());
                            fresh.set(fc, resonance, rate);
                            const float input = amplitude * (0.8f * std::sin(0.07f * n)
                                                           + 0.2f * std::sin(0.831f * n));
                            const auto a = outputs(cached.process(input));
                            const auto b = outputs(fresh.process(input));
                            for (std::size_t i = 0; i < a.size(); ++i)
                            {
                                finite = finite && std::isfinite(a[i]) && std::isfinite(b[i]);
                                worstDifference = std::max(worstDifference, std::abs(double(a[i]) - b[i]));
                            }
                            worstResidual = std::max(worstResidual, std::abs(cached.getSolverResidual()));
                            newtonSteps += cached.getLastNewtonSteps();
                            startsConverged += cached.getLastNewtonSteps() == 0;
                        }
                    }
    std::printf("nodal cache: maximum output difference %.12g; maximum residual %.12g\n",
                worstDifference, worstResidual);
    std::printf("nodal solver work: Newton steps %llu, converged starts %llu\n",
                static_cast<unsigned long long>(newtonSteps), static_cast<unsigned long long>(startsConverged));
    check(finite, "cached and freshly rebuilt solvers remain finite");
    check(coefficientsCached, "Jacobian terms rebuilt only when cutoff/Q/rate changes");
    check(worstDifference <= tolerance["maximum_reference_output_difference"], "cached Jacobian terms preserve solver continuation");
    check(worstResidual <= tolerance["maximum_scalar_residual"], "accepted nodal solves meet the nonlinear residual tolerance");
    check(startsConverged > 0, "converged starts skip Newton steps");
}
void zeroEquilibrium()
{
    cs80::Ig00156Stage stage;
    stage.set(12000.0f, 9.0f, 96000.0f);
    bool exact = true, noNewton = true;
    for (int n = 0; n < 1024; ++n)
    {
        const auto out = stage.process(0.0f);
        const auto steps = stage.getLastNewtonSteps();
        exact = exact && out.hp == 0.0f && out.lp == 0.0f && out.bp == 0.0f;
        noNewton = noNewton && steps == 0;
    }
    check(exact, "zero equilibrium is exact");
    check(noNewton, "zero residual performs no Newton/Jacobian work");
}
}
int main(int argc, char** argv)
{
    try
    {
        const ws1::Tolerances tolerance(argc > 1 ? argv[1] : "tests/parity/tolerances-v2/kernel-cache.json", "filter");
        solverCache(tolerance);
        zeroEquilibrium();
    }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 2; }
    return failures == 0 ? 0 : 1;
}
