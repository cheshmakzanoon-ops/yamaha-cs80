#pragma once
// Circuit-engine EXP volume path as an optical cell (ws4 hand-off; Phase 3
// output stage). Source placement (docs/FOOT_CONTROL_EVIDENCE.md): OE1/OE2 ->
// PRA I2/I3 optical expression -> stereo VOLUME -> output preamps; the optical
// branches I2 -> VL1 and I3 -> VL2 are driven by the same EI pedal line as the
// wah photoresistor. Their steady-state law and polarity are not recovered.
//
// DECLARED model: the same LDR law as the PRA wah cell (PraWahCircuit::Profile:
// heel/toe resistances, 12 ms brightening / 60 ms darkening in log R), with
// log R mapped back linearly to the 0..1 pedal position, so the steady-state
// gain is the existing linear expression gain and only its dynamics change.
// Toe = bright = full level. Alternatives: a shunt LDR (reversed polarity, slow
// opening), a different taper, separate trims per channel (VR11/VR12).
#include "core/PraWahCircuit.h"

#include <algorithm>
#include <cmath>

namespace cs80::stationary_before
{
class ExpressionOpticalCell
{
public:
    ExpressionOpticalCell() noexcept { prepare(96000.0); }

    // Bounded work, no allocation: safe on the render thread (factor change).
    void prepare(double sampleRate, const PraWahCircuit::Profile& p = PraWahCircuit::Profile {}) noexcept
    {
        const double rate = sampleRate > 0.0 ? sampleRate : 96000.0;
        // Identical coefficients to PraWahCircuit::prepare.
        attackStep = p.opticalTauSeconds == 0.0 ? 1.0 : -std::expm1(-1.0 / (p.opticalTauSeconds * rate));
        decayStep = p.opticalDecaySeconds == 0.0 ? 1.0 : -std::expm1(-1.0 / (p.opticalDecaySeconds * rate));
        heelLog = std::log(p.heelOhms);
        toeLog = std::log(p.toeOhms);
        reset(1.0);
    }
    // Start exactly on a pedal position (no sweep after a reset).
    void reset(double pedal) noexcept
    {
        setPedal(pedal);
        opticalLog = targetLog;
    }
    void setPedal(double pedal) noexcept
    {
        if (!std::isfinite(pedal)) return;
        pedalTarget = std::clamp(pedal, 0.0, 1.0);
        targetLog = heelLog + (toeLog - heelLog) * pedalTarget;
    }
    // One internal sample of the cell (same recurrence as the wah cell);
    // returns the optical pedal position, the expression gain. A settled cell
    // returns the pedal itself, so a static pedal keeps its exact gain.
    double tick() noexcept
    {
        opticalLog += (targetLog < opticalLog ? attackStep : decayStep) * (targetLog - opticalLog);
        if (std::abs(targetLog - opticalLog) <= 1.0e-12 * std::max(1.0, std::abs(targetLog)))
            opticalLog = targetLog;
        return position();
    }
    double position() const noexcept
    { return opticalLog == targetLog ? pedalTarget : (opticalLog - heelLog) / (toeLog - heelLog); }
    double opticalResistance() const noexcept { return std::exp(opticalLog); }

private:
    double attackStep = 1.0, decayStep = 1.0, heelLog = 0.0, toeLog = 1.0;
    double opticalLog = 0.0, targetLog = 0.0, pedalTarget = 1.0;
};
} // namespace cs80::stationary_before
