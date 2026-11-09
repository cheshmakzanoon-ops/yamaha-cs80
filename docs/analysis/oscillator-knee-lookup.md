# Rejected exact oscillator knee lookups

**Neither candidate is shipped.** Both were locally bit-correct in the tested
compiler profiles, but neither met the predeclared dual-profile cost gate.
Shipping `src/`, registered `tests/`, `CMakeLists.txt` and screenshots are restored
exactly to `c48d0816d7dae3e130035060dc415b0a1098d811`. The earlier rejected
[prepared-value cache](oscillator-profile-cache.md) and its evidence are unchanged.
This report is not a current whole-instrument CPU scorecard.

## Selecting a bounded path

A fresh GNU gprofng run on unchanged source `35f3431` completed the existing
15-case Source Performance quick suite, but the collector reported **“Collection
interval timer period was changed (997 -> 0); profile data may be unreliable.”**
Only 0.212 seconds of samples were reported over 21.367 seconds. The retained
function table is not treated as a representative cost distribution or used to
claim a percentage saving. This is a measurement limitation, not a DSP failure.

Deterministic source-instrumented execution of two existing eight-voice fixtures
instead counted the work at issue. Both used 48 kHz / 128 frames, selector 0,
256 held callbacks and their normal warm-up/sustain/release phases:

| Fixture | Node evaluations | Old proximity comparisons | Knee adjustments |
| --- | ---: | ---: | ---: |
| Dry Source Performance | 3,103,315 | 12,413,260 | 198,808 |
| MPE + chorus/reverb | 5,216,673 | 20,866,692 | 227,204 |

These are operation counts, not sampled cost shares or timing gains. The
[instrumentation patch and driver](../experiments/oscillator-knee-lookup/profiling/)
are offline only. Their reproduction script restores the pinned source into a
new directory and changes only the original driver's scratch CSV destination.
The original absolute scratch path remains in the archived measured driver for
provenance; use the wrapper rather than executing that driver directly.

## Two exact local implementations

The baseline loop checks whether the normalized absolute shaper input is within
`2e-11` of `.25`, `.5`, `.75`, or `1`, then applies its existing signed side
adjustment. These four neighborhoods are disjoint, and an adjustment cannot
reach another neighborhood. No oscillator state, analytic segment, event search,
root solve, residual, model profile, error tolerance or public layout changed.

1. **Scalar nearest (`4fa36a8`)**: ordered coarse bounds `[.2,1.1]` precede
   `int(a*4+.5)`. Any original match selects the same integer 1..4; outside the
   coarse bounds none can match. The exact original strict comparison and
   `knot + copysign(2e-11, aSlope*double(side))` remain.
2. **Vector follow-up (`8bc5a9c`, code `3ed4971`)**: a translation unit that
   already defines `__AVX2__` compares all four original distances in parallel.
   The zero/one-bit mask selects the same dyadic knot and original assignment.
   Other builds use the scalar-nearest path. This is compile-target selection,
   **not runtime ISA dispatch** or mixed-ISA support. No new ISA requirement is
   added to generic builds.

The scalar [plan](../experiments/oscillator-knee-lookup/preregistered.md) preceded
implementation/timing. Its complete unfavorable native results were preserved
before declaring the [vector follow-up](../experiments/oscillator-knee-lookup/vector-followup-plan.md).
The original scalar snapshot and both complete matrices remain archived. No
compiler floating-point flag or acceptance threshold was relaxed.

## Correctness evidence, not performance approval

Each version passed **1,448,939 checks / 161,787 full oscillator ticks** under
GCC 14 generic and native release profiles. Independent Clang 19 generic/native
runs reproduced both results; the vector version also passed an independent
`-mavx2 -mno-fma -mno-bmi -mno-bmi2` build.

The direct DUT helper is compared against the literal old loop at every knot,
strict-tolerance neighbors, coarse guards, nearest-bucket midpoints, signed-zero
and ordinary slopes, side −1/0/+1, infinities and quiet NaNs, plus randomized
magnitudes. The retained full corpus compares every returned pin and public
continuation/residual/diagnostic/event field by bit pattern, including signed
zero, across eight rates, nine profiles, changing/invalid controls, copying,
reset/restore, idle/wake and real registry exhaustion. No padding comparison is
used. The callback/idle allocation counter observed zero allocations. Both
objects remained 5,984 bytes on this ABI.

The scalar candidate additionally passed a GCC `-O1` ASan/UBSan/
float-cast-overflow run with the same count; leak detection was disabled.
This sanitizer result is **not** transferred to the vector implementation.
These are tested fixed-FP-environment results, not all-input/all-compiler proofs
or guarantees about sticky floating-point exception flags.

## Complete cost matrices and rejection

Each implementation had a separate complete matrix: two GCC profiles, six
workloads, 16/32 oscillators, nine alternating-order pairs, or **432 rows / 216
pairs per implementation**. Generic used `-O3 -DNDEBUG`; native additionally used
`-march=native`. The identical input/warm-up and full-state observer probe ran at
96 kHz internal rate for 32,768 measured frames. Idle used 2,048 calls advancing
16 samples each. Input/setup/final hashing were outside timing; all returned
pins and complete post-state were consumed. All four probe processes reported affinity
`{8}`, with every sampled start/end CPU at 8. Across **all 864 rows / 432 pairs**,
statuses were successful and every baseline/candidate checksum matched.

Primary acceptance required the median of all 45 active 16-oscillator paired
reductions to be at least **3% in both profiles**. No individual case could have
more than 3% median paired regression. Reductions below use
`100 * (1 - candidate_ns / baseline_ns)`, not a ratio of separate medians.

| Candidate | Generic pooled active-16 | Native pooled active-16 | Decision |
| --- | ---: | ---: | --- |
| Scalar nearest | +7.187532% | −1.777077% | Reject |
| Vector follow-up | −0.097857% | +0.930174% | Reject |

Secondary failures also remain: scalar native held-high/16 was **−5.993407%**;
vector generic dense-controls/32 was **−3.113898%**, and vector native
held-middle/16 was **−6.650366%**. All per-case medians, ranges and improving-pair
counts are in the [validated summary](../experiments/oscillator-knee-lookup/evidence/summary.json).
No row or outlier was discarded.

A particularly important reliability check: **the generic executables for the
two matrices are byte-identical**, SHA-256
`9a002d94189b4713f835427e158af34904363a7a97d57df48a830fcf315a061a`.
Their pooled result nevertheless changed from +7.19% to −0.10%. That is direct
evidence that the apparent initial generic saving was not robust in these
shared-machine measurements. Affinity does not isolate scheduling, shared
resources or other run-to-run effects. These data neither prove a universal
slowdown nor support shipping a reliable speedup.

The vector plan required an additional complete 131,072-frame confirmation and
paired eight-voice Engine timing **only after** its first matrix passed. Because
it failed, those runs were not performed. Neither candidate proceeded to the
unchanged numerical/promotion/continuity suites, strict whole-Engine pinned audio
compatibility or behavioral mutation qualification. Vector sanitizer
qualification also remains unrun. Older test results do not fill these gaps.

No source optimization, whole-instrument gain, real-time deadline guarantee,
release qualification or CPU/idle target completion follows from this experiment.
The next useful CPU step should remove larger repeated work, with realistic
Engine timing rather than another arithmetic-count-only claim.

## Reproduce the archived result

```sh
docs/experiments/oscillator-knee-lookup/reproduce.sh /tmp/knee-check
# Optional new timing run, preserving all historical data:
docs/experiments/oscillator-knee-lookup/reproduce.sh /tmp/knee-timing --timing 8
python3 docs/experiments/oscillator-knee-lookup/summarize.py
# Separate structural-count reproduction, not a benchmark:
docs/experiments/oscillator-knee-lookup/profiling/reproduce.sh /tmp/knee-counts
```

The wrappers write only into new output directories. The summary validates
historical row order, workload coverage, evaluation counts, observed CPUs,
status/checksum agreement and both rejection gates. A fresh optional timing run
is separate evidence, not permission to rewrite this decision. The
[manifest](../experiments/oscillator-knee-lookup/manifest.json) pins archived
source, harnesses, raw results and reproduction tools. No archived helper or
probe enters the normal build.
