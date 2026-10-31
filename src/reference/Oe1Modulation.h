#pragma once
// OFFLINE truth model for the OE1 chorus/tremolo modulation that drives OE2, never
// production audio. Closed form in double precision. Evidence classes and the
// alternatives considered: docs/workstreams/ws4-circuits.md, Step 3.
#include <array>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <stdexcept>

namespace cs80::reference::oe1
{
// Adjustment procedure printed 21 (BA617_SOURCE_REVIEW.md): DO1 56 ms and DO2 0.55 s
// (each +/-5 %) at maximum SPEED with TREMOLO on. Source-derived. Both oscillators are
// taken to track the one SPEED control at this ratio (DECLARED INFERENCE).
inline constexpr double kDo1MaxPeriod = 0.056, kDo2MaxPeriod = 0.55;
inline constexpr double do2PerDo1() { return kDo1MaxPeriod / kDo2MaxPeriod; }

// Same table: O1/O2/O3/O4 0.65/0.58/0.58/0.65 Vpp under the VCA-trim setup. O1 (IC7) and
// O2 (IC9) hang on the upper MN3001 pair through IC5 and return on OE1 I2/I3 (left); O3
// (IC8) and O4 (IC10) on the lower pair through IC6, I4/I5 (right). Source-derived
// routing (OE2_CIRCUIT_EVIDENCE.md); the left/right assignment of the two OE1 summing
// amplifiers is the routing ledger's reading.
inline constexpr std::array<double, 4> kBranchVpp {0.65, 0.58, 0.58, 0.65};

// Oscillator values at a sample time t (seconds) for a SPEED value in Hz. DECLARED: both
// sines, both starting at phase 0, DO1 at the SPEED value, DO2 at the documented ratio.
struct Oscillators { double do1 = 0, do2 = 0; };
inline Oscillators oscillators(double speedHz, double t)
{
    const double w = 2.0 * std::numbers::pi * speedHz * t;
    return {std::sin(w), std::sin(w * do2PerDo1())};
}

// CHORUS: the BA617 clock node has two equal 100 k modulation feeds (OE57); DECLARED:
// one from each oscillator, so the clock moves by the mean of DO1 and DO2.
inline double clockOctaves(const Oscillators& o, double depth, double fullOctaves)
{
    if (!(depth >= 0 && depth <= 1)) throw std::invalid_argument("OE1 reference: depth outside 0..1");
    return depth * fullOctaves * 0.5 * (o.do1 + o.do2);
}

// IG00151 VCA gains for O1..O4. At rest each left/right pair sums to wetGain in the
// trimmed O1..O4 proportions. TREMOLO (DECLARED): linear control input, modulation index 1
// at full DEPTH, LO1 = +DO1, LO2 = +DO2, LO3 = -DO2, LO4 = -DO1, so left and right move in
// antiphase at two rates and the mono sum of the wet returns stays constant.
inline std::array<double, 4> vcaGains(const Oscillators& o, double depth, bool tremolo, double wetGain)
{
    if (!(depth >= 0 && depth <= 1)) throw std::invalid_argument("OE1 reference: depth outside 0..1");
    const double pair = kBranchVpp[0] + kBranchVpp[1];
    const double m = tremolo ? depth : 0.0;
    const std::array<double, 4> control {o.do1, o.do2, -o.do2, -o.do1};
    std::array<double, 4> g {};
    for (std::size_t k = 0; k < 4; ++k)
        g[k] = wetGain * kBranchVpp[k] / pair * (1.0 + m * control[k]);
    return g;
}
} // namespace cs80::reference::oe1
