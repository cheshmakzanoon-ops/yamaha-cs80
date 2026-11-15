# Exact dynamic-state AVX admission: candidate evidence

This candidate reuses six already gathered state vectors to check bitwise magnitude bounds, then checks finite slopes with the existing floating-point comparison semantics. It changes no filter solver arithmetic, static coefficient eligibility, runtime ISA gate or scalar fallback. Rejected packs leave histories and output untouched. Signalling NaNs are rejected without evaluating them; all four FTZ/DAZ combinations are covered.

## Measured narrow experiment

Local baseline `bfbd8f6506836c267911b41cc2fc7295a10860aa`, AVX enabled; candidate `bfa32f6` adds only dynamic-state admission and its tests. Matching GCC 14.2 builds use `-O3 -DNDEBUG -fPIC -std=c++20 -ffp-contract=off` for all translation units, no native ISA tuning or fast-math. Measured binaries and original commands are identified in `quick-pairs/input-hashes.txt` and scripts. These pre-integration binaries differ from later fresh default-CMake qualification builds.

Three alternating quick paired runs preserved all 36 matched checksum rows each. Median source warmed-held candidate/baseline mean callback ratios: approximately 0.98528, 0.96132, 0.96802. This is a narrow 1.5–3.9% timing improvement, not a general CPU claim. Raw CSV, metadata, logs, exit codes, scripts and unfiltered summary remain under `quick-pairs/`.

Virtualized Intel Xeon Platinum 8573C, nominal 2.3 GHz, 9 visible logical CPUs; benchmark pinned to CPU 0. Shared-host scheduling remains uncontrolled. Metric is mean timed callback wall duration divided by represented audio duration. It includes callback control events and render, not DAW/driver/plugin UI. It is **not system CPU percentage**, and does not establish the requested under-2% goal. Raw process/thread CPU counters include validation work and are not render-only CPU. Windows/macOS performance is not established.

## Integrated qualification

Integrated atop local default-AVX lineage `b5b25297` (qualified default-AVX production lineage), source candidate `9b558fe`. Fresh CMake Release configuration preserves default AVX eligibility with recovery OFF; unsupported contracts and CPUs retain scalar behavior. Wider 48-kHz/256-frame and idle checks are recorded below; qualification is scoped to these workloads.

Fresh integrated qualification passed 7/7 registered tests (batch, engine parity, scheduling, compiler/math-contract admission, historical replay and negative missing/truncated inputs). ASan/UBSan edge suite passed 1,183,425 checks with leak detection disabled. The full 9,332,176-byte historical source-audio V2 corpus compared byte-identical, SHA-256 `08a381f4ac9de1709a9c9197a78baefb12ffac93cd79f76d4b1e92d4f329e195`. Logs and replay commands are retained in `qualification/`; these are targeted checks, not a claim that every repository test or platform CI passed.

## Follow-up CPU and idle evidence

Fresh default-CMake binaries completed a full 48 kHz / 256-frame performance pair with 100 exact rows: median matched warmed source wall ratio 0.97683 across 33 cases. Initial quiet-isolation idle was slower (ratio 1.14517), retained rather than removed.

The opt-in callback-only thread-CPU instrumentation is included in this branch and independently qualified. Identically instrumented active quick comparisons at 48 kHz / 128 frames have held-source CPU ratios 0.98647, 0.97641 and 0.98744. Summing callback CPU across every active-suite phase yields ratios 0.99599, 0.99419 and 0.98395. These are modest measured workload improvements, not an under-2% result. This callback CPU metric excludes validation/observation and controller synthesis, unlike the older including-checks counters; it includes the callback's wall-clock reads and adds clock instrumentation overhead equally to both sides.

Longer 512-callback baseline/idle comparisons are mixed. Quiet isolation CPU ratios are 1.01307, 0.98830 and 1.05170; overall suite CPU ratios are 0.96909, 0.94918 and 1.13204. Third-round release outliers and scalar-only control variation remain recorded. Zero-active dispatch does not execute the changed batch kernel, but these measurements are not proof of uniformly unchanged idle cost. One active-suite chorus/reverb selector-10 release case also has small repeated CPU regressions (about 2.69%, 1.22%, 0.32%). No universal or regression-free performance claim is made.

All matched result keys and checksums agree and nonfinite counts remain zero. Independent audit is retained with the thread-CPU evidence. Two additional idle confirmation pairs completed with exact outputs. Quiet-isolation callback CPU ratios were 1.05924 and 0.96673; overall isolation held medians were 0.98827 and 0.96673. The larger original quiet-idle penalty did not consistently recur, but idle results remain variable. The accepted scope is a modest repeatable active-workload improvement, with documented counterexamples and no guarantee for every phase or host. Combined measurement-tool unit/schema/preflight regression passed 3/3; logs are included.

## Promotion decision

Accept the exact dynamic-state validation rewrite for its repeated active-workload benefit, not as a universal cost reduction. The independent review reproduced the paired data and found no correctness blocker. All negative, mixed and adverse timing results remain included. The zero-active path does not call the changed vector kernel; no blanket idle-state or circuit-history skipping was introduced. No audio approximation, quality setting, tolerance, historical reference or unsupported-platform policy changed. Additional CPU reduction remains open.
