#pragma once

#include "Dsp.h"

#include <cmath>

// Circuit-engine (Source Performance / Isolation) ring modulator on the PRA board:
// speed/envelope -> frequency converter and the RMO multiplier with offsets. The legacy
// policy keeps ringModulationFrequency() and the ideal multiply in Engine.cpp, bit for bit.
// Evidence classes and alternatives: docs/workstreams/ws4-circuits.md, Step 2. Truth
// model: src/reference/RingModulator.h.
namespace cs80
{
struct RingModulatorCircuit
{
    // Yamaha adjustment procedure, printed p19 (PRA): depth OFF, VC1 = +5.0 V gives a
    // 150 ms +/-5 % period; maximum SPEED, depth OFF, gives 5 ms +/-5 %. Source-derived.
    static constexpr double kAnchorVolts = 5.0;
    static constexpr double kAnchorHz = 1.0 / 0.150;
    static constexpr double kMaxSpeedHz = 1.0 / 0.005;
    // DECLARED INFERENCE: maximum SPEED is VC1 = +10 V, the 0-10 V panel-control span.
    // A linear VCO cannot meet both anchors with f(0 V) >= 0, so the converter is taken
    // as exponential: log2(30) / 5 V = 0.981 octave per volt.
    static constexpr double kMaxSpeedVolts = 10.0;
    // DECLARED INFERENCE: the AD envelope output EO peaks at +5 V (the IG00152 attack
    // level span, service p42) and full DEPTH adds it to the converter node, so full
    // depth sweeps log2(30) = 4.907 octaves above the base speed.
    static constexpr double kEnvelopePeakVolts = 5.0;

    static double octavesPerVolt() noexcept
    {
        return std::log2(kMaxSpeedHz / kAnchorHz) / (kMaxSpeedVolts - kAnchorVolts);
    }
    // VC1 -> frequency, and the inverse used to place the host SPEED value on that scale.
    static double hzForVolts(double volts) noexcept
    {
        return kAnchorHz * std::exp2((volts - kAnchorVolts) * octavesPerVolt());
    }
    static double voltsForHz(double hz) noexcept
    {
        return kAnchorVolts + std::log2(hz / kAnchorHz) / octavesPerVolt();
    }
    // Carrier frequency for a SPEED value in Hz, DEPTH 0..1 and envelope 0..1. Written as a
    // factor on the base so DEPTH 0 returns the SPEED value exactly.
    static float frequency(float baseHz, float depth, float envelope) noexcept
    {
        const double control = (double) clampf(depth, 0.0f, 1.0f) * (double) clampf(envelope, 0.0f, 1.0f)
                             * kEnvelopePeakVolts;
        return (float) ((double) std::max(0.0f, baseHz) * std::exp2(control * octavesPerVolt()));
    }

    // RMO multiplier, as an offset four-quadrant multiplier (DECLARED INFERENCE; the PRA
    // multiplier's trim residues are not recorded):
    //   y = (x + kSignalOffset) * (carrier + kCarrierOffset) + kOutputOffset
    // kSignalOffset leaks the carrier (feedthrough), kCarrierOffset leaks the signal, and
    // the products plus kOutputOffset give the DC offset, which the PRA input coupling
    // removes downstream. Engine units: one voice at full level is about 0.1.
    static constexpr float kSignalOffset = 5.0e-4f;   // carrier -40 dB below a 0.1 voice's sidebands
    static constexpr float kCarrierOffset = 5.0e-3f;  // signal -40 dB below its own sidebands
    static constexpr float kOutputOffset = 1.0e-3f;   // multiplier output DC

    static float multiply(float x, float carrier) noexcept
    {
        return (x + kSignalOffset) * (carrier + kCarrierOffset) + kOutputOffset;
    }
    // Panel MODULATION (VR7) blends MO (dry) and RMO.
    static float blend(float x, float carrier, float amount) noexcept
    {
        return lerp(x, multiply(x, carrier), amount);
    }
};
} // namespace cs80
