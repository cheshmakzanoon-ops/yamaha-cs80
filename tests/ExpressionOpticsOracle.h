#pragma once
// Closed-form oracle for the circuit engine's EXP volume optical cell
// (src/core/ExpressionOptics.h, ws4 hand-off). Since each pedal change, ln R
// relaxes exponentially toward its target with the direction's time constant
// (12 ms brightening, 60 ms darkening; heel 1 MOhm, toe 2 kOhm) and the gain is
// ln R mapped back to 0..1. The cell starts on the pedal at the first sample;
// a settled cell gives the pedal itself. Not the production recurrence.
#include <algorithm>
#include <cmath>

namespace cs80::tests
{
struct ExpressionOpticsOracle
{
    double rate = 96000.0;
    double heel = std::log(1000000.0), toe = std::log(2000.0);
    double attackSeconds = 0.012, decaySeconds = 0.060;
    double pedal = 1.0, start = 0.0, target = 0.0, now = 0.0, tau = 0.012;
    long long samples = 0;
    bool started = false;

    double logOf(double x) const { return heel + (toe - heel) * std::clamp(x, 0.0, 1.0); }
    void setPedal(double x) { if (std::isfinite(x)) pedal = std::clamp(x, 0.0, 1.0); }
    // One internal sample; returns the expression gain.
    double tick()
    {
        const double wanted = logOf(pedal);
        if (!started)
        {
            start = target = now = wanted;
            started = true;
            return pedal;
        }
        if (wanted != target)
        {
            start = now;
            target = wanted;
            samples = 0;
            tau = target < start ? attackSeconds : decaySeconds;
        }
        if (now != target)
        {
            now = target + (start - target) * std::exp(-(double) ++samples / (tau * rate));
            if (std::abs(target - now) <= 1.0e-12 * std::max(1.0, std::abs(target)))
                now = target;
        }
        return now == target ? pedal : (now - heel) / (toe - heel);
    }
};
} // namespace cs80::tests
