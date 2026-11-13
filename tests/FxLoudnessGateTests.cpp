#include "core/FxLoudness.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace {
constexpr double pi = 3.14159265358979323846;
constexpr double slewSeconds = 0.15;
constexpr double errorLimit = 1.0e-10;
constexpr double movementMinimum = 1.0e-5;
int checks = 0, failures = 0;
void check(bool ok, const char* name, int rate)
{
    ++checks;
    if (!ok) ++failures;
    std::printf("%s rate=%d %s\n", ok ? "PASS" : "FAIL", rate, name);
}

// Public weighting is intentionally shared with the implementation. This is a
// gate/ballistics observer, NOT an independent K-weighting standards oracle.
struct GateObserver {
    cs80::fx::KWeighting left, right;
    double energy = 0.0, peak = 0.0, energyCoef, peakCoef;
    explicit GateObserver(int rate)
        : energyCoef(1.0 - std::exp(-1.0 / rate)),
          peakCoef(std::exp(-1.0 / (3.0 * rate)))
    { left.prepare(rate); right.prepare(rate); left.reset(); right.reset(); }
    bool process(float l, float r)
    {
        const double a = left.process(l), b = right.process(r);
        energy += energyCoef * (0.5 * (a * a + b * b) - energy);
        peak = std::max(energy, peak * peakCoef);
        // Output is exactly half the input, including filter ring-down:
        // output energy is input energy / 4 for this entire history.
        return energy > 1.0e-7 && energy > 0.01 * peak && energy * 0.25 > 1.0e-12;
    }
};

void run(int rate)
{
    cs80::fx::LoudnessMatcher matcher;
    matcher.prepare(rate);
    GateObserver observer(rate);
    int openSamples = 0, closure = -1, closedSamples = 0;
    bool finite = true, reopened = false;
    double gainBeforeClosure = 1.0, gainAtClosure = 1.0, maximumError = 0.0;
    for (int frame = 0; frame < 2 * rate; ++frame) {
        const double phase = 2.0 * pi * 1000.0 * frame / rate;
        const float l = frame < rate / 2 ? static_cast<float>(0.0008 * std::sin(phase)) : 0.0f;
        const float r = frame < rate / 2 ? static_cast<float>(0.0008 * std::cos(phase)) : 0.0f;
        const bool open = observer.process(l, r);
        if (open) ++openSamples;
        // Snapshot BEFORE processing the first closed sample after real opening.
        if (!open && openSamples > 0 && closure < 0) {
            closure = frame;
            gainBeforeClosure = matcher.currentGain();
        }
        const float applied = matcher.process(l, r, 0.5f * l, 0.5f * r);
        const double actual = matcher.currentGain();
        finite = finite && std::isfinite(applied) && std::isfinite(actual);
        if (closure >= 0) {
            reopened = reopened || open;
            ++closedSamples;
            if (frame == closure) gainAtClosure = actual;
            // Target 2 follows from exact binary proportional scaling, not
            // private state access. Closed-form slew is independent of the
            // implementation's iterative update and includes the first sample.
            const double expected = 2.0 + (gainBeforeClosure - 2.0)
                * std::exp(-static_cast<double>(closedSamples) / (rate * slewSeconds));
            maximumError = std::max(maximumError, std::abs(actual - expected));
        }
    }
    const double movement = matcher.currentGain() - gainAtClosure;
    check(finite, "finite gain throughout fixed two-second history", rate);
    check(openSamples > 0, "gate genuinely opened", rate);
    check(closure >= rate / 2 && !reopened && closedSamples >= rate / 4,
          "gate closed during silence and remained closed at least 250 ms", rate);
    check(closure >= 0 && 2.0 - gainBeforeClosure >= movementMinimum,
          "nontrivial gain deficit at closure", rate);
    check(closure >= 0 && maximumError <= errorLimit,
          "held target 2 with analytic 150 ms continuing slew", rate);
    check(closure >= 0 && movement >= movementMinimum,
          "measurable gain movement after closure", rate);
    std::printf("METRIC rate=%d open_samples=%d closure_frame=%d closed_samples=%d "
                "gain_before_closure=%.17g max_slew_error=%.17g movement=%.17g\n",
                rate, openSamples, closure, closedSamples, gainBeforeClosure, maximumError, movement);
}
}
int main()
{
    for (int rate : {44100, 48000, 96000}) run(rate);
    std::printf("RESULT checks=%d failures=%d\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
