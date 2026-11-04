# Exact FIR work elimination after complete zero history

Date: 2026-11-01. Published implementation: `23a7b8df5c9f118d0b625f1d4bd37e12fa3c6162` (local implementation commit `c894a0ca2417f44329d54ea9388304ab04200d76`).

The reconstruction FIR now skips its convolution only when its complete retained history proves that the original ordered sum returns positive zero. This is a numerical implementation optimization, not a new CS-80 circuit law or a hardware measurement. All coefficients, input history, retained output phases, and nonzero arithmetic stay unchanged. There is no amplitude threshold or fade.

## Admission and continuation

`OversamplingDecimator::push()` maintains an exact count of samples with nonzero IEEE magnitude bits in the entire live ring. Both signs of zero count as zero; subnormals, infinities and NaNs count as nonzero. Integer bits are observed through a local volatile value so a compiler cannot replace the classification with a DAZ-sensitive floating-point comparison. Reset, preparation, copies and assignment retain their existing state semantics.

A shortcut additionally requires a positive-zero center sample and contemporaneous nearest rounding. The standard rounding query and, on x86, independently selectable MXCSR rounding bits must both admit it. Every other state executes the original center-first symmetric convolution. A negative-zero center can produce an all-negative-zero ordered sum; downward rounding can turn an initially positive-zero sum negative. Both counterexamples are explicit regression fixtures. Production code only reads the environment and preserves caller controls and sticky exceptions.

The historic `Decimator` implementation is unchanged. Engine uses `OversamplingDecimator` in both render policies, so exact zero-history work elimination may improve Legacy idle CPU as well as circuit idle CPU. Legacy audio remains subject to its two byte comparisons.

## Prospective acceptance and source identity

The acceptance and directed-rounding extension were published in `156d82e9f3ee20089bb952ba853e0f0c779b903f`, before candidate evaluation. `tests/cpu/zero-fir-history.json` remains byte-identical to that registration (SHA-256 `e03a258785d23ac6135cdf932034ed7b88fc744aeb5d9a0abb3f5118ff604997`).

The frozen unmodified `Dsp.h` positive control has SHA-256 `d704003dcc4ca262e31bc98a2df0494e7b3da97f747c892644264efd5f3a4347`. The qualified and published candidate has SHA-256 `1edc968eb4a297f29cb78f1301f253554fea7a01362bc71bd074598f90df71f5`. The directed-rounding driver passed against the unmodified source before evaluation of the shortcut. Its chronological oracle retains the original summation order with independent history indexing.

## Component and integration evidence

All output comparisons retain the registered zero-ULP requirement, including signed zero. Fixtures cover default unprepared state, factors 2/4/8, every ring position, complete impulse drains, random bursts, positive and negative subnormals, reset, copy, assignment, invalid-factor fallback, all four standard rounding directions, all four x86 FTZ/DAZ combinations, and changes of rounding or denormal mode on a retained history.

| Local qualification | Checks | Failures |
| --- | ---: | ---: |
| Unmodified rounding positive control | 3,276,237 | 0 |
| Candidate and restored rounding driver, each | 3,276,237 | 0 |
| Candidate and restored history driver, each | 227,117 | 0 |
| CMake Release rounding driver | 3,276,237 | 0 |
| CMake Release history driver | 227,117 | 0 |
| Existing oversampling design, latency and allocation gate | 85 | 0 |
| Existing core regression executable | 11,512 | 0 |

The rounding runs record 16,352 original downward negative-zero witnesses and three independently changed MXCSR directions. The two new test targets are registered in CMake; only the rounding test receives `-frounding-math` or `/fp:strict`. Production floating-point compilation options are unchanged.

Actual-source mutants are compiled and executed in isolation. Every mutant exits normally through the intended assertion failure; build errors, crashes and timeouts are not accepted as mutation kills. Restored positives pass after mutation evaluation.

| Actual-source mutation | Failed assertions |
| --- | ---: |
| Treat nonzero history as silent | 33,506 |
| Omit history-count decrement | 84,694 |
| Omit history-count reset | 108,104 |
| Declare silence before the full history drains | 1,892 |
| Discard the ordered negative-zero result | 3,608 |
| Classify history through a DAZ-sensitive float comparison | 44,316 |
| Omit both rounding checks | 47,976 |
| Omit independent MXCSR admission | 23,984 |
| Cache rounding admission at preparation | 8 |
| Admit downward rounding as nearest | 47,976 |

The [history manifest](component/history/qualification.json) and [rounding manifest](component/rounding/qualification.json) accompany every build/run log and exact mutation patch under `component/`. Unified diffs are gzip-compressed without changing their raw bytes; `sha256.json` records their uncompressed hashes as well as stored-file hashes. The reproducible runners are `tests/cpu/run-zero-fir-mutations.py` and `tests/cpu/run-zero-fir-rounding-mutations.py`.

`scripts/check-compat-gates.sh --strict-v2` exited zero using `/tmp/cs80-compat`. Legacy default and tails are byte-identical to `d56911a`; V1 circuit numerics are byte-identical to `22d6a32`; V2 circuit-engine audio is byte-identical to `1f842b9`. [The whole-engine manifest](whole-engine/qualification.json) records all eight capture sizes and hashes, and the complete logs accompany it. No V2 audio re-pin was made for this exact optimization.

## Benchmark method

Three complete stock rounds were rendered before the production edit from pristine published `475063ca4b5640c1b17cb45eb89a23706ad18873`. Three after rounds use a clean checkout of published `23a7b8df5c9f118d0b625f1d4bd37e12fa3c6162`. Every round uses `--rate 48000 --block 256 --callbacks 1024`, with all 27 cases and 51 rows. No quick subset is used and no completed round is discarded.

The benchmark source is unchanged. The normalized compile commands for the benchmark and both core translation units are identical, using GCC 13.3 Release. The core source difference is this FIR change plus a test-only Engine friend declaration with no runtime or object-layout effect. `after/production-diff.patch.gz` retains the exact difference.

Results select the minimum of the three callback medians per fixture; p99 and budget misses come from that same selected round. All raw medians and rows remain available. Other agents paused builds, tests and profilers for the timing windows, but the managed executor still had unrelated background activity and substantial scheduler variation. These local timings are not dedicated-machine or DAW qualification. The separate immutable-checkpoint GitHub serial workflow is the preferred source of a same-runner comparison; its outcome is reported independently.

| Local stock benchmark, minimum of three medians | Before `475063c` | After `23a7b8d` |
| --- | ---: | ---: |
| Circuit held, 11 tone pairs (RTF) | 0.632455–0.957676 | 0.626982–0.908651 |
| Circuit held p99 from selected rounds (RTF) | 1.078864–1.997518 | 0.981997–1.581012 |
| Circuit held budget misses / 11,264 selected callbacks | 974 | 745 |
| Circuit quiet idle (RTF) | 0.070246875 | 0.060575438 |
| Circuit moving controls (RTF) | 0.976010812 | 0.911899687 |
| Circuit moving controls p99 (RTF) | 1.805770312 | 2.153501813 |
| Circuit moving-control budget misses / 1,024 selected callbacks | 444 | 274 |
| Legacy held, 11 tone pairs (RTF) | 0.255702–0.305002 | 0.247639–0.300874 |
| Legacy quiet idle (RTF) | 0.021546375 | 0.012639187 |
| Nonfinite samples in all three rounds | 0 | 0 |

All quiet-idle round medians are retained: Before: round-1 = 0.072734063, round-2 = 0.147539813, round-3 = 0.070246875 RTF. After: round-1 = 0.113987625, round-2 = 0.060575438, round-3 = 0.062563500 RTF.

The selected quiet-idle median falls by **13.77%**, from 7.02% to 6.06% of realtime on this shared executor. This is an observed local result, not a confidence interval or a dedicated-machine guarantee. The moving-control p99 worsens despite its lower selected median, demonstrating why these data do not establish a held-performance improvement. Neither requested circuit CPU target passes.

All 306 rows have exactly equal non-timing fields for their corresponding fixture across all six rounds, including checksum, peak, RMS, tail completion, voice ownership and event counters. [The comparison manifest](comparison.json) enumerates the compared fields and the timing fields excluded from that exact comparison. This supplemental CSV check does not replace the four raw-byte compatibility gates.

## Scope of completion

This implements and exactly qualifies one additional idle optimization. It does not establish the requested 0.15–0.20 realtime held target or the 2% idle target. The earlier whole-core instruction profile identifies a substantial quiet-idle FIR share, but instruction shares are not native timing percentages. Broader oscillator/filter/rank work and native platform qualification remain separate requirements. No seeded stochastic circuit law was changed for this optimization.
