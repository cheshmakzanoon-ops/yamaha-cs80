# Memory nonlinear-range and scheduling study

## Preregistered scope

This study follows the experimental
[semantic/loaded-receiver boundary](memory-control-routing.md). It does not
activate that boundary in the Engine. “Truth” below means the existing declared
nonlinear network model, not a measured instrument.

Extend the offline dense KCL reference to accept explicitly supplied bus-current
feedback `I = constant + L * Vbus`. The real memory/panel diodes, all inactive
branches, rail drivers, followers, shunts and eight receiver loads stay in the
nonlinear solve. Add the current derivatives to its analytic Jacobian. Separately
check the result by repeated nonlinear source solves with externally updated
receiver currents; retain convergence and residual checks, never the last
unconverged iterate. A null or all-zero feedback matrix must preserve the old
reference's bus/Jacobian bits.

The candidate accuracy limits are frozen in
`tests/parity/tolerances-v2/memory-nonlinear-study.json` before this study runs:
1 mV maximum bus error, 0.001 effective-cutoff-CV error, 0.1% relative Q and
stage-duration errors, and 0.001 absolute normalized level-gain error. These
are proposed numerical approximation budgets, not sourced Yamaha tolerances
or perceptual certification. A 0.001 effective-CV error corresponds to roughly
0.069% frequency error under the current exponential model.

Sweep both ranks, memory/PANEL selectors, declared pot settings, endpoints and
unequal banks. Separate zero-drive prepared maps from maps relinearized at
nonzero receiver operating points. Report actual injection distance and errors
for every tested case. Passing sampled cases does not establish a continuous
safe radius, untested bank configuration or a universal fixed-map replacement.

Evaluate candidate scheduling separately from nonlinear approximation error:
compare held shared buses with fresh per-voice receiver inputs against the
per-sample experimental linear solution, then against the nonlinear reference.
Include smooth trajectories and abrupt/fast stress inputs, not just static
held notes. Test candidate fixed update intervals rather than assuming a
musically useful rate is adequate. No interpolation may rely on future inputs
unavailable to the audio callback.

Measure preparation and full receiver evaluation on the same machine, including
varying controls. Report work per rank/frame and the projected readout-only
budget at audio rates; do not call that whole-instrument CPU or a worst-case
deadline guarantee. Separate expensive nominal PWM/envelope-law decoding from
the shared electrical solve and per-voice algebra when considering a later
schedule. Do not move the allocating dense compiler into `processBlock`.

The outcome is a documented accepted/rejected scheduling proposal and the next
bounded implementation step, not automatic shipping approval. Whole-instrument
cost, asynchronous publication/late-map behavior, 16-voice topology and
host/source-selection continuity remain separate obligations.

## Result: do not activate the current fixed map

The full-range fixed-map approximation and a blind fixed-rate hold are rejected.
There is no Engine/plugin activation in this increment. The small affine
`residualAmps` in `LoadedMemoryReceivers` certifies its local-linear receiver
closure, **not** the nonlinear memory source equations.

### Nonlinear source error

The sweep contains 12,474 sampled configurations; 11,892 meet all candidate
limits and **582 do not**. This is a designed grid, not a probability estimate
for user performances. The maximum errors are:

| Quantity | Largest sampled error | Candidate limit |
| --- | ---: | ---: |
| Bus voltage | 0.608203 V | 0.001 V |
| Effective cutoff CV | 0.00433623 | 0.001 |
| Q, relative | 5.05632% | 0.1% |
| Stage duration, relative | 0.0117857% | 0.1% |
| Normalized level gain, absolute | 0.0000261652 | 0.001 |

Both ranks, selectors 11/12/13, 10/50/100 kOhm assumptions and seven physical
wiper configurations are covered. Shapes 0–4 are uniform 0/0.25/0.5/0.75/1;
shapes 5/6 are deterministic unequal per-row/per-bank patterns. These physical
positions include some extremes outside the current semantic adapter's host
range. Center 0 prepares at zero receiver drive; center 1 prepares at a
nonzero combined drive. The probe moves toward positive/negative envelope,
touch, RES, combined and zero-drive destinations at the recorded fractions.
All truth solves converge; an invalid solve aborts the probe rather than
disappearing from the denominator.

For sampled nonzero excursions with the evaluator's maximum current delta at
most 3 microamps, all 5,734 cases pass. The worst bus errors are 0.0141 mV for
the zero center and 0.4014 mV for the nonzero center. **This is not a certified
3-microamp radius**: it covers sampled configurations/directions only. At
4.54545 microamps from a nonzero operating point, row 8 already misses the bus
budget by reaching about 1.159 mV error. Relinearizing is necessary but does not
make an arbitrary-sized next step accurate.

The registered truth test also constructs a corresponding **selected memory**
case entirely through valid stored float controls, with no adapter clipping.
Its row-8 error is 1.15896256 mV at a 4.54545455-microamp change, while its
affine receiver residual is only 2.44e-21 A. This defeats the interpretation
that successful affine closure alone qualifies the audible source. The bus
budget is an engineering approximation requirement, not a claim of audibility
at that particular 1.159 mV discrepancy.

Raw aggregate data:
[sweep summary](memory-nonlinear-study/nonlinear-summary.csv),
[current-distance groups](memory-nonlinear-study/nonlinear-radius.csv), and
[progressively smaller-current counterexamples](memory-nonlinear-study/nonlinear-counterexamples.csv).
The counterexample list is not an exhaustive list of all 582 rejected cases.

### Scheduling: fresh local inputs and explicit event updates are essential

Twelve 2,048-sample trajectories at 96 kHz compare the per-sample linear result
and nonlinear truth with a causal prototype: hold only the shared buses and
stage controls, but recompute each voice's cutoff/Q/level from fresh inputs.
The trajectories include smooth movement, roughly 1 ms envelope attacks, and
abrupt master/velocity/pressure changes. Event periods and phase offsets are
deliberately coprime to the tested update intervals; an earlier aligned-grid
smoke probe was inadequate because it could accidentally sample every edge.
These are supplied control-state trajectories: the study does not yet integrate
an actual envelope using the held stage durations or render the resulting audio.

| Bus interval at 96 kHz | With event-triggered updates: all-sample scheduling passes | Worst scheduling-only bus/CV error |
| --- | ---: | ---: |
| 1 sample | 12 / 12 | Roundoff only |
| 2 samples | 12 / 12 | 0.0630 mV / 0.0000630 |
| 4 samples | 12 / 12 | 0.1891 mV / 0.0001891 |
| 8 samples (12 kHz) | 12 / 12 | 0.4411 mV / 0.0004411 |
| 12 samples (8 kHz) | 12 / 12 | 0.6932 mV / 0.0006932 |
| 24 samples (4 kHz) | 8 / 12 | 1.4494 mV / 0.0014494 |
| 48 samples (2 kHz) | 5 / 12 | 2.9618 mV / 0.0029618 |
| 96 samples (1 kHz) | 4 / 12 | 5.9866 mV / 0.0059866 |

Without event updates, even a two-sample interval fails the abrupt-input cases;
the worst missed bus change is 0.923 V. Events here are supplied by the known
synthetic input sequence, not inferred from future samples. A real controller
must carry the actual MIDI/control event timing.

Scheduling and linearization consume the **same total error budget**. Even
with per-sample evaluation, only 10/12 trajectories meet the combined nonlinear
budget. At an event-aware 12-sample interval the combined result passes only
9/12, although scheduling error alone passes all 12. Thus the table is not
permission to ship an 8 kHz memory path. The 8-sample candidate leaves more
room for an independently enforced nonlinear-error allowance, but it too needs
a validity certificate and wider sample-rate/trajectory coverage.

Full [scheduling results](memory-nonlinear-study/scheduling.csv) separate
scheduling-only, combined and per-sample-linearization errors and retain actual
electrical-update counts. There are 24,576 nonlinear trajectory samples reused
across the candidate schedules, not a separate nonlinear solve for every cell
in that table.

### Cost: dense re-preparation is the largest immediate obstacle

The final GCC 14.2 / Linux x86-64 probe uses `-O2`, seven alternating-order trials
and 500,000 varying-input iterations per candidate. A whole-object compiler
barrier keeps every intended output live; the checksum alone is not relied on
to prevent dead-work elimination. Times are **shared-machine wall time**.
Ranges below are ranges of trial averages, not per-call worst-case latency.

| Candidate | Median ns per rank/frame | Range of trial averages | Projected two-rank readout-only time at 96 kHz |
| --- | ---: | ---: | ---: |
| Full evaluator every sample | 749.9 | 674.1–1074.0 | 0.144 s/s |
| Partial held-bus projection; 8-sample updates | 169.1 | 149.2–210.9 | 0.0325 s/s |
| Partial held-bus projection; 12-sample updates | 164.8 | 118.2–170.5 | 0.0317 s/s |
| Partial held-bus projection; 24-sample updates | 97.2 | 92.1–129.4 | 0.0187 s/s |
| Partial held-bus projection; 96-sample updates | 75.2 | 72.6–98.4 | 0.0144 s/s |

The two workloads do **not** return equal output sets. The full path materializes
every `MemoryReceiverReadout` field. The scheduled prototype materializes only
its `Observed` projection: buses, effective CV, Q, level gain and six stage times.
It does not retain pin diagnostics or several oscillator, target, sustain and
provenance fields. Its timings are partial-work projections, **not the measured
cost of a complete scheduled receiver API** or a like-for-like speedup claim.

These are prototype costs, not whole-instrument CPU, a release speedup or a
deadline guarantee. The fine differences between adjacent schedules are not
reliable performance rankings under this contention. The timing input repeats
a 1,024-sample smooth-control segment, including wrap discontinuities, without
event-triggered refreshes. Dense event traffic can force the full update rate
and must be budgeted as such. The raw file also retains the intentionally redundant one-sample
scheduled path rather than omitting its extra overhead.

Forty complete offline re-preparations (full nonlinear receiver solve, followed
by map compilation at that current point) take **1.589–60.834 ms**, median
10.983 ms, with a sampled 95th percentile of 52.795 ms. Even the fastest sampled
preparation is far longer than an 8-sample/96-kHz interval. This allocating dense
path cannot be used in `processBlock` or presumed to refresh frequently enough
on a worker. These observed extremes are not WCET bounds.

Raw [readout timing](memory-nonlinear-study/cpu.csv) and
[preparation timing](memory-nonlinear-study/preparation-cpu.csv) retain every
trial. Earlier exploratory runs varied materially under machine contention;
only the final frozen-source run supplies this table.

## Next bounded implementation: a certified sparse source update, still offline first

1. **Retain a complete candidate node state and configuration identity.** The
   current bus map lacks the rail/wiper voltages required to evaluate the true
   nonlinear KCL residual. Preserve all three banks, selector, electrical
   assumptions, model versions and generation identity. Changing any wiper or
   source selection changes the graph operating point even when injected-current
   distance is zero; a current-distance check cannot detect a stale configuration.
2. **Implement and validate a nonlinear residual certificate.** In the declared
   memory/PANEL model, `K = -dF/dV` has nonpositive off-diagonal entries. Passive
   diode conductances cancel from row dominance; ideal follower ratios lie in
   [0,1]. A conservative positive margin is

   ```
   m = min(driverConductance, 1000,
           1 / (potOhms + endOhms),
           min_analog_rows(1/10000 + receiverLoadSiemens - sum_j L[row,j]))
   ```

   Here `L` is the nonnegative receiver-current feedback matrix and the
   reference's zero-output-impedance driver uses 1000 siemens. For fixed valid
   velocity/pressure boundaries, this margin is independent of diode voltage.
   A future certificate must explicitly validate entrywise `L >= 0`, finite
   configuration values and `m > 0`; otherwise this argument is inapplicable.
   Under the exact declared equations, integrating the Jacobian along the line
   between candidate and solution preserves strict row diagonal dominance;
   the inverse infinity-norm bound gives
   `maxNodeVoltageError <= maxAbsTrueKclResidual / m`.

   This is a **design derivation, not an implemented floating-point certificate**.
   It needs independent review, full-node residual evaluation, diode-solver and
   rounding-error allowances, and counterexample tests. It cannot be applied to
   the existing affine `residualAmps`. At nominal 10/100 kOhm settings, the simple
   margin is approximately 1e-4/1e-5 siemens, corresponding to roughly
   100/10 nA residual budgets for a 1 mV node-error budget.
   Propagate that certificate through the receiver equations as well: under a
   uniform node error `epsilon`, HP effective-CV error can reach `1.65*epsilon`
   and LP can reach `3*epsilon` at full initial/after touch. Thus a 0.0005-CV
   nonlinear allowance can require a node bound at most 0.000167 V, not 1 mV.
   At fixed fresh external boundaries, conservative relative Q error is at
   most `0.45*epsilon`, stage-time relative error is at most
   `exp((0.3*ln(10))*epsilon)-1`, and absolute level-gain error is at most
   `((1+2*560/270)/10)*epsilon`. These do not cover input/scheduling errors or
   subsequent envelope integration.
3. **Replace dense preparation with a bounded sparse nonlinear update kernel.**
   Exploit each wiper's local connections and the three shared rail coordinates
   rather than allocating/eliminating a 94-by-94 matrix. Warm-start from the
   previous complete state; limit iteration count, verify the true residual
   certificate, and fail without publishing an unconverged state. Compare every
   result against the dense oracle across the current counterexamples and a
   wider parameter/drive domain before considering audio-thread use. A faster
   diode evaluation also needs an explicit error bound, not just an elapsed-time
   win.
4. **Only then qualify a causal schedule and publication path.** Separate fast
   per-voice algebra from shared source updates and expensive static-law decoding.
   Start with an event-aware, no-future-input schedule no slower than the sampled
   8-sample/96-kHz candidate, with separate nonlinear and scheduling allowances
   whose combined bound fits the total budget. Absolute-error allowances add.
   For the relative Q/stage errors used here, scheduling is relative to the
   linear result and linearization is relative to truth, so use
   `e_total <= e_schedule + e_nonlinear + e_schedule*e_nonlinear`, or reformulate
   both in a common-denominator/log-ratio domain. Do not simply add those
   relative allowances. The directly measured combined CSV errors already use
   truth as their denominator and are unaffected by this budgeting distinction.
   Scale and test the schedule at other internal rates.
   A failed/late preparation does not authorize indefinite use of a stale map.
   Define a genuinely qualified fallback before activation; immutable publication
   alone does not solve source validity or source-change continuity.

The residual certificate and sparse kernel are the next concrete deliverable.
Engine activation, 16-voice load policy, whole-instrument performance and native
host/source-switching evidence remain after that, not claimed by this research.

## Verification and reproduction

- The dense receiver truth agrees with independently repeated nonlinear source
  solves in 48 cases: 1,396 checks pass, largest voltage difference
  1.77636e-15 V, largest KCL residual 7.67952e-15 A. The independently updated
  source solves converge within four iterations in this grid.
- A 36-case old/new null-feedback binary comparison preserves 2,553,120 bytes of
  bus/rail/Jacobian values exactly. SHA-256:
  `65d2fe8ae8df038d67b6d255372f7f306efb4e5464b2bb74f81f820f06743ca0`.
- Nine focused CTests pass. Comparator self-tests reject NaNs, infinities and
  invalid relative-error denominators rather than allowing `std::max` to hide
  an invalid sample. The final finite-guarded numerical CSVs reproduce the
  earlier finite numerical results byte for byte.
- Fresh ASan + UBSan with library assertions passes the 1,396 truth checks and
  comparator self-tests; LeakSanitizer is disabled in this sandbox. Independent
  UBSan/float-cast-overflow review reproduces the same truth and comparator results.
- The preregistered limit file remains byte-identical, SHA-256
  `75171d226efeeb6de3acf18256c074d13470cea503b8b09264d341b1e5af4593`.
  The source/content provenance and final result hashes are in
  [manifest.json](memory-nonlinear-study/manifest.json).

Reproduce the study from the repository root:

```sh
c++ -std=c++20 -O2 -Wall -Wextra -Wpedantic -Werror -I src \
  tests/MemoryQualificationProbe.cpp -o /tmp/memory-study
/tmp/memory-study tests/parity/tolerances-v2/memory-nonlinear-study.json /tmp/memory-results sweep
/tmp/memory-study tests/parity/tolerances-v2/memory-nonlinear-study.json /tmp/memory-results schedule
/tmp/memory-study tests/parity/tolerances-v2/memory-nonlinear-study.json /tmp/memory-results cpu
```

The opt-in CMake probe target runs the same source; ordinary CTest runs only its
comparator self-test, not a noisy timing gate or a claim that every sampled
approximation meets the candidate budget. Benchmark numbers should change with
the machine/load; the deterministic numerical CSVs should not be substituted by
timing results.
