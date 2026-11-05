#pragma once

// Diagnostics only: use a monotonic clock and report the measured times in the
// test's own output. CTest may replay that output only after a timeout, so its
// outer log timestamps cannot identify an expensive phase.
#include <chrono>
#include <cstdio>
#include <utility>

namespace cs80::tests::ui_timing
{
using Clock = std::chrono::steady_clock;

inline double secondsSince(Clock::time_point start, Clock::time_point end)
{
    return std::chrono::duration<double>(end - start).count();
}

class Phases
{
public:
    void begin(const char* name)
    {
        const auto now = Clock::now();
        report(now);
        std::printf("UI SMOKE PHASE: %s\n", name);
        previous = name;
        phaseStart = now;
    }

    void finish() const { report(Clock::now()); }

private:
    void report(Clock::time_point now) const
    {
        if (previous != nullptr)
            std::printf("UI SMOKE TIMING: phase=%s seconds=%.6f total_seconds=%.6f\n",
                        previous, secondsSince(phaseStart, now), secondsSince(start, now));
    }

    const Clock::time_point start = Clock::now();
    Clock::time_point phaseStart = start;
    const char* previous = nullptr;
};

// Accumulate an operation without changing its return type or execution count.
// No output or message-loop dispatch occurs inside the measured operation.
class Duration
{
public:
    template <typename Operation>
    decltype(auto) measure(Operation&& operation)
    {
        struct Sample
        {
            double& total;
            const Clock::time_point start = Clock::now();
            ~Sample() { total += secondsSince(start, Clock::now()); }
        } sample { seconds };
        return std::forward<Operation>(operation)();
    }

    double seconds = 0.0;
};
} // namespace cs80::tests::ui_timing
