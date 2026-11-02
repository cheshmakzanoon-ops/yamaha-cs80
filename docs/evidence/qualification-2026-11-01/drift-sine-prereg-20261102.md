# Preregistered original Engine drift/sine oracle and uncontended CPU baseline

2026-11-02 · **TEST ONLY** · production `src` tree unchanged at
`7dd159d491012fd47b46c0aa651e86ea959ca289` · CPU targets **RED**

## Frozen original and concrete candidate

At the source `src/core/Engine.cpp` around 2197, `tickRanks` computes one
`global.drift * std::sin(kTwoPi * (driftPhase + voiceDrift[vi])) * 0.9f`
per advanced voice each internal sample. It then discards that result for
sourcePerformance's thermal model0 and instead uses the tolerance-card term.
The baseline hard `--suite baseline` isolates PURE CS-80 with `parityMode=1`:
`Engine::buildSettings` **forces effective global.drift=+0.0f**, yet still
calls the sine. A thermal-only skip cannot save a single trig call in the
hard isolation workload; zero-drift might, **only if its original signed-zero,
FP exception and complete downstream continuation are preserved**.

Before any source alteration, we froze the exact original two expressions
in `tests/fixtures/drift_sine/OriginalDrift.h` under an independent namespace,
with no call into a candidate helper. Original `Engine.cpp` Git blob
`5e7271e9c545149d450ec348049cdddc9ba2d639`, SHA-256
`a874c328d623bda38f4adffea59ac442ca721c46465f829db1c68ef20fd52a28`.
Frozen header SHA-256
`77b5d26b69460ed91263c397902957e4aed8f5fc7d5a5b0712e77af934ef1b17`.
Neither a future shortcut nor a new hardware-law inference may rewrite that
original reference or re-pin Legacy/V1/current V2 captures.

## Before-source-edit oracle, positive and negative controls

The independent frozen literal and the separately compiled original-source
scalar positive control match **1,638,423 exact bit and FP checks**, **zero
discrepancies**. Eight voice offsets use `(float) i * 0.73f`; 256 phase
positions include exact zero and nearby half-cycle boundaries; five drifts
cover `+0`, `-0`, `+0.25`, `-0.25` and float subnormal minimum. Four IEEE
rounding choices × four supported x86 MXCSR FTZ/DAZ choices yield **16 FP
control cases**. This includes **163,840 sine pairs**, **94,957
signed-zero outputs** and **491,520 thermal/worn-selection pairs**, with
each caller's FP control and sticky exception bits checked.

The **real original `cs80::Engine`** independently renders 24,576 frames per
policy, with eight simultaneous held notes, release, quiet and eight-note
wake. Its complete interleaved stereo float output and 256-frame explicit
voice, rank, Q/readout, coupling-tail and sub-state checkpoints match
bit-for-bit across host partitions **1 / 17 / 256**. Captured unoptimized
Linux x86-64 GCC13.3/glibc2.39 FNV64 regression fingerprints:

| Original-source policy | Audio FNV64 | Public-state FNV64 |
| --- | --- | --- |
| PURE isolation, forced drift0 | `ece24f4e0b81d431` | `cbe3430e20a0cf6b` |
| sourcePerformance thermal, drift0 | `ece24f4e0b81d431` | `cbe3430e20a0cf6b` |
| sourcePerformance thermal, drift +0.25 | `945bd7421116ecbd` | `710e1db62e155520` |
| sourcePerformance worn sine, drift +0.25 | `44cfefe44b55c0c1` | `6adbecc0c679115a` |
| Legacy original sine, drift +0.25 | `f5574236042a1539` | `ace3f1e915a0094d` |

Those whole-engine golden hashes are asserted **only** on the matching
compiler/glibc host. Elsewhere, the frozen direct arithmetic and cross-block
partitions still run; an independently qualified original engine capture
must precede a modified-source comparison. FNV64 is a regression sentinel,
not mathematical proof of byte identity; the strict V2 audio files are the
compatibility gate.

The registered standalone `cs80_drift_sine_exact_oracle_tests` passed via
CTest **1/1**, 7.00 s. All six deliberately wrong `-O3 -DNDEBUG
-frounding-math` test variants compiled and then failed normally with
**exit code 1** and the expected named first assertion:

| Mutation | First named failure |
| --- | --- |
| Flip exact positive-zero output sign | `drift.signedZeroAndSineBits`, `0x80000000` vs `0x00000000` |
| Naively omit sine for `drift==0` | `drift.signedZeroAndSineBits`, `0x00000000` vs `0x80000000` |
| Read another voice's phase | `drift.signedZeroAndSineBits` |
| Clear caller's FE_INEXACT | `drift.fpStickyAndExceptions`, 0 vs 32 |
| Contaminate thermal pitch branch | `drift.thermalOrWornTermBits` |
| Corrupt all three engine partitions alike | `engine.frozenAudioBits` (independent original hash catches self-parity blind spot) |

This test-only prereg has **zero real optimized-source branch calls**; the
candidate source has not been edited, built, benchmarked or promoted. Actual
source mutations and real source-site work instrumentation are mandatory
*after* coding, separately from these six controls.

## Three original-source benchmark rounds, entirely before source editing

Parent-coordinated exclusive local benchmark window: **02:18:32–02:24:35
UTC**, Xeon Platinum 8573C / KVM / 9 vCPU, GCC 13.3.0, unchanged
`cs80_source_factory_benchmark` binary SHA-256
`3fad41e7ff9659e7dcc4c1a73ce771892de74dc7386eea24d8185635236808a5`.
The three serial `--suite baseline --rate 48000 --block 256 --callbacks 1024`
effective runs used the original source tree, common fingerprint
`a95212770767733c`, 1s warmup, thread CPU clock and exactly **51 rows each**;
`completed=true` on all. All non-timing checksums, peaks, RMS, event and
voice/tail fields matched by fixture across rounds. The nine raw
CSV/metadata/completed records are under
`artifacts/cpu-drift-sine-before-20261102-r{1,2,3}/`.

| Metric (all outliers retained) | Round 1 | Round 2 | Round 3 |
| --- | ---: | ---: | ---: |
| Source eight-held RTF, median across 11 preset-pair fixture medians | 0.8582895 | 0.75276375 | 0.72935325 |
| Quiet-idle source median RTF | 0.069322125 | 0.059082375 | 0.0622126875 |
| Source held deadline misses / 11,264 callbacks | 3,142 | 2,368 | 1,368 |
| Worst source held p99 RTF | 36.877030125 | 5.0716125 | 3.3243298125 |

The three-round median of each preset's 11 source held RTF medians, then
median across the 11 presets, is **0.730701**; quiet-idle three-round
median is **0.0622126875**. The benchmark's noisy outliers and host
contention sensitivity are not trimmed. The required absolute held RTF
**0.15–0.20** and idle **≤0.02** both remain **far from passing**. No after
measurement or speedup claim exists yet.

## No production publication without absolute and exact gates

Any experiment must first prove actual `tickRanks` sine calls were eliminated
at the original source site using separately compiled test-only source
instrumentation and binary assembly evidence, not synthetic wrapper counts.
Require source-performance thermal and hard-isolation zero-drift work
witnesses with unchanged eight-voice clock and visible state, preserving
all FP sticky/control bits, independent golden engine state, moving control,
note release and wake, strict Legacy/tails, V1 and V2 **byte** captures,
and all physical/source/idle/click/full CTest gates. A source-only shortcut
which discards the original signed zero or FE_INEXACT flags is not yet safe.

Only after those pass may a separately granted same-host quiet window run
**three uncontended after rounds** of the unchanged complete 51-row
benchmark, with p99, misses, moving/idle/Legacy and every outlier retained.
Failing either absolute CPU target means **reject the candidate**, keep it
on an isolated experiment branch and do not update PR6/main, pin, release
or hardware-identification constants. This test-only commit is safe to
review independently of the optimization.
