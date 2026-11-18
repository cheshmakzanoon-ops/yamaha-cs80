# Frozen prospective real-VST3 average-cost study

This is a new candidate and real VST3 workload, not a reanalysis, extension or retiming of failed previous controls. Only average CPU cost is in scope. No DAW deadline, tail-latency, callback-level precision or CPU instruction-count prediction is intended.

## Design and outcome unit

There are 24 complete four-treatment blocks in six independently randomized four-block Williams sets, with four fresh processes per block, 96 total. Assigned permutations are correlated within each set; arbitrary temporal independence is not asserted. Treatments S0 and S1 are blind logical aliases of the exact same verified physical stock module path and inode. C is the genuine normal eight-stage V2 hybrid release-module prototype, never a reporting build. It reuses the verified normal wrapper/shared objects and is not a clean full product rebuild. S+ loads that same stock module and performs 544 rather than 512 genuine measured callbacks (6.25% more). No auxiliary synthetic loop substitutes for these callbacks.

Each process is pinned to CPU 0 (available in the preflight affinity set); its frozen host reports/validates this. Every process uses the same public IAudioProcessor host, eight held factory notes, both factory-selected ranks, 48 kHz, 64 frames, and 1,024 complete warm callbacks. Normal arms execute 512 measured process() calls. S+ executes those same first 512 calls and 32 further calls continuing its genuine held state. Host setup, controls, event delivery, continuation, PCM/FP and public state checks are outside each timed process() interval. Recorded audio work is the sum of the process() CPU intervals. All totals are normalized to 512 baseline callbacks, including S+; never divide S+ by 544 for the positive-control contrast.

A process total is one observation. Its 512/544 calls are not independent replicates. The primary bootstrap resamples 24 complete blocks, never individual callbacks; mandatory six-set t sensitivity addresses within-set correlation. Raw process totals, counts, public endpoint hashes, all failures and actual schedule are retained.

## Randomization

`schedule.py` fixes the seed `cs80-eight-native-average-v1/0x80305124` before timing. Four Williams rows are (0,1,3,2), (1,2,0,3), (2,3,1,0), (3,0,2,1). In each of six complete sets, independently draw a treatment bijection and row order with the specified SHA-256 counter generator, rejection sampling and descending Fisher-Yates. This gives six of each treatment at every position and six of every directed distinct predecessor pair within blocks. Each 12-block temporal half remains balanced. Between-block transitions are retained and reported but not claimed balanced. Fresh processes avoid shared mutable plugin state; shared machine conditions can still drift.

Schedule SHA-256: `e411f3b45439b80b5b61d48eebb7bb86f55a326d62f55cdd77a5d3543856fe62`.

## Statistical admission

The accompanying frozen `offline/statistical-plan.json` is authoritative for exact seeds and whole-set correlation sensitivity. Let x be log process CPU nanoseconds. In each complete four-process block define:

- P = x(S1) − x(S0).
- E = x(S+) − [x(S0) + x(S1)] / 2.
- G = [x(S0) + x(S1)] / 2 − x(C).
- G0 = x(S0) − x(C); G1 = x(S1) − x(C).

Use equal mean logs and a joint whole-block percentile bootstrap with 20,000 draws, independent seed `0x80305125`. Percentiles use linear interpolation at (n−1)p, p = 0.025 and 0.975. Signed placebo/extra effects are 100×expm1(mean); candidate savings are 100×[1−exp(−mean)]. Gate comparisons use percentages rounded to 12 decimal places.

All gates are mandatory: placebo entire 95% CI inside literal [−2%, +2%]; extra entire 95% CI inside [+3%, +10%]; candidate lower 95% saving bound strictly greater than 3% against pooled twins AND each twin separately. The 6.25% work increase is descriptive, not an assumption of exactly constant callback cost. A failed calibration yields an inconclusive candidate result, not an admitted gain.

Prespecified drift/order controls: both first-12 and last-12 block candidate lower 95% bounds exceed zero, and their point-saving gap is at most 3 percentage points. By actual candidate position (six blocks each), all candidate point savings exceed zero and their range is at most 5 percentage points. Placebo split into S1-before-S0 versus S1-after-S0 has each point effect within [−2%, +2%]. All subgroup contrasts and CIs are reported. Mandatory whole-Williams-set sensitivity averages each contrast within each of the six complete four-block sets. Its 95% t interval uses the six set means, df = 5 and critical value 2.570581835636305, transformed with the same signs. Placebo, positive and all three candidate gates must pass BOTH the 24-block bootstrap and six-set t intervals. All six set means and six leave-one-complete-set point estimates are reported; leave-one-set values are descriptive only, not optional exclusions. This retains within-set correlation but cannot prove independence against arbitrary same-VM drift.

No multiple outcome screening, exclusions, repeat/subset analyses or post-outcome retuning are allowed. Both favorable and adverse controls are retained, and all contrasts are reported even if an earlier gate fails.

## Collection and integrity

A one-shot authorization binds the exact plan and final freeze. The collector consumes a permanent latch before launching any timed process, writes a new campaign directory, and executes the fixed slot order once. It never prints, ranks, compares, analyzes or screens candidate/placebo/positive numerical outcomes while collecting. Only identity, endpoint/count/FP correctness, clock validity, external process success and resource safety are checked before the next launch. A numerical calibration failure is not an early-stop reason.

The loader environment is an explicit whitelist matching the zero-clock admissions: PATH=/usr/bin:/bin, LANG=C, LC_ALL=C, the pinned native prefix LD_LIBRARY_PATH, a new initially empty per-slot HOME outside the strict output directory, and the authorization token only. No caller-shell variables are inherited. The immutable prefix shared libraries are hash-pinned; each module path/stat and actual loaded path are checked. The fresh HOME is never reused across slots.

There are no retries, resumes, replacement slots, exclusions, selective repetitions or interim outcome access. Interruption, artifact drift, identity/correctness mismatch, invalid clocks, process failure or budget exhaustion stops the entire trial; partial raw data and the attempted-slot receipt remain. A partial trial has no admission. The full analysis is run only after a valid complete-96 terminal record and ledger.

Budgets are prospective: 240 CPU seconds (collector plus children) and 360 wall seconds for the full collection. Each process has a 4 CPU-second ceiling and 6 wall-second watchdog; the collector reserves 5 CPU seconds and enough wall budget before launching. Artifact hashing and validation are included in collection resources. These are safety limits, not performance endpoints. The expected budget is justified from existing held-stock core observations, not a new candidate timing: historical median 11,860.06 ns/frame implies about 1.17 seconds for 1,536×64 frames, or about 112 seconds for 96 ordinary runs before native/load/observer overhead. This estimate is not evidence of native performance; ample overhead is reserved, and the prospective bound is not expanded in reaction to measured outcomes.

## Scope and stopping

Frozen prerequisites include the existing AVX2/AVX512F/VL/DQ + OS ZMM-state capability receipt, the exact hook-free normal V2 module build receipt, and separate native reporting-module attribution (8,090 accepted eight-wide calls in its 16-call window) with normal/reporting PCM/FP/state parity. The new zero-clock host retains that prior 1,041-call prefix exactly. Measured runs do not add private backend counters, so this is external untimed attribution, not a new per-measurement hit assertion. The independent work-count result and untimed real-native correctness/attribution evidence explain what the candidate does; they do not substitute for measured controls. This experiment does not rescue earlier failed clock campaigns. Even an admitted result applies only to this held, average-throughput VST3 workload and frozen compiler/module/host configuration.
