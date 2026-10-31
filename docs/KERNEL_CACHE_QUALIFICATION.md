# Analytic scheduler and nodal solver caches

Numerical engineering work, 2026-10-31. These are software measurements of the
existing declared circuit models, not measurements of a hardware instrument.
The physical model and alternatives remain in
[OSCILLATOR_REALTIME_PROPOSAL.md](OSCILLATOR_REALTIME_PROPOSAL.md) and
[IG00156_NODAL_MODEL.md](IG00156_NODAL_MODEL.md).

## Oscillator event schedule

The old scheduler searched only as far as the end of the current sample. The
new scheduler searches to the next physical boundary (reset starts or ends),
records the first comparator, fold, shaper or turning-point event, and counts
down that time across samples. Every frequency/threshold change invalidates
the schedule, including a change small enough to omit its antialias residual.
Reset, preparation, idle evolution and snapshot restoration invalidate it too.
An oscillator copy retains its schedule. Stopped oscillators retain the bounded
one-sample search because their physical horizon is infinite.

The transfer laws, coupling poles, shaper knots and antialias kernel are
unchanged. Searching from a segment origin changes floating-point root
bracketing and therefore circuit-engine bytes; the integration must publish
this audio commit and separately re-pin the V2 audio gate. Legacy and V1
numerics are independent of this oscillator.

Acceptance was registered before evaluation in
`tests/parity/tolerances-v2/kernel-cache.json` (local commit `b4bf20a`, published
as `3bef95d10646c3317e8126d2db938bb8cdaa4b32`). The continuation oracle restores
physical state before every sample, forcing a fresh search. This checks cache
semantics; the existing independent continuous-waveform and event-time truth
tests still supply physical-equation validation.

| Check | Result |
| --- | --- |
| Cached vs freshly searched pins, four internal rates, two RC values, control steps and idle evolution | maximum 3.3957e-10 V; limit 2e-6 V |
| Event-time difference | maximum 2.3870e-15 s; limit 1e-9 s |
| Restoration of an earlier full/idle snapshot | exact output agreement |
| Searches per 8192 steady samples, 261.6 Hz at 96 kHz | 307 (0.037476/sample); limit 0.25/sample |
| Same fixture with cache disabled | 8499 (1.037476/sample) |
| Existing reconstructed-oscillator gate | pass |
| Existing oscillator promotion/alias gate | pass |

Mutation checks used temporary header copies; no mutation is shipped:

| Mutation | Detection |
| --- | --- |
| Search every sample | efficiency assertion fails (1.037476 > 0.25) |
| Retain cache after threshold changes | event mismatch and 3.235 V output error |
| Retain cache after frequency changes | event mismatch, 4.151 V output error and 7.497 ns timing error |
| Retain future events after restoring a past snapshot | 0.07263 V output error |

Whole-engine minimum-of-three benchmarks and all three immutable compatibility
checks are recorded by the integration in `STATUS.md`. Reduced search count is
an operation-count result; it is not a claim that the whole-engine CPU target
has been reached.
