# Offline sparse memory update and full-node residual gate

## Preregistered scope

The previous [nonlinear study](memory-nonlinear-qualification.md) rejects the
fixed affine map as a general audio source replacement. This increment remains
an offline, opt-in experimental path. No Engine, host state or production
receiver routing is changed.

Retain all 94 reference nodes: three rails, 66 local wipers, an inactive
factory-emitter coordinate, and 24 analog buses. Admit only the two ranks and
memory/PANEL selections 11..13, with finite validated electrical configuration.
Preserve complete configuration identity. Reject stale warm starts, invalid
inputs and incomplete/uncertified iterations without modifying the caller's
published state.

The Newton update eliminates each local wiper, exploits the six directed
physical receiver-feedback entries, and solves a three-rail Schur system.
There must be no allocation, lock, dense 94-by-94 factorization or unbounded
iteration in this candidate. A hard iteration/step cap is not a deadline proof.

The separately callable residual gate evaluates all node KCL equations, not the
previous map's affine closure. It includes diode-current evaluation error and
floating arithmetic through conservative enclosures before applying a positive
row-dominance margin. Ordinary libm approximations may propose a diode current,
but cannot independently justify its enclosure. State the arithmetic and graph
assumptions explicitly; fail closed when they are not met.

The candidate graph uses prepared binary64 coefficients as exact constants.
Any distinction between that graph and the dense reference's floating evaluation
must be disclosed and checked, not hidden in an ideal-real arithmetic claim.
The preregistered voltage/iteration limits are in
`tests/parity/tolerances-v2/memory-sparse-update.json`. Compare complete nodes
against the unchanged dense solver, including the prior selected-memory
counterexample, both ranks, all selectors, alternate electrical assumptions,
endpoints, unequal banks, cold/warm starts and rapid changes. Independently
challenge the residual gate with perturbed nodes, NaN/overflow, bad feedback,
stale configuration, exhausted work budgets and mutations of the update path.

Benchmark cold and warm source updates separately from residual verification,
with all 94 output nodes consumed. Report complete equivalent outputs and
failure counts, not partial work as a replacement speedup. Record compiler,
source hashes and trial ranges. Shared-machine time is neither WCET nor whole-
instrument performance. Scheduling, publication, fallback behavior, receiver
readout cost and 16-voice topology remain outside this increment.

## Implemented boundary and proof obligations

`MemorySparseCompiler.h` prepares fixed coefficients without a nonlinear solve.
The full key compares the caller generation, sparse/source/boundary versions,
source-table fingerprint, rank, selector, all 78 bank positions, all nine
numerical electrical inputs and 26 background injections. It is not merely a
hash. `Prepared` is immutable after compilation by contract. Neither it nor
`State` is a public preset/session format. Never manually modify coefficients
while retaining the old key.

The certified graph treats the prepared binary64 conductances, follower ratios,
loads, diode parameters and input currents as exact real constants. This is a
precise numerical-model claim, not a component-tolerance or hardware claim.
Dense-reference floating evaluation differs slightly in operation ordering;
complete nodes are compared separately. Receiver-drive compilation itself is
not an interval evaluation of the original resistor expressions.

Only the six physical nonnegative feedback edges are admitted: rows 7/9 receive
rows 23/25, and row 22 receives rows 24/26. The source and destination sets are
disjoint. Thus after the 66 independent wipers are eliminated, the bus operator
`D - L` has the exact inverse

```
(D-L)^-1 = D^-1 + D^-1 L D^-1,
```

because `L D^-1 L = 0`. Applying that inverse to the bus RHS and three rail
columns leaves a 3-by-3 rail system. Back-substitution recovers every bus and
wiper; the inactive emitter retains its separate 1000-siemens zero constraint.
Follower derivatives are intentionally nonsymmetric. No source load is added
twice. A different topology needs a different explicitly validated solver.

For the prepared exact-real graph, `K = -dF/dV` has nonpositive off-diagonal
entries. Passive branch conductances cancel from row dominance; follower terms
leave a nonnegative `g*(1-ratio)` remainder. The independently accumulated
certificate uses the conservative margin

```
m = min(driverG, 1000,
        all unbuffered wiper bottomG,
        each bus load - sum(outgoing receiver-feedback coefficients in its row)).
```

The final expression is outward evaluated and its positive lower endpoint is
required. The same margin holds along any candidate-to-root segment, so the
mean Jacobian is strictly row diagonally dominant and its inverse infinity norm
is at most `1/m`. The continuous diode law with positive series resistance is
defined for every real voltage; the uniform margin also makes the map proper
and nonsingular, supporting a unique root for finite forcing. Consequently

```
max_node |candidate - exact_prepared_graph_root|
    <= max_node |true_KCL_residual| / m.
```

The numerator is an interval enclosure of **all 94** node equations. The
[verified diode primitive](memory-verified-diode.md) bounds the logarithm with
an outward-rounded rational series and analytic tail; no assumption about
`std::exp`/`std::log` accuracy is used for acceptance. Ordinary libm only proposes
Newton candidates. A per-call verified reverse-current shortcut avoids repeated
logarithms: for `-Is < i_hi < 0` and `R <= Rmax`, a lower enclosure of
`Rmax*i_hi + nVt*log((Is+i_hi)/Is)` is a sufficient voltage threshold for current
in `[-Is,i_hi]`. Its width, domain and threshold are rechecked each call.
The complete summed residual, not that shortcut or a single branch width,
decides whether the final requested voltage bound is met.

This floating-point certificate is conditional on strict IEEE binary64,
round-to-nearest, gradual underflow and masked FP traps. In that supported
trap environment, incompatible rounding/denormal modes are rejected. Masked FP
traps are a caller precondition, not a detected property: unmasked traps can
interrupt arithmetic checks before a normal rejection is returned. The functions
do not change those modes, but may set exception flags.
The independent proof does not certify arbitrary compiler transformations or
future consumers that omit the arithmetic checks. CMake disables contraction
on the standalone test/probe targets (MSVC uses strict FP).

`update` admits at most 64 Newton steps, each limited to 0.5 V per node. It
copies a complete warm state before working, so an in-place caller alias is
safe. Every success must pass `certify`; invalid input/configuration, stale
identity, nonfinite/overflowing arithmetic, unattainable enclosure width,
singular elimination or exhausted work limits leave the destination unchanged.
There is no "publish the last iterate" fallback. A zero-step request may accept
an already certified state, not an arbitrary cold guess.

A returned certificate applies only to the exact graph, inputs and candidate
supplied for that call. A new pressure/envelope/input event invalidates any
assumption that yesterday's successful certificate covers today's boundary.
This is not an asynchronous frame-publication protocol or an Engine fallback.

## Results on the frozen research source

The source evaluated here is local commit `5178f93`; portable file identities
are in the [manifest](memory-sparse-study/manifest.json). The preregistered
limits file is unchanged, SHA-256
`1370d58617cc566838e733fc5b16780a2314393051ca7138c54ab18093c4c5cf`.

### Accuracy and failure behavior

The full grid is 2 ranks x 3 selectors x 3 pot values (10/50/100 kOhm) x 7 bank
shapes x 4 declared electrical models x 4 receiver-drive patterns: **2,016
cases**. It includes uniform wiper endpoints/quarters and two unequal-bank
patterns, both signs of envelope input, pressure and keyboard terms, nominal
and forward-active drivers, and low/high-drop diode alternatives. This is a
sampled declared-model grid, not every possible valid coefficient combination.

- **533,518 checks, zero failures**, including every one of the 94 nodes.
- Maximum sparse-versus-dense node difference: **1.56923 nV**, below the
  preregistered 20 nV comparison limit.
- Maximum accepted full-node certificate: **6.01681 nV**, below the unchanged
  10 nV requested bound. Both sparse and dense states are separately checked
  against the prepared graph; every observed difference lies within the sum
  of their two returned bounds.
- At most **28 cold / 6 warm Newton steps** on the grid, within the hard 64-step
  limit. These counts do not establish a continuous convergence guarantee.
- Zero observed `new`/`delete` allocations during cold/warm update and its
  certificate, alongside fixed-array/no-lock source review.
- The prior valid-float-control, selected-memory counterexample's **1.15896 mV**
  affine bus error falls to **0.753804 nV** with the nonlinear sparse update;
  its full-node bound is 3.16182 nV.

Every rail, wiper, inactive-emitter coordinate and bus is challenged with both
signs of a 1 mV perturbation in six fixtures. Invalid/nonfinite/digital inputs,
negative or dominance-breaking feedback, stale same-generation wipers,
generation mismatches, bad budgets, overflow, and exhausted/zero work limits
are rejected without overwriting the prior state. The already-converged
zero-step case and in-place warm-output alias are also tested.

Two source mutations prove that the checks are not merely finiteness tests:
zeroing the full-node residual causes **650 failed assertions**; omitting
receiver feedback from the Newton residual causes **9**. The runner requires
both a failing exit and a named behavioral assertion, not a compile error or
crash. The diode primitive separately passes its exact-rational/high-precision
oracles and detects four independent faults; see its
[proof and execution report](memory-verified-diode.md).

The optional dense `NodeSnapshot` does not alter default solver arithmetic.
A 36-case dump of bus/rail/full-Jacobian output is byte-identical to the earlier
reference: 2,553,120 bytes, SHA-256
`65d2fe8ae8df038d67b6d255372f7f306efb4e5464b2bb74f81f820f06743ca0`.
There are no Engine or plugin includes of this experimental boundary.

Verification records: [full grid](memory-sparse-study/sparse-checks.txt),
[ASan/UBSan/float-cast-overflow and assertions](memory-sparse-study/sanitizer-checks.txt),
[12 focused CTests](memory-sparse-study/ctest.txt),
[negative controls](memory-sparse-study/negative-controls.txt), and
[primitive profiles/fault injections](memory-sparse-study/primitive-checks.txt).
LeakSanitizer was disabled for the sandbox; no leak qualification is claimed.
A separate source review reproduced the full grid and both source mutations,
checking the Schur signs, all-node residual, margin, identity and publication
contracts. This approval is for the offline numerical boundary only.

### Cost: improved offline solve, still rejected for the proposed audio schedule

Final GCC 14.2 / Linux x86-64 timing uses `-O2 -ffp-contract=off`, seven trials in
alternating scenario order, 32 dense calls or 128 calls for the other scenarios
per trial. Dense calls sample only indices 0..31 of the precomputed 128-frame
input sequence, whereas sparse cold/warm calls sample indices 0..127. Their
sampled input distributions are therefore not identical: this table is **not a
paired per-input speedup estimate**. All 94 nodes and diagnostic result fields
are materialized, not only a checksum or selected receiver fields. Median times below are microseconds
per rank update. Parentheses give the range of **trial means**, not per-call
latency or WCET.

| Scenario | 50 kOhm, unequal banks | 100 kOhm, full travel | 100 kOhm, unequal banks |
| --- | ---: | ---: | ---: |
| Dense cold full-node solve | 5,548.8 (5,397.2–6,019.5) | 10,561.0 (9,834.9–11,414.8) | 5,668.5 (5,415.6–5,877.3) |
| Sparse cold solve + certificate | 994.6 (965.7–998.1) | 612.6 (578.3–703.9) | 1,128.9 (1,097.2–1,217.3) |
| Sparse warm update + certificate | 403.1 (382.2–438.9) | 192.4 (184.5–199.7) | 475.2 (455.5–506.2) |
| Certificate only, fixed valid state | 311.0 (308.5–346.3) | 129.2 (125.4–169.6) | 417.0 (395.6–451.9) |
| Fixed coefficient preparation | 11.0 (10.5–15.6) | 11.4 (10.6–24.3) | 11.0 (10.6–11.9) |

All 105 scenario/trial rows had zero rejected evaluations; the
[raw CSV](memory-sparse-study/cpu.csv) retains every value. The dense routine
assembles its fixed coefficients internally on every call. The sparse graph is
prepared beforehand, with that cost reported separately. Warm updates reuse
the preceding state and are not a like-for-like cold-start comparison. Inputs
vary over a precomputed 128-frame sequence with master-control steps; this is
not an executed audio/scheduling path. The isolated certificate scenario holds
its complete state and boundary fixed.

These are shared-machine wall-clock measurements. Neither the apparent speed
improvement over the dense research oracle nor bounded loop counts constitute
real-time qualification. Even the warm **trial averages exceed** the 83.3 us
period of the earlier 8-sample/96-kHz candidate for one rank. Two ranks at that
rate project roughly 4.62–11.40 seconds of serial measured work per second of
audio, before receiver decoding or the instrument. That is a workload
projection, not measured hardware CPU utilization. **Do not activate this path
in the Engine.**

### Next bounded step

The full-node certificate is now an independently testable oracle/gate for
future approximations; it is not yet an affordable per-update runtime path.
Profile and reduce verified residual cost without removing outward arithmetic,
positive tails, branch loads or the final error bound. A useful next experiment
is a separately proved reusable operating-region enclosure that can amortize
validation across a bounded input box, or a cheaper verified branch enclosure,
with the current dense and certified sparse paths as independent checks.

Any region reuse must cover configuration, receiver-current/feedback changes
and finite expiry; source/input events outside its proved domain must reject
or refresh, never quietly extrapolate. Then revisit combined nonlinear and
causal scheduling budgets, asynchronous publication and a genuinely qualified
fallback. Eight-versus-sixteen-voice loading, full receiver/audio propagation,
whole-instrument cost and native host continuity remain separate unmet gates.
