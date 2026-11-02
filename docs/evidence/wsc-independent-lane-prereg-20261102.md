# WSC independent-lane SIMD — frozen original-source oracle (test-only)

**2026-11-02 UTC.** This isolated branch adds tests, frozen acceptance data
and CMake registration. The production `src` tree stays exactly
`7dd159d491012fd47b46c0aa651e86ea959ca289`; it introduces **zero new
vector backend calls, zero optimization source changes and zero performance
claims**. The original WSC implementation has SHA-256
`c9bb458f238ea6cb0bf74444a1a9459441da846bbf3ab98598848b45b147034c`.
The independent `tests/reference/WscLoopOriginal.h` was already frozen from
that original and was not edited.

## Why this region, and what this cannot prove

The earlier retained Callgrind eight-held profile (2,877,600,053
instructions, 128 callbacks, 48 kHz/256) attributes **27.32%** to
`ReconstructedOscillator::tick()` and **24.89%** to the IG00156 phase
kernel (29.92% full filter). The complete rank loop is 82.53%. These
are **instruction shares in an older source profile**, not native elapsed
time shares, and must not be added across overlapping scopes. The already
tested four-lane phase SIMD experiment improved held medians
0.726036→0.573527 RTF on its separate Xeon host, yet still fails
held ≤0.20 and idle ≤0.02 and worsens moving-control p99.

An independent-lane oscillator route has a materially larger potential
work region than another small phase predictor shortcut. It also carries
nontrivial event divergence: each WSC node has its own physical reset,
capacitor trajectories, comparator/sine fold events, 16-sample residuals,
copy/restore state and diagnostic history. The existing standalone
one-segment WSC oracle sees 45,311 event-free ticks of 56,576, but **that
is not an Engine eligibility measurement**. Factory tone pairs use PWM
depth, which may change WSC thresholds every internal sample and invalidate
the cached future branch event. No speedup or SIMD utilization fraction is
inferred from the oracle's event-free count.

## Frozen test-first coverage and observed positive result

`tests/cpu/wsc-independent-lane-simd.json` declares the future WSC batch,
Engine dispatch, FP and physical-event obligations before any optimized
source is written. The independent test executable compiles both the live
header and the separate frozen original WSC namespace with identical
`-O3 -DNDEBUG -frounding-math` flags. It does not include a shared candidate
math kernel inside the reference.

The standalone positive was compiled from the **new isolated original-base
worktree** with GCC 13.3 and `-std=c++20 -pthread -Isrc -Itests`, then run:

| Original-source qualification measure | Result |
| --- | ---: |
| Distinct rate/width/FP scheduling cases | **96** |
| Internal rates (Hz) | 88,200; 96,000; 192,000; 384,000 |
| Lane widths | 1, 2, 3, 4, 7, 8, 15, 16 |
| IEEE rounding | nearest, downward, upward, toward zero |
| Supported x86 MXCSR denormal combinations | **4** |
| Actually clocked WSC lanes | **69,916** |
| Masked lane samples | **159,460** |
| Actual diagnostic events | **5,315** |
| Event-free active ticks | **64,720** |
| Independent copies / resets / restores / idle-wakes | **96 each** |
| Exact output/state/diagnostic/FP field comparisons | **178,329,376** |
| Bit mismatches | **0** |
| Production/vector backend calls | **0** |

Every sample compares all sixteen lanes, including **masked and unclocked
lanes**, the complete double-bit physical state, elapsed time, oscillator
residual cursor and all 3×16 residuals, diagnostic counters, x87 status,
MXCSR and sticky flags. All **64 event slots**, active *and inactive*,
have their times, charges, types and pre/post pin bits checked without
comparing object padding. The workload rotates lane order, changes
frequency/PWM/gates, includes stops/nonfinite/subnormal/signed-zero
controls, an exact physical sample-end boundary and adjacent ULPs,
one-sample reset plateau, copy, reset, state restore, decimated idle and
first wake. The no-FP-layout-change scalar positive passes under all
supported adversarial modes.

The fixed test grid additionally asserts the exact 8-width/4-rate
coverage, 32 full 192-sample cases, 16×available-denormal 128-sample
FP cases, per-lane active+masked ownership, all event categories and a
lower bound of **775 explicitly checked fields per lane/sample**. Simply
dropping a fixture or inactive event slot cannot silently pass.

## Negative controls, ordinary exit 1

Four deliberately wrong test-only variants were compiled with the same
GCC13.3 flags and ran to normal named failures. These prove the original
oracle actually notices the injected changes; they do not constitute
mutations of a future, not-yet-existing SIMD backend.

| Variant | First named failure | Exit |
| --- | --- | ---: |
| `CS80_WSC_LANE_MUTATE_MASK` (tick a masked lane) | `lane.state.charge` (plus capacitor/cursor/readout) | 1 |
| `CS80_WSC_LANE_MUTATE_CROSS_LANE` (neighbor Hz) | `lane.state.charge` (and `output.sawPin5`) | 1 |
| `CS80_WSC_LANE_MUTATE_EVENT` (one ULP timestamp) | `events.absoluteSeconds` | 1 |
| `CS80_WSC_LANE_MUTATE_FP` (clear FE_INEXACT) | `fp.exceptions`, x87 and MXCSR | 1 |

The normal CMake target is `cs80_wsc_independent_lane_prereg_tests`.
Four optional named negative executables are behind
`CS80_BUILD_WSC_LANE_PREREG_MUTANTS=ON` and excluded from default builds.
The portable CMake/Ninja executables were unavailable after workspace
cleanup, so this follow-up used directly verified compiler invocations,
**not** a claim that the full CTest matrix has been freshly rerun.

## Future source and actual-work obligations

Only a future separately reviewed experiment may introduce real AVX2
double-lane WSC arithmetic, preserving exact per-lane argument and
operation order, `std::exp`/`std::log1p`/long-double event certification
where required, caller rounding, FP exception flags and scalar fallbacks.
Source-site work witnesses must prove actual vector *arithmetic* and lane
commits rather than unrolling or dummy load/store, and separately count
scalar event/FP fallbacks. Actual-source mutations must defeat the work
gate with ordinary exit 1. Next qualify exact Engine audio/state,
original event/RNG/mix/HP-LP order, source V1/V2 strict bytes,
oscillator alias, physical, idle/wake, click and full CTest gates.

Only after all of those pass may a parent-coordinated **uncontended
three-original-before/three-candidate-after** run of the unchanged complete
51-row stock source-factory benchmark be attempted. Retain all checksums,
p99, missed deadlines, moving/idle/Legacy controls and raw outliers;
no microbenchmark establishes a whole-engine gain. Any candidate still
above 0.20 held or 0.02 quiet-idle RTF remains **rejected for production**.
No main/PR6 edits, numeric pin/tolerance changes, push, merge, tag or
release are authorized by this preregistration.

### Remaining quiet-idle opportunity (separate from WSC)

The older idle Callgrind recording (535,065,489 instructions) attributed
43.72% *inclusive* to `tickGlobals`, including `updateSourceReadouts`
20.03%; `beginSmoothedControls` added 7.74% of total and the entire TWS
loop/wrappers 5.05%. Readout's 20.03% includes mandatory sixteen filter
envelope clocks (7.84% of total), precise elapsed-time updates and existing
memoryless-network cache savings; the shares are **not additive**. A
future exact stable-drive/controller specialization is a more defensible
idle investigation, but it must preserve every `preciseTime` addition,
event-set target, touch acquisition and electrical/output tail. The separate
Touch fixed-state preregistration already covers part of this area. No
single narrow omission is shown to close the ~3.06× idle gap.
