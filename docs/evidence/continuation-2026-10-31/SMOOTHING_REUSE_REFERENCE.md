# Smoothing reuse: independently settled reference

2026-10-31. This is a test-fixture correction, not a production audio change.
The contract was published as
`794c339df29d2dbb159b5112b216a72f34a638eb` in
[smoothing-reuse-settled-reference.json](../../../tests/parity/tolerances-v2/smoothing-reuse-settled-reference.json)
before implementation or evaluation. No hardware measurements are claimed.

## Original failure and causal control

The full smoothing suite reported **84 checks, one failure**:
`reused circuit starts at its own zero touch`. A temporary diagnostic selected
the unchanged `testStaticAndSnap` function and printed its physical state. It
was compiled against the cached `780658d` headers and core library, then against
the current cold-timing repair (`46a7a5a`) headers and library. Both runs produced
identical diagnostic logs and the same failed assertion.

| Observation | Before cold-timing repair | After cold-timing repair |
| --- | ---: | ---: |
| Active voices after reassignment | 1 | 1 |
| Pressed LP cutoff | 6748.456543 Hz | 6748.456543 Hz |
| Reused fresh LP cutoff | 1275.520142 Hz | 1275.520142 Hz |
| Cold reference LP cutoff | 3604.039063 Hz | 3604.039063 Hz |
| Original relative cutoff error | 1.825544715 | 1.825544715 |
| Reused filter-envelope state after one host frame | -1.496911168 | -1.496911168 |
| Cold reference filter-envelope state after one host frame | 0.001617962 | 0.001617962 |
| Reused TWS a after reassignment | 6.97896e-8 | 6.97896e-8 |

The reused filter capacitor had settled to IL = -1.5; the never-rendered
reference still began with zero startup charge. Assignment now preserves
both states, as required by the earlier
[charge-continuity repair](../../COLD_FILTER_ENVELOPE.md). The test compared
different envelope histories. Its active-count and pressed-cutoff predicates
already passed, and pressure on the new note was zero. This is an existing
fixture assumption exposed by intentional capacitor continuity, not a new
touch regression caused by the cold oscillator timing repair.

## Correction and retained limits

The independent reference now renders **100 blocks of 512 silent host frames**
before its first note, using the same fixed settling duration already present
on the reused side. It receives no earlier note or pressure, and no state is
copied between engines. Each rank's expected IL is calculated independently
as `-3 * clamp(CP_fIl, 0, 1)`, the existing declared physical-PANEL coordinate.
This interface scale is not a measured circuit voltage.

The original eight-note schedule, pressure excitation, release loop, reused
settling, one-host-frame comparison, failure label and **all 84 checks** are
retained. The original inequalities remain exactly:

```cpp
e.activeVoices() == 1
pressed > 1.5f * fresh
abs(fresh - reference.lpfHz) <= 1.0e-3f * fresh
```

The same assertion also requires exact pre-note IL, zero pre-note active
voices, matching post-note envelope charge, one active reference voice and
zero reference TWS a. With zero new pressure, reused TWS a must remain
nonnegative and must not increase from its pre-note residual. This last
condition follows the TWS17 100 kOhm / 1 uF follower law documented in
`TouchCircuit.h` and
[the keyboard workstream](../../workstreams/ws3-keyboard.md).

After correction both fresh cutoffs are **1275.5201416 Hz**, with **zero**
relative error. Both ranks begin at exactly -1.5. Reused TWS a decays from
`6.98041375813e-8` to `6.97895998769e-8`; the reference stays at zero. The full
suite reports **84 checks, zero failures**.

## Mutation qualification

Both registered mutations were applied only to temporary copies. No tracked
core file, production behavior or numerical tolerance changed.

| Mutation | Observed mechanism | Full-suite result |
| --- | --- | --- |
| Preserve the old per-voice pressure smoother on reassignment, retaining its initial snap and all bend/timbre logic | TWS a rises from 6.98e-8 to 0.0002077316; the new no-growth boundary rejects the stale controller drive | 84 checks, one failure; exit 1 |
| Remove only the independent reference warm-up | Reference charge remains zero instead of IL; original relative cutoff error returns to 1.825544715 | 84 checks, one failure; exit 1 |
| Restored implementation and corrected fixture | Matching charge history and decaying residual touch | 84 checks, zero failures; exit 0 |

The pressure mutation's cutoff error is only **0.0002877899** on the first
host frame, below the unchanged 0.001 limit because the physical follower
attenuates that first stale drive. Checking the restored MIDI pressure target
would also miss the defect: `endSmoothedControls()` restores that target after
each internal sample. The preregistered TWS no-growth boundary makes the gate
sensitive to the actual fault without tightening or weakening its older
cutoff tolerance.

## Evidence and reproduction

[smoothing-reuse-reference/](smoothing-reuse-reference/) contains the original
full-suite failure, identical before/after diagnostic logs and source, the
corrected and restored full-suite logs, both mutation patches/logs, mutation
results and exact source/library SHA-256 provenance. The diagnostic-only
temporary main runs four existing checks; all qualification and mutation runs
execute the complete 84-check suite.

The corrected suite links the existing Release core library; the pressure
mutation compiles a temporary `Engine.cpp` object against the same headers
and links it before that library. The reference-settling mutation changes
only a temporary copy of the test. Reproduction uses C++20, `-O3 -DNDEBUG`,
the repository `src` and `tests` include paths, and the two archived patches.
The ordinary CMake target remains `cs80_smoothing_tests`; no test is skipped,
removed or replaced.
