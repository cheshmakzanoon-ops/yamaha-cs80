#include "reference/MemoryPanelPorts.h"
#include <bit>
#include <cstdint>
#include <cstdio>
#include <limits>

namespace mp = cs80::reference::memory_panel;
int main()
{
    int failures = 0, cases = 0, derivatives = 0;
    double worst = 0.0;
    auto check = [&](bool pass) { if (!pass) ++failures; };
    for (int rank : {0, 1})
        for (int selector : {0, 5, 10, 11, 12, 13})
            for (double pot : {10000.0, 50000.0, 100000.0})
            {
                mp::Setup setup;
                setup.rank = rank;
                setup.selector = selector;
                setup.electrical.potOhms = pot;
                for (size_t bank = 0; bank < setup.banks.size(); ++bank)
                    for (size_t i = 0; i < setup.banks[bank].size(); ++i)
                        setup.banks[bank][i] = static_cast<double>((i + bank * 3) % 5) / 4.0;
                auto plain = mp::solve(setup);
                auto ports = mp::linearise(setup);
                ++cases;
                check(plain.converged && ports.valid);
                for (size_t row = 0; row < 26; ++row)
                    check(std::bit_cast<uint64_t>(plain.busVolts[row])
                          == std::bit_cast<uint64_t>(ports.operatingPoint.busVolts[row]));
                for (size_t source = 0; source < mp::Ports::count; ++source)
                {
                    constexpr double h = 1e-7;
                    auto plus = setup, minus = setup;
                    const size_t index = static_cast<size_t>(ports.rows[source] - 1);
                    plus.injectedAmps[index] = h;
                    minus.injectedAmps[index] = -h;
                    auto a = mp::solve(plus), b = mp::solve(minus);
                    check(a.converged && b.converged);
                    for (size_t target = 0; target < mp::Ports::count; ++target)
                    {
                        const size_t output = static_cast<size_t>(ports.rows[target] - 1);
                        const double fd = (a.busVolts[output] - b.busVolts[output]) / (2.0 * h);
                        const double analytic = ports.loadedOhms[target * mp::Ports::count + source];
                        const double error = std::abs(fd - analytic);
                        check(std::isfinite(error) && error < 0.1 + 1e-5 * std::abs(analytic));
                        worst = std::max(worst, error);
                        ++derivatives;
                    }
                }
            }
    // A nonzero loaded operating point is also differentiable; no unloaded-source claim.
    mp::Setup biased;
    for (auto& bank : biased.banks) bank.fill(0.5);
    biased.injectedAmps[7] = 100e-6;
    auto ports = mp::linearise(biased);
    check(ports.valid);
    auto moved = biased;
    moved.injectedAmps[7] += 1e-8;
    auto nonlinear = mp::solve(moved);
    check(nonlinear.converged);
    for (size_t target = 0; target < mp::Ports::count; ++target)
    {
        size_t source = 0;
        while (ports.rows[source] != 8) ++source;
        const size_t row = static_cast<size_t>(ports.rows[target] - 1);
        const double predicted = ports.operatingPoint.busVolts[row] + ports.loadedOhms[target * mp::Ports::count + source] * 1e-8;
        check(std::abs(predicted - nonlinear.busVolts[row]) < 1e-7);
    }
    for (double invalid : {std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()})
    {
        auto bad = biased; bad.injectedAmps[0] = invalid;
        check(!mp::solve(bad).converged && !mp::linearise(bad).valid);
    }
    for (size_t row : {3U, 4U})
    {
        auto bad = biased; bad.injectedAmps[row] = 1e-6;
        mp::Jacobian stale; stale.dimension = 123; stale.values = {1.0};
        check(!mp::solve(bad, &stale).converged && stale.dimension == 0 && stale.values.empty());
        check(!mp::linearise(bad).valid);
    }
    std::printf("%d operating points, %d derivatives, worst absolute error %.9g ohms, failures %d\n", cases, derivatives, worst, failures);
    return failures != 0;
}
