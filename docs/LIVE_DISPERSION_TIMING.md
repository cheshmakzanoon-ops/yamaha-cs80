# Live dispersion and envelope timing

2026-11-09. Software control-connectivity correction in Source Performance,
not a new component model or hardware measurement. The declared card draws,
R*C mapping, 5 ms control smoothing and envelope RC laws are unchanged.

## Defect

`Engine::refreshToleranceRatios()` follows VOICE DISPERSION at every internal
sample. Before this correction the amplitude and filter envelope timing ports
only consumed those ratios when `applyParams()` ran. Re-sending an unchanged
control frame therefore changed a continuing envelope; a single edit with no
subsequent frame could leave its old timing indefinitely.

Against `1b146c45226e14f46af9d62e6e3bd0ced4476213`, a 48 kHz Source Performance
probe with one-second attacks and dispersion 0 -> 1 produced these states
100 ms after the edit:

| Delivery of the same edit | Amplitude EG | Filter EG |
| --- | --- | --- |
| Once, followed by one 4800-frame render | 0.295887649 | -0.567953825 |
| Same values repeated every 64 frames | 0.319510639 | -0.551698208 |
| Corrected engine, either delivery | 0.319684386 | -0.551577568 |

The maximum stereo-sample difference was 0.000561603. Legacy and unchanged
dispersion controls were identical in that probe. These values describe a
software regression fixture, not audible thresholds or analog tolerances.

## Correction and scope

The ratio refresh now reports whether it changed. At the same internal sample,
the engine refreshes the live envelope timing ports, using the existing timing
equations, release floors and per-card ratios. `setTimes()` retains capacitor
value, stage and elapsed time. Filter envelopes of idle circuits remain
connected as before; inactive amplitude envelopes remain closed.

Settled ratios skip the extra work. Legacy and PURE CS-80 use identity ratios
and retain their policies. There are no parameter, state-schema, latency,
allocation or library changes. Moving dispersion in Source Performance can
intentionally change continuing attacks, decays and release tails. Use the
preceding commit for the old block-dependent behavior.

## Real-time cost

This is a correctness fix, not a CPU optimization. Although `setTimes()` only
updates stored times, the following envelope tick sees a new coefficient
argument and misses its exact-argument `ExpMemo` cache. With eight active
voices and two ranks, the amplitude and filter generators can therefore add
up to **32 `std::exp` evaluations per internal sample while dispersion moves**.
The count scales with the internal oversampling rate; the extra work stops
when the ratios settle. Idle and completed stages may require fewer calls.

An independent Linux linker-wrapped call-count probe at 48 kHz Standard,
eight voices in Attack and 128 host frames (256 internal samples) counted:

| Condition | Before correction | Corrected |
| --- | --- | --- |
| Static dispersion | 768 `exp` calls | 768 `exp` calls |
| Moving dispersion | 768 `exp` calls | 8,960 `exp` calls |

The increase is 8,192 calls, exactly 32 per internal sample in that fixture.
The existing 28,672 `pow` calls in the moving fixture were unchanged. These
are operation counts, not callback timings or a realtime CPU-budget pass.
Reducing the moving-control cost requires a separately qualified numerical
optimization; freezing the timing ports would reintroduce the defect.

## Regression evidence

`tests/ToleranceDriftTests.cpp` adds:

- Attack, Decay and Release at 44.1/48/96 kHz Standard and 48 kHz HQ.
- Two independent voice cards, both ranks and all three render policies.
- Dispersion 0 -> 0.35 -> 1 -> 0, with exact state continuity at each edit.
- Bit-identical audio and envelope state for one render versus 31-frame blocks
  that repeat the unchanged control frame.
- An independent attack recurrence from the smoothed dispersion, each card's
  R*C values and the RC charge equation. This rejects ignoring live timing as
  a false solution to the partition test.

The expanded suite passes 1,119 checks. Linking the exact same test source to
the preceding core archive fails 184 checks: 180 partition/state comparisons
and all four independent timing-law checks. These are locally executed Linux
Release results; they do not qualify other platforms or DAW automation.

The source-factory compatibility corpus remains byte-identical to that same
pre-fix revision (SHA-256
`08a381f4ac9de1709a9c9197a78baefb12ffac93cd79f76d4b1e92d4f329e195`).
A separate 27-case static-dispersion render corpus, covering three policies,
44.1/48/96 kHz and dispersion 0/0.35/1, is also byte-identical (SHA-256
`3afb89a9384a2f5e438cae177509add97cc6019186c63773bd09a0958452355e`).
These comparisons are scoped regressions, not a claim about every performance.

Run the registered regression with:

```
cmake -S . -B build-core -DCS80_BUILD_PLUGIN=OFF -DCS80_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-core --target cs80_tolerance_drift_tests
ctest --test-dir build-core -R '^cs80_tolerance_drift_tests$' --output-on-failure
```
