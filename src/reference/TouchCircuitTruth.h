#pragma once

// Offline truth model for the TWS a/b/C nodes and the TRG5 diode-OR bus
// (IMPROVEMENT_PLAN 2.4). NOT used by Engine. Same declared circuit as
// src/core/TouchCircuit.h, integrated independently: backward Euler with
// `substeps` sub-steps per sample on the continuous ODEs, the ideal clamp
// applied as a constraint (memory never above b), and the bus solved to
// convergence. It holds the realtime discretisation to the declared model; it
// is not a measured CS-80.

#include "core/TouchCircuit.h"

#include <array>
#include <cmath>

namespace cs80::reference::touch_truth
{
class TouchVoiceTruth
{
public:
    explicit TouchVoiceTruth(int substeps = 64) : substeps(substeps) {}
    void trigger(double strikeValue) { strike = strikeValue; elapsedSeconds = 0.0; acquiring = true; }
    struct Output { double a, b, c; };
    // One sample of length dt. The coupled input to the hold node is 0 for the
    // contact lead, then the strike value, while the window is open.
    Output tick(double dt, double pressure, double holdLeakSeconds = cs80::touch::kHoldLeakSeconds)
    {
        namespace t = cs80::touch;
        const double h = dt / substeps;
        double y = 0.0;
        for (int i = 0; i < substeps; ++i)
        {
            a = (a + h / t::kAPathSeconds * pressure) / (1.0 + h / t::kAPathSeconds);
            const bool open = acquiring && elapsedSeconds < t::kWindowSeconds;
            if (open)
            {
                const double input = elapsedSeconds < t::kContactSeconds ? 0.0 : strike;
                b = (b + h / t::kAcquireSeconds * input) / (1.0 + h / t::kAcquireSeconds);
            }
            else if (holdLeakSeconds > 0.0)
                b = b / (1.0 + h / holdLeakSeconds);
            memory = (memory + h / t::kCPathSeconds * b) / (1.0 + h / t::kCPathSeconds);
            if (memory > b) memory = b; // ideal clamp
            elapsedSeconds += h;
        }
        y = b - memory;
        return { a, b, -y };
    }

private:
    int substeps;
    double a = 0.0, b = 0.0, memory = 0.0, strike = 0.0, elapsedSeconds = 1.0e9;
    bool acquiring = false;
};

template <std::size_t N>
double busVolts(const std::array<double, N>& a)
{
    return cs80::touch::busVolts(a, 0.5 * cs80::touch::kFullScaleVolts, 200);
}
} // namespace cs80::reference::touch_truth
