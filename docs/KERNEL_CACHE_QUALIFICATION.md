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

## Nodal Jacobian cache

Only the input-independent terms of the scalar BP Jacobian are cached when
cutoff, Q or sample rate changes. With `g = gmPart`, `f = ffPart` and
`c1 = 2 (xFromBp + 1)`, the derivative is

```
J = 1 + ((g + f) - g t1^2) ((c1 + g + f) - g t2^2)
```

The two parenthesized constant sums and `c1` are cached; the OTA-dependent
terms remain current. The same four-step Newton cap and 1e-7 residual stopping
rule are preserved. A converged initial guess still skips all Newton work.
Reassociation of a few double operations can change circuit-engine bytes, so
the integration treats this as a separate V2 numerical checkpoint.

A more aggressive previous-sample inverse-Jacobian predictor was **rejected**.
At the original residual stopping threshold, its accumulated continuation
error reached 1.13e-4 against the registered 2e-6 limit. Tightening its internal
solve to 1e-9 brought that check within the limit, but the independent nodal
hard-noise cascade growth assertion failed. It also cost 1.08-2.05 times the
original filter time on a small static sine probe. Neither a tolerance nor an
existing assertion was changed to admit that variant.

The accepted cached-term implementation passes the existing nodal gate and
its new continuation gate. On the declared cache fixture the forced-rebuild
reference is exactly equal, the largest independently reconstructed scalar
residual is 9.99993e-8, and 865112 of 1966080 sample starts skip Newton.
A separate filter-only probe (32 stages, 96 kHz, Q 5, 0.5-unit sine) gave
minimum-of-three CPU times about 4-6% lower at 1/12/20 kHz; the 250 Hz case
was within timing noise. The whole-engine benchmark, not this microprobe,
determines whether the CPU target is met.

Mutation checks for the new gate:

| Mutation | Detection |
| --- | --- |
| Wrong cached HP/BP node derivative | reconstructed scalar residual 56.19 |
| Rebuild coefficient/Jacobian terms every sample | revision-count assertion |
| Continue Newton after convergence | converged-start and zero-equilibrium work assertions |
| Reverse the nonlinear Jacobian correction sign | reconstructed scalar residual 0.01807 |
