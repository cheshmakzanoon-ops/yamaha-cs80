# Offline verified-interval point-operand cost experiment

## Preregistered scope

The [centered-log experiment](memory-certificate-cost.md) retains the audio
activation blocker. This experiment changes no Engine route, graph coefficient,
requested error bound, diode/log approximation or fail-closed requirement.

The general interval multiplication/division helpers evaluate four endpoint
combinations. When an operand is an exact point interval, two are duplicates;
when both are points, three are duplicates. Point-point addition/subtraction
also repeat the same rounded operation. Try explicit point-operand paths that
perform each distinct rounded operation once, retain outward expansion and
finite/domain checks, and select endpoint order using the proved monotonic
sign. No operation may assume an interval is a point merely because it is
narrow. Do not remove volatile rounding stores, outward ULP expansion or the
analytic series tail.

Acceptance requires independent exact-rational tests for point-left,
point-right and point-point cases, including signs, zero, cancellation,
subnormal and overflow/rejection behavior. Compare baseline/candidate raw
interval outputs as well as oracle containment. Retain all existing primitive
profiles, invalid-environment tests, fault controls and the full 2,016-case
94-node sparse gate with sanitizer and failed-publication checks. A source
review must independently verify endpoint order and unchanged failure scope.

Use the same 128-evaluation probe for both versions. The three solve scenarios
traverse input indices 0..127; certificate-only and coefficient preparation
repeat their identical fixed workloads. Materialize complete outputs, preserve
all trials/failures and repeat in reverse version order. Accept a cost change
only on measured evidence, not operation counts alone. Shared-machine timing
is not WCET, actual CPU utilization or whole-instrument qualification. This
remains offline regardless of a scalar speed improvement.

## Measurement follow-up after the initial wall-time trials

The first four full-probe runs gave mixed wall-time changes and substantial
between-run variation. Do not call those a successful speedup or discard them.
Add separately labeled process CPU time (`std::clock`, single-threaded probe)
to distinguish arithmetic cost from descheduling, without presenting it as a
wall-time deadline bound. Add a mode/trial selector, keeping the same three
fixtures and 128 corresponding evaluations. Alternate baseline/candidate in
shorter adjacent trials for the warm-update and certificate-only workloads;
reverse order on alternating rounds. Preserve both the original full trials
and these follow-ups. This is a measurement refinement prompted by observed
noise, not a retrospective change to numerical acceptance limits.

The short-trial follow-up was also repeated with both child benchmark processes
restricted to the first CPU in the parent's allowed affinity set (CPU 0 here).
Only child affinity changed; the parent remained untouched. This reduces core
migration as a confounder but does not provide an isolated machine, fixed CPU
frequency or a deadline guarantee. Keep this phase and its mixed results
separate from the unpinned measurements.

## Numerical result: identical endpoints, unchanged certificate

The point paths preserve the exact arithmetic contract. After interval/domain
validation, addition/subtraction with two points perform the same rounded
operation once instead of twice. Point multiplication orders its two distinct
endpoints by the scalar sign. Fixed-denominator division orders endpoints by
the denominator sign; fixed-numerator division follows derivative `-a/x^2` on
either permitted denominator-sign interval. Two points use one rounded result.
Every result still receives the same outward ULP expansion and `checked()`
validation. A zero-containing denominator is rejected before any shortcut.

Signed-zero endpoints can compare equal, but the existing outward helpers map
both rounded zero signs to the same ±minimum-subnormal enclosure. Endpoint
overflow/nonfinite values still produce an invalid interval. Masked FP traps,
strict binary64 rounding and the other existing arithmetic preconditions are
unchanged. Neither the diode/log formula nor its analytic tail is shortened.

The expanded oracle retains all prior vectors and adds 1,800 point-left/right/
point-point cases plus 96 signed-zero/domain/overflow/nonfinite cases. **7,374
vectors and 56 environment/self checks pass**. Baseline and candidate produce
byte-identical serialized status/hex-endpoint outputs, SHA-256
`18f80e5cdd43f124246817ab1df8cc389794d729616e8023ecf9f3341ba07536`.
This comparison includes failure outputs; it does not claim to compare NaN
payload bits that the text serialization does not encode. Tight-width diode
availability remains 860 accepted / 4 safely rejected.

All four existing compiler/sanitizer profiles pass the expanded suite, and the
457-case exact-Fraction logarithm oracle still passes. The complete sparse
network remains **2,016 cases / 533,518 checks, zero failures and zero observed
allocations**, with the same 1.56923 nV maximum dense-node difference, 6.01681 nV
maximum accepted certificate and 28/6 cold/warm iteration maxima. The full gate
also passes ASan/UBSan/float-cast-overflow and assertions; LeakSanitizer remains
disabled for this sandbox. Four registered focused CTests pass and all six
existing fault controls remain detected.

Two independent source reviews approve the monotonic endpoint order, exact
point test, outward expansion and unchanged failure scope. A separate reviewer
also compared an independently constructed 1,600-case signed-zero/subnormal/
overflow/invalid corpus against the old header with identical serialized
outputs. These are correctness approvals, not speed or audio approvals.

## Cost result: promising warm-path signal, not a uniform speedup

All compared versions execute corresponding workloads and materialize full
outputs. The three solve scenarios vary over input indices 0..127; the
certificate/preparation scenarios retain their fixed workload repetitions.
Every recorded row has zero evaluation failures. The [manifest](memory-interval-cost-study/manifest.json)
identifies each exact probe/header and records all 588 timing rows.

### Initial full-probe wall-time runs were mixed

Each entry below is baseline → point-path median microseconds per rank warm
update. The two columns are the forward and reversed version-order runs.

| Fixture | First order | Reverse order |
| --- | ---: | ---: |
| 50 kOhm, unequal banks | 277.7 → 238.1 | 283.2 → 311.0 |
| 100 kOhm, full travel | 142.8 → 163.4 | 177.3 → 125.3 |
| 100 kOhm, unequal banks | 330.3 → 369.4 | 379.2 → 359.3 |

These do **not** demonstrate a uniform improvement. Certificate-only timing
is mixed too. The complete initial files remain available:
[baseline first](memory-interval-cost-study/baseline-forward.csv),
[point second](memory-interval-cost-study/point-forward.csv),
[point first](memory-interval-cost-study/point-reverse.csv), and
[baseline second](memory-interval-cost-study/baseline-reverse.csv).

### Short adjacent paired trials reduce, but do not remove, variability

The follow-up measures both wall time and Linux process CPU time in the
single-threaded probe. Baseline/point order alternates across seven adjacent
pairs per fixture/scenario. Each row still contains 128 evaluations, not a
single-call latency. The unpinned phase has positive median paired warm CPU
reductions of 20.1–23.3%, with 6/7 pairs improving in each fixture, but includes
individual regressions. Process CPU time is a cost diagnostic, not a wall-time
deadline bound or measured utilization.

A second phase uses CPU 0, the first permitted CPU, for child processes only.
Parent affinity is unchanged. This controls migration, not other machine load
or CPU frequency. Its results are:

| Fixture/scenario | Baseline median CPU us | Point median CPU us | Median within-pair CPU reduction | Improving pairs |
| --- | ---: | ---: | ---: | ---: |
| 50 kOhm unequal, warm | 289.2 | 252.1 | 10.4% | 7/7 |
| 100 kOhm full travel, warm | 156.7 | 139.7 | 8.4% | 6/7 |
| 100 kOhm unequal, warm | 332.3 | 283.2 | 12.1% | 6/7 |
| 50 kOhm unequal, certificate only | 219.5 | 181.6 | 16.0% | 6/7 |
| 100 kOhm full travel, certificate only | 85.9 | 93.5 | 8.4% | 5/7 |
| 100 kOhm unequal, certificate only | 264.5 | 205.0 | 19.6% | 6/7 |

The median of paired percentage changes is **not** the percentage change of
the two separate medians. In particular, the full-travel certificate row has
worse separate medians even though its median within-pair ratio improves.
That disagreement and the negative individual pairs are retained, not resolved
by choosing whichever summary looks favorable. The [paired summary](memory-interval-cost-study/paired-summary.csv)
includes every pair range and improvement count; the
[unpinned](memory-interval-cost-study/alternating-short.csv) and
[pinned](memory-interval-cost-study/alternating-pinned.csv) raw files preserve
all observations. No precise general speedup is established by these runs.

## Decision and remaining gate

Retain the small point-operation implementation in the **offline experimental
boundary**: it is independently reviewed, preserves observed endpoints exactly,
and the adjacent warm-path trials provide a modest favorable signal. Do not
turn that decision into a claim of uniform or release-qualified performance.
The initial mixed full-probe results and later regressions remain material
caveats. A different compiler/workload needs its own measurement.

Audio activation is still rejected. Even the favorable affinity-controlled
warm medians, about 140–283 us of process CPU time for one rank, exceed the
83.3 us proposed update period before a second rank, receiver readout or the
instrument. Neither fewer scalar operations nor these averages qualify
worst-case execution, causal scheduling, frame publication, fallback behavior
or sixteen-voice loading. The next larger cost target should be redundant
candidate/verified source work that can be skipped under an explicit bound,
not removal of residual checks or silent precision reduction.

## Source and reproduction

Candidate source is local `1131b07`, header SHA-256
`5989c0969b9c8292817f44e9fd3e55b89c44e884f820bceaca67d6c712c01bb0`.
The centered baseline header is retained in the study directory. The original
full-probe source and exact shorter-trial probe are also archived: the final
probe only adds a check for a failed ending CPU timestamp after both measured
timestamps, outside the measured region. This preserves exact input provenance
without pretending the initially compiled probe had that later guard.

Build both versions with GCC 14.2 using
`-std=c++20 -O2 -Wall -Wextra -Wpedantic -Werror -ffp-contract=off` and identical
probe source, supplying the historical header through an `experimental/`
include overlay for the baseline. The short probe accepts
`output.csv mode trial`; mode 2 is warm update, mode 3 certificate-only, and
trial is 0..6. Invoke baseline first for even trial indices and point first
for odd indices. Each invocation emits the three fixtures. The optional pinned
phase sets only each child process's affinity to the recorded first allowed
CPU. All source, probe, result and affinity identities are in the manifest.

For exact endpoint compatibility, build `MemoryVerifiedDiodeTests.cpp` once
with each header and run the independent Python oracle with both executables:

```sh
CS80_COMPARE_RAW_INTERVALS=1 python3 tests/test_memory_verified_diode.py \
    /path/to/baseline-probe /path/to/point-probe
```

`tests/run_memory_verified_diode_checks.sh` retains four normal profiles,
four primitive faults, fast-math rejection and the exact-rational log oracle.
The CMake sparse tests retain complete-node and failed-publication checks.
No production Engine/plugin include is introduced by this increment.
