# Exact control-transform cache qualification

> Historical component qualification from `bd2ade0cbc552b7d36a41652f5179cb04d64219d`.
> The acceptance contract and frozen fixtures were published for the current
> continuation in `c881983fffbf24ef8abe7305ee60d2864711406a` before integration.
> Results below describe the original isolated component; combined verification
> and new three-round timings must be recorded separately.

2026-10-31. This is arithmetic memoization, not a change of a physical law.
The original float `midiToHz` expression, PRA ring converter, original SUB
converter and log-rate recurrence, and float DC coefficient expression are
retained. The caches reuse a result only while every relevant argument is
exactly unchanged. The old Legacy call sites remain explicit and untouched.

Acceptance was committed in `79e0913` and extended for the original SUB in
`552f9bd`, before candidate evaluation. `cs80_exact_control_transform_tests`
passes 174,978 bitwise checks against independent direct expressions and a
separate time-domain log-node recurrence. Coverage includes pitch and tuning
changes, seven rates, moving and stationary ring/SUB controls, sustained
transients and reset. Maximum allowed and measured error is exactly zero.

Five independent temporary mutations are rejected: stale pitch (36,864
failures), stale ring envelope (180), stale DC exponential argument (96,114),
stale SUB pressure (240), and bypassed moving SUB log node (69,280). The
restored production sources are unmodified by the mutation procedure.

MIDI pitch memo objects are stored with the sixteen rank-field arrays.
Neither oscillator phase nor any capacitor state is cached or skipped. The
SUB node still advances each internal sample; only repeated logarithm and
exponential evaluations are reused. The static law provenance remains in
`Dsp.h`, `RingModulatorCircuit.h` and `OriginalSubOscillator.h`. No hardware
measurement or new approximation is claimed.

Three-round timing and integrated Legacy/V1/V2 equality are recorded in
STATUS.md after integration. No CPU-budget claim follows from this exactness
test alone.

## Current integration extension: signed-zero key

Preregistration `81b8c79e5cf226c9de6bbb0a2991b2c4a76c309b` extends the
unchanged zero-error contract with both directions of `+0`/`-0` tuning-reference
transitions. Numeric equality alone reused the wrong signed zero. The cache
now also compares the reference sign bit; its prior NaN-miss comparisons remain.
The plugin tuning reference remains clamped to 415–465 Hz (Pure uses 440 Hz),
so this repairs the explicitly registered transform API boundary without
changing accepted engine control values.

All 174,978 original checks are retained, with 24,584 new transition/repeat
checks: the original key fails 9,219 of 199,562; the corrected key passes all
199,562; restoring numeric-only equality fails those 9,219 again. Exact raw
logs and candidate hashes are in
[the signed-zero record](continuation-2026-10-31/pitch-cache-signed-zero/qualification.json).
The integrated smoothing suite also passes its 84 existing checks and added
state boundaries. Final strict compatibility passes Legacy default, Legacy tails, V1 and V2
byte-identical to the unchanged V2 pin `46a7a5a`. Logs are in
[cpu-integration/01-transforms](continuation-2026-10-31/cpu-integration/01-transforms/).
The three-round after timing is recorded separately after publication; the
three-round before snapshot is `cpu-integration/cold-time`.
