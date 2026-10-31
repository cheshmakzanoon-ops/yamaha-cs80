#pragma once
// Circuit-engine sine-only voice source exactly as Engine runs it since plan
// step 1.2 (docs/OSCILLATOR_PROMOTION.md): the reconstructed WSC sine pin at
// the internal rate, cold-started at the voice's assignment phase, saw and
// pulse gates off, through the fixed kWscEngineUnitsPerVolt adapter. Replaces
// the former sin + 3.5 % third-harmonic placeholder in pipeline oracles.
#include "core/ReconstructedOscillator.h"
#include "core/Vca.h"

struct CircuitSineSource
{
    cs80::ReconstructedOscillator osc;
    CircuitSineSource(double internalRate, float assignmentPhase)
    {
        osc.prepare(internalRate, cs80::OscillatorNominalProfile {});
        osc.reset(assignmentPhase, cs80::OscillatorNominalProfile {}.referenceHzNominal);
    }
    // Phase 2.1 (free-running voices), intentional circuit-engine change: a
    // circuit is never reset at assignment and starts warm, so the oracle
    // continues an exact copy of the engine oscillator's state at the note-on.
    CircuitSineSource(double internalRate, const cs80::ReconstructedOscillator::State& atAssignment)
    {
        osc.prepare(internalRate, cs80::OscillatorNominalProfile {});
        osc.restoreState(atAssignment);
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

// Circuit-engine voice VCA as Engine runs it since plan step 1.4
// (docs/IG00151_VCA_MODEL.md) for a rank-I-only (MIX 0), centre-panned voice
// with no SUB VCA modulation: IG00151 LI node at the LC level, then the 0.17
// voice scale and the 0.5 centre pan gain. Replaces sine * level * amp.
struct CircuitVoiceVca
{
    cs80::Ig00151Vca vca;
    explicit CircuitVoiceVca(double internalRate, float level = 1.0f)
    {
        vca.prepare(internalRate);
        vca.reset(level);
    }
    float operator()(float input, float amp, float level = 1.0f)
    {
        return vca.process(input, level, 0.0f, amp) * 1.0f * 0.17f * 0.5f;
    }
};
