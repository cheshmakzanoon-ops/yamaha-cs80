# Envelope/VCA endpoint-clock diagnostic

`scripts/compare_envelope_endpoint_clock.py` is a fixed, descriptive offline
comparison of the [event-aware sine reference](EVENT_AWARE_ENVELOPE_SINE_VCA.md)
with the existing native [PWL VCA endpoint reference](VCA_REFERENCE.md).
It changes neither implementation, production code nor contracts. Python 3.11+
and an independently built `cs80_vca_reference` executable are required.

## Fixed ordinary experiment

Three clocks, 8,000 / 16,000 / 32,000 Hz, cover exactly 0.02 seconds with
161 / 321 / 641 endpoints. ADSR is .004 / .03 / .4 / .02 seconds/unitless;
sine amplitude .4, frequency 440 Hz, initial phase .125 cycles; LC .8, EI -.5
octaves, initial LI .1 and initial coupling zero. Events are note-on at .00037,
note-off at .00711 and retrigger at .01173 seconds. These are ordinary explicit
model inputs, not a calibrated hardware recording or a production patch.

For each rate a fresh analytic bundle supplies its exact endpoint sine input and
envelope. Those values, with constant LC/EI, become the native CSV forcing. The
native solver interprets each interval as piecewise-linear input/envelope;
it does not receive the original subsample event times or continuous sine law.
Its internal coarse/fine RK4 trajectories use the **same** forcing knots.
`--steps 8` produces coarse 8/fine 16 substeps per interval. The native absolute
h/h2 guard is `1e-7` engine units; the exported trace is the fine trajectory.
The diagnostic explicitly passes initial LI .1, verifies zero initial coupling
and no time-zero advance. Native `--compare-realtime` accepts only `true`, so
it is omitted; the emitted manifest must confirm `compare_realtime: false`.
No engine comparison or WAV is requested.

## What the numbers mean

Two measurements stay separate:

1. Native `max_abs_h_h2_difference` is copied verbatim for LI, coupling and
   output. It measures numerical refinement of a fixed PWL forcing trajectory,
   not interpolation error, a rigorous error bound or hardware acceptance.
2. Native fine minus analytic values are compared at all 161 common exact
   `i/8000` endpoints using integer sample strides 1/2/4, not nearest-time
   searches. The differences combine forcing interpolation, event timing,
   residual solver error and small arithmetic-order rounding differences.
   They are **not** asserted to be pure interpolation error.

Common-point max absolute and RMS deltas are descriptive. No monotonic
improvement with rate, global maximum-error qualification, hardware parity or
production equivalence is claimed. Analytic shared-point identity is checked
exactly; native clocks, columns, units, constants and sampled forcing are
verified before differencing.

Boundary metadata deduplicates actual event and autonomous envelope boundaries.
`t0` is its own bucket. A common point t is `boundary_near` when a boundary is
in **(t-1/8000,t]**; otherwise it is `smooth`. These fixed common-clock buckets
are descriptive labels, not causal attribution or a promise that earlier event
influence has vanished. Metrics for `all` include every endpoint regardless of
bucket. Buckets do not establish accuracy away from boundaries.

## Invocation and retained evidence

```sh
python3 scripts/compare_envelope_endpoint_clock.py \
  --vca build-native/cs80_vca_reference --out NEW_CLOCK_DIAGNOSTIC
```

The two options are required; the destination must be new with an existing
parent. The trajectory is intentionally not configurable. The executable is
read once, copied into the private bundle and hashed before execution. Embedded
native source identities must match retained source snapshots. Snapshot identity
is not authentication or proof that arbitrary executable bytes were actually
built from those source bytes. Build provenance must be retained separately.

The bundle retains:

- `analytic_RATE/`: full fresh analytic exports and audits.
- `inputs/native_RATE.csv`, `native_RATE/`: exact sampled forcing and complete
  native stimulus, fine trace and manifest including h/h2 diagnostics.
- `native_RATE_execution.json`: command, exit code and capped stderr tail.
- `events.csv`, `tools/vca`, `sources/`: executable and source/document identities.
- `common.csv`: 483 pointwise rows (3 rates × 161 endpoints), explicit indices,
  bucket, analytic/native LI/coupling/output and signed deltas.
- `report.json`: descriptive metrics, exact boundaries and interpretation.
- `manifest.json`: successful completion, commands, budgets and SHA-256/byte
  inventory of all preceding artifacts, written last.

Exactly three native processes are planned, each with a 120-second timeout,
26,880 aggregate native RK4 steps, at most 128 filesystem entries and 128 MiB
retained bytes; the copied executable is bounded to 32 MiB. Native and analytic
input/output bounds also apply. No automatic retries or parameter relaxation
occur. A failure retains useful partial artifacts plus `failure.json`, without
a complete top-level manifest; nonzero native results are recorded rather than
silently retried. This explicit retention differs from transactional exporters.

## Qualification scope

Independent valid-input tests should consume one retained bundle, recompute its
common-point comparisons and provenance, and compare selected native PWL states
to an independent quadrature oracle. They should not rerun the experiment merely
to populate tests. Implementation alone is not a passing result; exact runtime,
source and binary results belong in retained verification evidence. No broad,
malformed-input, fuzz, GUI, CPU/benchmark or hardware qualification is implied.

## Verified local result

The existing core-only `cs80_vca_reference` target built successfully. One
bounded diagnostic run completed all three native invocations, and one direct
run of six independent retained-bundle tests passed. All 161 common endpoints
per rate were checked; the analytic values agree exactly across the three grids.
An independent exact-LI/Simpson-integrating-factor oracle checked the actual
native PWL forcing with 32/64 quadrature subdivisions, separately from the
analytic-event-aware comparison. No outcomes were retried or discarded.

Observed maximum absolute native-fine-minus-analytic output differences were
1.0293351167e-5 at 8 kHz, 2.6740879330e-6 at 16 kHz and 6.6911675920e-7 at 32 kHz.
The distinct same-forcing native h/h2 output differences were 7.8265630088e-11,
4.8307885470e-12 and 3.0006552798e-13 respectively. These are measurements of this
one trajectory, not a blanket monotonic-convergence assertion, certified error
bound, pure interpolation-error measurement or hardware/production claim.

Python 3.12.14 and Debian GCC 14.2.0 were used. Only the existing native reference
target was built; no broad suite, CTest suite or production plugin was run.
Existing production sources, exporters and all 20 contracts remain unchanged.

[Retained verification evidence](experiments/envelope-endpoint-clock/verification.json)
contains complete numerical outputs and source/compiler/build provenance. The
committed numerical subset explicitly omits the native executable and records
its hash; it is **not a complete replay bundle**. The original full bundle remains
in the working environment, and the subset manifest records its original
manifest hash and the omitted binary's identity.
