# Native eight-stage filter average-cost result

**NO ADMISSION. Calibration failed, and the candidate did not meet the prespecified improvement gate.** This completed new real-VST3 study does not establish a speedup or a reliable percentage regression. All unfavorable observations and all 17 failed gates are retained.

The one-shot run completed **96 of 96 processes**, in **24 four-treatment blocks across six independently randomized Williams sets**. Collection used 162.312870 CPU seconds and 163.586307299 wall seconds, within its fixed 240 CPU / 360 wall ceilings. Every process passed the frozen image, callback-count, PCM/FP/public-state, affinity, raw-clock and external-envelope integrity checks. There were no retries, replacements, exclusions, selective early stops or post-outcome rule changes.

## Observed result

Percentages below use the frozen mean-log estimators. A positive candidate saving favors C; its negative point estimate is unfavorable, not an admitted regression estimate.

| Contrast | Point | 24-block bootstrap 95% interval | Mandatory six-set t 95% interval |
|---|---:|---:|---:|
| Same-binary placebo S1 / S0 | +0.539% | −3.844% to +5.329% | −6.820% to +8.479% |
| Genuine extra callbacks S+ / pooled stock | +7.475% | +4.329% to +10.614% | +3.255% to +11.868% |
| C saving versus pooled stock twins | −2.283% | −4.735% to +0.363% | −4.238% to −0.366% |
| C saving versus S0 | −2.559% | −5.427% to +0.722% | −6.667% to +1.391% |
| C saving versus S1 | −2.009% | −5.098% to +2.174% | −6.749% to +2.521% |

The placebo intervals do not fit inside the required ±2%. The positive-control intervals do not fit wholly inside +3% to +10%. All three candidate lower bounds fail the required improvement greater than 3%. Main gates had to pass under **both** interval methods.

Both temporal-half candidate lower bounds also fail positivity. Candidate point savings by its four scheduled positions are −4.309%, −5.376%, −3.465% and +3.758%; their 9.134-point range fails the 5-point ceiling. The favorable fourth-position subset is not an alternative result. The S1-before-S0 placebo point is +3.004%, outside ±2%. The temporal-half point gap and opposite placebo order pass; they do not rescue the failed mandatory gates. `official-decision.json` includes every point, interval, gate, failure, set mean, leave-one-set descriptive result and process total.

For absolute scale only, the 24 process-total CPU observations per treatment have these descriptive ranges/medians. These are not additional admission estimators:

| Treatment | Genuine measured callbacks | Minimum / median / maximum process CPU total |
|---|---:|---:|
| S0, stock | 512 | 428.204 / 447.493 / 622.739 ms |
| S1, same stock binary | 512 | 430.993 / 450.382 / 685.908 ms |
| C, normal V2 | 512 | 445.592 / 462.337 / 531.344 ms |
| S+, same stock binary | 544 | 465.912 / 481.306 / 602.392 ms |

No outlier is removed. Callback-level rows are integrity inputs, **not independent statistical replicates**. Neither the work-count reduction nor this failed calibration supports a CPU, energy, DAW deadline or tail-latency claim.

## Actual workload and controls

The genuine public `IAudioProcessor::process()` host loads the verified release modules. S0, S1 and S+ are labels for the **same physical stock module**, not separate rebuilt stock images. C is the hook-free eight-stage V2 hybrid release-module prototype; it is not a clean full product rebuild. The actual measurement host sources and original collection/analyzer sources are included.

Each fresh process receives the same eight factory-held notes, both factory-selected ranks, 48 kHz, 64-frame blocks and 1,024 genuine warm callbacks. Ordinary arms then execute 512 callbacks; S+ executes those same calls followed by 32 additional callbacks on the same held instance, then each run has one untimed continuation. The zero-clock admission proves the complete ordinary 1,537-call lifetime is byte-exactly recoverable as the longer arm's prefix. Real extra work is not a synthetic delay or a fake denominator.

Every measured callback records monotonic start, thread-CPU start, the actual plugin call, thread-CPU end, and monotonic end. Allocation, buffer preparation, correctness checks, hashing, serialization and state observation occur outside those brackets. CPU intervals are summed to **one process-total observation**. All totals use a 512-callback baseline, including S+; dividing S+ by 544 would erase the positive control and is forbidden. Clock/call overhead is not subtracted. Per-callback clock wrappers are not a claim of zero observer overhead.

All processes use CPU 0, the exact admitted loader library prefix, C locale, fixed PATH, and a fresh initially empty HOME. The source collector preserves every attempted process. Its authorization binds the frozen plan; a permanent consumed latch prevents restart. Physical module path/stat, actual loaded image, full immutable PCM/FP/state/control artifacts, complete clock rows and external process resource bounds were validated before the next slot. No comparative effects were calculated during collection.

Prior zero-clock native evidence establishes actual packed eight-wide execution with normal/reporting-module parity and the required AVX2/AVX512F/VL/DQ plus OS ZMM state. The timed host has **no private backend or voice counter**. Thus that evidence is an external untimed prerequisite, not an invented per-measurement hit count.

## Prespecified randomization and statistics

`PROTOCOL-frozen.md` is the exact premeasurement procedure. `schedule.json`, `schedule.py`, `statistical-plan.json` and the unmodified `study_statistics.py` reproduce its numerical rules.

Six independently randomized four-block Williams sets balance all four treatments across positions and directed distinct within-block predecessor pairs. Assignment and row order use frozen seed `cs80-eight-native-average-v1/0x80305124` and the specified SHA-256/Fisher–Yates procedure. Between-block transitions are reported, not falsely claimed balanced. The first and second 12-block halves are separately balanced.

The primary estimator is an equal mean of complete-block log contrasts. The joint 20,000-draw whole-block percentile bootstrap uses independent fixed seed `0x80305125` and group-key substreams. Mandatory sensitivity uses the six complete-set means and a df = 5 t interval. All subgroup rules were frozen beforehand. Six-set sensitivity addresses within-set correlation, not arbitrary independence across time on a shared machine. No subset, omission, alternative seed or more permissive gate may replace the full result.

## Portable evidence and normalization

- `records.jsonl.gz`: all 96 process totals, complete public host outcomes, external resources, role mappings, status, and original receipt/stdout/clock/outcome hashes.
- `callback-clocks-set-0.csv.gz` through `callback-clocks-set-5.csv.gz`: all **49,920** callback rows and their four observed endpoints, losslessly sharded by the prespecified sets. Each shard contains 8,320 callbacks. `clock-shards.json` pins the ordered concatenation; no callback is summarized away.
- `ledger.jsonl` and `terminal.json`: the complete 194-event one-shot history and completed terminal, with clock origins normalized consistently.
- `official-decision.json`: all original numerical results and failures; only process identifiers and terminal clock origin are normalized.
- `countercontrols/`: all 56 frozen statistical synthetic controls, the collector/host controls, their original test sources and the independent review. The earlier no-op synthetic fixture mistake is documented in the review; it was corrected before collection, not hidden by a timed rerun.

For privacy, original PIDs become chronological process ordinals 1–96. One common integer wall-clock origin is subtracted from all wall timestamps. Each process's first measured CPU-start integer is subtracted from all its CPU endpoints. These transformations preserve every interval, gap, cross-process wall order, sum and statistical result exactly. No floating conversion or rounding is applied to the clocks. The removed origins, raw PIDs, private filesystem paths and device/inode numbers are not published.

`provenance.json` retains the original private plan/freeze/authorization/terminal/ledger/decision hashes and the role-to-module mapping. Every normalized process links to its original private receipt and raw clock hashes. Common immutable PCM/FP/state artifact hashes are in `expected/512.json` and `expected/544.json`.

Large PCM/state captures, raw private receipts, native binaries and actual loader bindings are **not bundled**. Their hashes identify evidence that was checked during the full private replay; hashes alone do not reproduce omitted bytes, prove a physical image binding, or independently rerun the native exactness gate. Public replay independently verifies the complete normalized clock evidence and frozen statistical decision, not those omitted native artifacts.

## Offline replay

Python 3.9+ standard library only:

```sh
python3 -B replay.py
python3 -B replay.py --self-test
```

Replay checks the manifest, all six shards, exact seed/schedule, complete ledger/process/callback order, physical-role aliases, fixed endpoints, clock sums, resource bounds, all 96 totals, all group statistics and every gate. It calls the genuine frozen statistics implementation. It does not load native code, invoke a subprocess, read a clock, render audio or collect another measurement. `--self-test` additionally rejects 12 corrupted/missing/order/identity/endpoint/count cases. Exit success means **the evidence reproduces NO ADMISSION**, not that performance passed.

The original collector and private full-campaign loader are archived in `procedure/` for method audit, not as the portable entry point. Host sources in `host/` and the normalized build command/provenance document the actual code used; building it requires the separately pinned project/SDK/native inputs. No rerun occurred to create this export.
