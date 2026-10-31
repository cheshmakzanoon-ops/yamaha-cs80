#pragma once
// Circuit-engine sine-only voice source exactly as Engine runs it since plan
// step 1.2 (docs/OSCILLATOR_PROMOTION.md): the reconstructed WSC sine pin at
// the internal rate, cold-started at the voice's assignment phase, saw and
// pulse gates off, through the fixed kWscEngineUnitsPerVolt adapter. Replaces
// the former sin + 3.5 % third-harmonic placeholder in pipeline oracles.
#include "core/ReconstructedOscillator.h"

struct CircuitSineSource
{
    cs80::ReconstructedOscillator osc;
    CircuitSineSource(double internalRate, float assignmentPhase)
    {
        osc.prepare(internalRate, cs80::OscillatorNominalProfile {});
        osc.reset(assignmentPhase, cs80::OscillatorNominalProfile {}.referenceHzNominal);
    }
    // pulseWidth is the panel duty proxy the engine passes (default 0.5).
    float operator()(float hz, float pulseWidth = 0.5f)
    {
        cs80::WscControls c;
        c.sawEnabled = c.pulseEnabled = false;
        c.pulseThresholdVolts = cs80::ReconstructedOscillator::panelThresholdVoltsInferred(pulseWidth);
        return static_cast<float>(osc.tick(hz, c).sinePin2 * cs80::kWscEngineUnitsPerVolt);
    }
};
