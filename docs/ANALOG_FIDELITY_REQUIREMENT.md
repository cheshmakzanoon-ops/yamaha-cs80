# Required analog circuit fidelity

User requirement confirmed 2026-10-28: **analog circuit behavior must be a 1:1 recreation of the real Yamaha CS-80**, in addition to every physical feature, control, tone and preset.

This is a target and acceptance requirement, **not a claim that the present implementation achieves it**.

## Required scope
- Reconstruct documented circuit topology, IC behavior, control-voltage paths and interactions between stages, not merely a similar final spectrum.
- Include nonlinear transfer characteristics, saturation, signal-dependent resonance, envelope and transient behavior, oscillator reset and free-running behavior, component tolerances, voice variation, temperature-dependent drift, noise, leakage and supply-related effects where supported by the original circuitry and measurement.
- Cover keyboard scanning and assignment, initial touch and polyphonic pressure, ribbon, pedals, programmer and preset networks, modulation, filters, amplifiers, chorus/tremolo and output circuitry.
- Preserve electrical state across performance events as the hardware does. Do not introduce convenient digital resets or permanent velocity scaling without source evidence.
- Model variation as calibrated physical parameters; arbitrary random detuning or generic saturation is not evidence of circuit equivalence.

## Evidence and acceptance
For each subsystem record primary source and page, circuit derivation, unknowns, implemented equations, test stimuli, reference data and measured error. Separate schematic-derived nominal predictions from measurements on an identified instrument with documented calibration, temperature, component condition and control positions.

Use DC operating points, transfer curves, frequency/phase response, transient response, modulation spectra, noise statistics and representative polyphonic performance comparisons as appropriate. Evaluate stage-level and complete-signal-path behavior. Establish numerical convergence and sample-rate consistency independently of hardware comparison.

Exact equality to every surviving analog unit is not a meaningful single reference: individual units vary. Identify the reference unit or calibrated nominal model and explicitly quantify uncertainty and variation. Do not silently weaken the user's 1:1 target to subjective resemblance. Without sufficient reference measurements, mark the affected behavior **measurement required**, not verified.

Passing software tests, host validators or screenshot jobs is necessary engineering evidence but does not certify analog equivalence. No completion or 1:1 claim is permitted while circuit coverage or validation gaps remain.
