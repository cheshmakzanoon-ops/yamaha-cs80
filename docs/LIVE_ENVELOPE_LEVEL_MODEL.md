# Live envelope level targets: engineering model

2026-10-29. **Inferred production behavior for opt-in isolation mode; not a recovered or measured IC law.** This implements the updated [public-knowledge fidelity requirement](ANALOG_FIDELITY_REQUIREMENT.md). Missing hardware does not block this model; its assumptions remain explicit and revisable.

## Source basis and engineering choice

The [target-path review](ENVELOPE_TARGET_TRANSITION_REVIEW.md) locates the selected LC target, opposing buffered feedback and a separate external integrating capacitor. That supports keeping stored state distinct from a control target. It does not reveal the chip's complete sequence thresholds, hysteresis, loading or charging-current law.

The chosen reduced-order policy preserves the current stage, visible value and elapsed stage time when a parameter changes. A selected target is followed through the existing provisional exponential time law. An inactive target is stored for its relevant phase. This avoids both permanent note-on latching and an instantaneous assignment of capacitor-state proxy to a moved control.

The phase-selection policy, reuse of decay/release timing and handling of a target crossing the current value are **engineering inferences**. In particular, an alternative directional-comparator model might advance a phase immediately after a target crossing; the present model instead follows the currently selected target until normal endpoint completion. No source observation is claimed to distinguish these alternatives. The choice prioritizes bounded, state-continuous live control and can be refined later without falsifying the source ledger.

## Stage policy

| Control change | Active behavior | Other phases |
|---|---|---|
| VCA sustain level S | Decay follows the new S using the existing decay law; held Sustain follows it with the unbent decay-time coefficient | Attack stores S for subsequent Decay; Release and Idle do not respond to S |
| Filter attack level AL | Attack follows the moved AL; the old onset-time timeout is disabled after an actual AL change so it cannot force a large snap | Stored without changing the grounded Decay/Sustain target or release target |
| Filter initial level IL | Release follows the new IL with its existing release law | Stored during Attack/Decay/Sustain |
| IL after completed filter release | If the voice is still active because its amplitude tail remains audible, completed-release Idle retains IL selection and follows changes with the unbent release coefficient | Reset/untriggered Idle remains inert; no inactive voice is resurrected |

The setters do not trigger a note, reset an oscillator/filter, change allocation or clear held/pedal state. Gate transitions still use the existing note-on/off policy. Reset and note-on clear the live-tracking flags. The ring envelope is not changed by this model.

## Numerical behavior and compatibility

The existing provisional law is `exp(-6 * multiplier / (time * sampleRate))`, with the existing time floor and phase bend windows. Held-sustain and completed-release idle tracking use multiplier1 rather than restarting a fast window. Existing endpoint thresholds remain: 0.0005 for VCA sustain/decay, 0.001 for filter attack and 0.0008 for filter release. These are numerical completion choices, not claimed transistor/comparator tolerances.

Live-target paths retain double-precision accumulator state. A float-only recurrence can stall far from a slowly approached target at long times/high sample rates; suppressing its old timeout without addressing this would leave an envelope stuck. Repeated same-target setters do not quantize or resynchronize the accumulator. Once an endpoint is reached, the established small endpoint snap is retained; state continuity does not mean an infinitely precise analog trajectory.

The initial live-level checkpoint retained original arithmetic when targets had not changed. The subsequent [numerical correction](ENVELOPE_NUMERICAL_ACCURACY.md) uses double state, coefficients and elapsed clocks in every isolation phase, including untouched controls and ring envelopes; static isolation capture responses therefore intentionally change. Engine activation is restricted to active voices in isolation mode; legacy-off rendering and its historical target-latching behavior remain unchanged. No host parameter is added or reordered. New behavior is intentionally audible when live targets change in isolation, including selected memory/tone/PANEL changes; this is not represented as a bit-identical replacement for those formerly latched transitions.

## Verification scope

Core tests cover setters, applicable/inactive targets, upward/downward and repeated moves, target crossings, both ranks, reset/note-on hygiene, stage-time preservation, block partitioning and high-rate endpoint convergence. Host tests exercise held sustain and selected versus unselected memory/PANEL changes with separately advanced primitive histories. Numerical reference checks verify the declared model, not a physical CS-80 trajectory.

The focused suite passes **221,594 checks**, including endpoint convergence at the supported 1.536 MHz internal rate. At the initial live-level checkpoint, all 24 local Release and ASan/UBSan CTest suites passed; local leak detection was off. Seven mutants are rejected: target latching, instantaneous sustain assignment, stage-time reset, legacy activation, stale attack timeout, float accumulator stall and repeated-setter accumulator quantization.

The standard and noisy reassignment/tail raw corpora remain byte-identical to pinned baseline `d56911a7c5f6953cd244457226b126cc567a538b` under the same compiler. Their hashes are respectively `4a3483f77c9df9895386b98eaa169358d19b463c21283231f994c5d0f6c523de` and `fe447882e3ea3560eefb7b5ae4a474b151efe5ced775b5ab36f2609c5fde0660`. This is not a cross-compiler hash promise. JUCE host tests require the exact-commit CI run; source review and local core checks do not replace that execution. The unknown chip law is a refinement opportunity, not a development gate.
