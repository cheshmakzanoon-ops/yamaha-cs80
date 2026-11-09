# Offline interval endpoint-extrema experiment

## Preregistered scope

The previous point-operand paths keep the memory source solver offline. This
experiment also keeps all arithmetic/environment assumptions, diode/log
formulas, residual certificates, requested budgets and failure behavior. No
Engine/audio route or coefficient is changed.

For non-point operands, generic interval multiplication/division compute four
rounded endpoint combinations and select extrema. Multiplication needs only
two endpoint products unless both intervals straddle zero; monotonicity gives
the extrema from the operand signs. Division over a strictly positive or
strictly negative denominator likewise needs only the two extremal quotients.
Try selecting those two endpoints explicitly. Keep the old four-product path
when both multiplicands straddle zero. Retain every input/domain guard,
volatile rounded operation, outward ULP expansion and output validity check.
The existing point-operand paths remain unchanged.

Acceptance requires exact serialized status/hex-endpoint equality with the
baseline on the complete 7,374-vector suite and an additional independent
exact-Fraction corpus over all sign combinations, signed zeros, narrow and
wide intervals, gradual underflow, cancellation, overflow and invalid domains.
Skipping a non-extremal operation must not conceal a failure: independently
review why finite extrema imply finite interior rounded values, and retain
outward endpoint overflow rejection. Require the existing four compiler/
sanitizer profiles, all environment checks and fault controls, full 2,016-case
94-node gate, allocation checks and failed-publication checks. Do not alter
numerical limits or special-case observed failing vectors.

Use the same complete-output 128-evaluation timing probe and compiler options
for both versions. Solve scenarios vary over inputs 0..127; certificate-only
and preparation repeat their fixed workloads. Measure seven adjacent pairs
with alternating version order for warm updates and certificate-only work,
recording both wall and single-process CPU time. Retain separate unpinned and
child-only allowed-CPU-affinity phases, every pair and failure, both pairwise
and separate-median summaries. Benchmarks occur only after numerical gates
pass. A source-count reduction is not a measured speed claim. Shared-machine
means are not WCET, instrument utilization, deadline, scheduling or fallback
qualification. No activation is authorized by this experiment.

## Result: exact endpoints retained

The implementation selects the two extremal products when at least one
multiplicand is sign-definite. When both straddle zero it retains all four
products. For division, the denominator is validated as strictly positive or
strictly negative first; numerator monotonicity and derivative `-a/b^2` select
the two extremal quotients. Existing point paths and all other verifier code
are unchanged.

Exact-real extrema also bound the magnitude of every skipped endpoint
operation. Round-to-nearest and outward expansion are monotone, including the
existing shared expansion for both signed zeros. Thus an overflow at a skipped
endpoint cannot be hidden by finite extremal results: the selected extremum or
its outward expansion must also fail validation. Subnormal and zero results
retain the same bounds. This proves status/endpoint equivalence under the
existing strict binary64, gradual-underflow and masked-trap contract. It does
**not** promise identical sticky floating-point exception flags: eliminating
non-extremal operations can change flags that are not part of this API.

The candidate passes:

- All **7,374 existing vectors and 56 environment checks**, with serialized
  status/hex-endpoint output identical to baseline, SHA-256
  `18f80e5cdd43f124246817ab1df8cc389794d729616e8023ecf9f3341ba07536`.
  Optional 1e-15 A availability remains **860 accepted / 4 safely rejected**.
- **12,228 new exact-Fraction cases**, independently computing all four exact
  products/quotients instead of using the implementation's sign table. The
  oracle requires 7,243 successful exact endpoint pairs and 4,985 rejections.
  Baseline/candidate serialization is identical, SHA-256
  `539957645c95cfb50c0f4100d92a4564bd6dfc4840f2dba75e017a97bd0c14b5`.
  Serialization does not compare NaN payload bits.
- Both corpora under `-O0`, `-O2`, `-O3 -march=native` and an ASan/UBSan/
  float-cast-overflow/assertion profile. The existing 457-case rational log
  oracle and excluded-boundary checks pass. Fast-math compilation is rejected.
- **2,016 full-network cases / 533,518 checks**, zero failures and zero observed
  update allocations, including configuration, stale state, aliasing and
  destination-preservation failures. Max cold/warm steps remain 28/6; maximum
  dense-node discrepancy is 1.56923 nV and maximum accepted bound 6.01681 nV.
  The full network separately passes the sanitizer profile.
- Five registered focused CTests, and all eight deliberate faults detected:
  four existing primitive faults, two new endpoint-selection faults (1,253 and
  872 numeric failures) and two existing full-network faults (650 and 9).

LeakSanitizer is disabled for this sandbox; no leak qualification is claimed.
An independent reviewer rebuilt sanitized baseline/candidate probes, reproduced
both exact serialized hashes, both corpus counts and the 860/4 availability,
and approved the sign/overflow proof. That is correctness approval, not a
measured-performance or audio-activation approval.

## Equal-workload cost result: a small signal, not a deadline qualification

The [raw timing data](memory-interval-extrema-study/timings.csv) contain 168 rows:
84 unpinned and 84 child-affinity rows, seven adjacent version pairs per each
of three fixtures and two scenarios. Each row performs 128 complete-output
evaluations. Warm updates traverse inputs 0..127; certificate-only repeats the
fixed valid base state/input. The two versions use the exact same probe source,
options and corresponding inputs. All rows have zero failures, and paired
maximum steps/bounds agree exactly.

The runner alternates version order each trial. In the affinity phase it sets
and verifies the child's allowed CPU set before `exec` (CPU 0 here); parent
affinity is verified unchanged. This does not isolate the core or fix CPU
frequency. Both wall time and single-process CPU time are retained. CPU time
includes the two wall-clock timestamp calls and is not a deadline or measured
instrument-utilization estimate.

Affinity-controlled process CPU results in microseconds per one-rank call:

| Fixture/scenario | Baseline median | Candidate median | Median paired reduction | Improving pairs |
| --- | ---: | ---: | ---: | ---: |
| 50 kOhm unequal, warm | 253.1 | 243.3 | 1.3% | 6/7 |
| 100 kOhm full travel, warm | 134.0 | 131.4 | 1.7% | 6/7 |
| 100 kOhm unequal, warm | 290.4 | 276.0 | 5.4% | 6/7 |
| 50 kOhm unequal, certificate only | 158.3 | 155.3 | 4.2% | 4/7 |
| 100 kOhm full travel, certificate only | 69.7 | 66.5 | 4.3% | 7/7 |
| 100 kOhm unequal, certificate only | 206.3 | 187.6 | 11.2% | 6/7 |

Unpinned median paired warm reductions are 1.8–3.5%, with 5/7 improving pairs
for each fixture. Certificate-only unpinned reductions are 7.2–9.5%, but both
phases contain individual regressions. The affinity 50 kOhm certificate row is
particularly noisy: its pair changes range from **43.6% slower to 27.4% faster**.
The [summary](memory-interval-extrema-study/paired-summary.csv) retains every
range, both clocks and all improvement counts. Median within-pair percentages
are not percentages of the separate medians. No observation is discarded and
no precise general speedup is established.

Retain this bounded exact-operation reduction only in the offline research
boundary. Its interval outputs/statuses are independently validated and the
warm cost signal is modestly favorable in both phases; this is not a release
performance guarantee. These shared-machine means cannot certify worst-case
execution or another compiler/workload.

## Scheduling and failure handling remain unqualified

The affinity warm medians still cost about **131–276 us for one rank**, above
the proposed 83.3 us update period before a second rank, receiver readout or
the instrument. No Engine route, scheduler, worker publication, audio fallback
or sixteen-voice loaded topology is enabled or qualified here.

`update` still publishes only after the full-node certificate succeeds;
invalid input/configuration, arithmetic mode, stale state, overflow or exhausted
work leaves the prior destination untouched. An untouched old state is **not**
a certified fallback for changed current inputs. The certificate applies to
its exact prepared graph, input boundary and candidate, not a future held
interval. Any future scheduling step must explicitly bind those identities,
validate a complete new frame before publication, and either certify reuse
under fresh boundaries or derive a separate temporal error allowance. A missed
budget or rejected update must not silently be relabeled as success. This
increment supplies no such temporal/publication qualification.

## Provenance and reproduction

Source is local commit `6ff4a5b`; verifier SHA-256
`c5b282da5da6137974a671ba58f10d1596da47cd233c92633f867635378626b3`.
The baseline is `4a86f9d` and its point-path header is archived in the study.
These are local evidence identities, not release-head assertions. The
[manifest](memory-interval-extrema-study/manifest.json) hashes all measured
source/test inputs and retained results. Numerical budgets are unchanged.

Build `tests/MemorySparseProbe.cpp` with GCC 14.2 and
`-std=c++20 -O2 -Wall -Wextra -Wpedantic -Werror -ffp-contract=off`. Build both
versions from that identical source, using the archived baseline verifier in
an `experimental/` include overlay before `src` for the baseline. Run:

```sh
python3 tests/run_memory_cost_pairs.py baseline-probe candidate-probe scratch-results
CS80_COMPARE_RAW_INTERVALS=1 python3 tests/test_memory_verified_diode.py baseline-primitive candidate-primitive
python3 tests/test_memory_interval_extrema.py baseline-primitive candidate-primitive
bash tests/run_memory_verified_diode_checks.sh scratch-profile-build
```

The raw timing runner sets affinity only for its child processes on platforms
supporting Linux-style `sched_getaffinity`/`sched_setaffinity`. It is an optional
research measurement, not an automatic CI performance threshold.
