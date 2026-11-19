# Qualifying a clock to measure small VST3 CPU savings

The [previous four-arm campaign](../../docs/experiments/eight-stage-filter/native-cost/README.md)
finished intact, but its same-binary interval extended from −3.84% to +5.33%
even though the correct answer was zero. Pinning the process and using thread
CPU time alone did not eliminate enough noise. It is not valid to select the
fastest runs, regard callbacks as independent replicates, or subtract the
wall-minus-CPU difference from the recorded thread cost. This qualification is
an experiment in a different *measurement design*, not a rerun of the failed
eight-wide candidate.

Use three independently initialized instances of **one stock VST3 module** on
one pinned thread, with adjacent short windows and balanced role order. Every
instance actually executes equal numbers of genuine callbacks at corresponding
musical positions. On 32 stripes of 17 calls, all arms measure the same first
16 calls; only S+ measures the 17th. Untimed catch-up for S0 and S1 preserves
equivalent engine timelines instead of comparing different oscillator and
release phases. Track callback CPU time exclusively around the real VST3 call;
hashing, trace serialization, and comparisons are outside that bracket.
The positive control is extra completed audio work, not a sleep, spinner, or
altered denominator. Save raw timing rows and validate audio, public state,
floating-point state, affinity, and counts before issuing a complete summary.

For one predeclared session, take exactly 12 fresh-process triplets on the same
machine, with no process exclusions or repeat-to-pass. Do not run alongside a
build, profiler, or other deliberate workload. Use the runner's signal,
scheduling, and machine diagnostics to report limitations, not to selectively
delete slow periods. An aborted acquisition is invalid, not a shortened session.

`analyze.py` consumes the complete-session summary. Within each triplet it
computes stock-twin and two positive-control contrasts as log ratios, then
makes a two-sided 95% Student t interval across the **12 process triplets**,
not across the 32 stripes or thousands of callbacks. In every case a gate
requires the *entire* interval to fit:

| Contrast | Numerator / denominator | Required interval |
| --- | --- | --- |
| Stock twins | S1 / S0 | −2% to +2% |
| Common work | S+ matched 512 calls / geometric mean(S0, S1) | −2% to +2% |
| Extra work | S+ all 544 calls / geometric mean(S0, S1) | +3% to +10% |

The matched subset of S+ calls is every first-16 stripe, **not** its first 512
consecutive calls. The ordinary measured total is 512 callbacks even for the
extra-work denominator. The reported median CPU ns per callback is auxiliary
description, not a qualification estimator. The t intervals depend on
approximately independent process contrasts; this screening design cannot
guarantee independence on a shared machine.

```sh
python3 -B analyze.py <new-run>/summary.json
python3 -B -m unittest -v test_analyze
```

Exit 0 means all stock gates passed for that acquisition class. A cloud
diagnostic can pass the numerical gates but can never qualify dedicated
target hardware. Exit 2 means a complete run failed at least one gate.
Exit 1 means malformed/incomplete input. The analyzer is not a
replacement for the collector's raw-evidence validation; the summary is its
input, not independent proof of acquisition integrity. Passing only qualifies
the tested host/workload setup to proceed to a separate candidate design.
It establishes no speedup, whole-host CPU utilization, glitch-free deadline
performance, or result for idle, wake, release, or other platforms.
