# Required analog circuit fidelity

User requirement updated 2026-10-29: **target 1:1 recreation of the real Yamaha CS-80 as far as publicly available knowledge and sound engineering allow**, including analog circuit behavior, every software-reproducible physical feature, control, tone and preset. This supersedes the earlier hardware-first development gate.

This is the engineering target, **not a claim that the present implementation achieves measured or exact hardware parity**. The present instrument is a circuit-informed behavioral model with substantial provisional laws and offline circuit references still awaiting integration.

## Required scope
- Reconstruct documented circuit topology, IC behavior, control-voltage paths and interactions between stages, not merely a similar final spectrum.
- Include nonlinear transfer characteristics, saturation, signal-dependent resonance, envelope and transient behavior, oscillator reset and free-running behavior, component tolerances, voice variation, temperature-dependent drift, noise, leakage and supply-related effects where the circuitry supports them. Where public sources leave a quantitative law unspecified, implement a justified physical or behavioral engineering model rather than waiting for hardware.
- Cover keyboard scanning and assignment, initial touch and polyphonic pressure, ribbon, pedals, programmer and preset networks, modulation, filters, amplifiers, chorus/tremolo and output circuitry.
- Preserve electrical state across performance events wherever supported; infer and test a declared state model where the precise behavior is unknown. Do not silently introduce convenient digital resets or permanent velocity scaling.
- Model variation using documented tolerances and defensible parameter ranges where available; label inferred distributions. Arbitrary random detuning or generic saturation is not evidence of circuit equivalence.
- Reproduce the audible and control consequences of physical features in software. Key force/travel, ribbon feel, chassis and mains safety cannot themselves be recreated by a plugin; state controller needs and distinguish these physical limits from implementable behavior.

## Evidence, inference and acceptance
For each subsystem record primary source and page where available, circuit derivation, unknowns, implemented equations, assumptions, alternatives considered, test stimuli and validation results. Distinguish **source-derived**, **engineering-inferred**, **numerically verified** and **hardware-measured** results. A justified inference is permitted production behavior; it is not a newly discovered hardware fact. Never fabricate source values, recordings or measurements, and preserve source contradictions.

**Hardware access is not a prerequisite for implementation, integration, milestone acceptance or release.** When a feature is known to exist but its exact law is unavailable, use public evidence plus programming and circuit knowledge to implement the strongest defensible model. Declare uncertain parameters and ranges, test their sensitivity, and review the model against known anchors and alternative hypotheses. Record the missing measurement as an optional future refinement, not a stop condition.

Use DC operating points, transfer curves, frequency/phase response, transient response, modulation spectra, noise statistics and representative polyphonic performance comparisons as appropriate. Evaluate stage-level and complete-signal-path behavior. Establish numerical convergence, stability, sample-rate consistency, compatibility and perceptual regressions independently of hardware comparison. Public recordings may inform listening tests, but uncontrolled recordings must not be presented as calibrated electrical references.

Exact equality to every surviving analog unit is not a meaningful single reference: individual units vary. Declare the nominal model and its uncertainty. If identified instrument measurements become available, record calibration, temperature, component condition and control positions, then quantify residuals and refine the model. Hardware comparison is optional future validation, not a prerequisite to pursuing the public-knowledge target.

Passing software tests, host validators or screenshot jobs does not certify analog equivalence. Completion claims must state their scope and evidence: a public-knowledge reconstruction milestone may be completed with disclosed engineering inferences and known limits; **measured hardware parity**, **bit-exact analog cloning** or unqualified **verified 1:1** must never be claimed without supporting evidence. Missing features remain work to do, not something to conceal by changing labels.
