# Experimental audio-rate filter convergence recovery

Base: qualified idle-only reconstruction6f71b0a443d83290ad767a9c9c6f5b732ab5dee2.
This experimental extension is NOT authorized by its evidence as realtime-ready
or an improvement in hardware fidelity. The idle-only publisher lineage is
separate. Production extension removes the unwarped-only scalar guard. At the
vector iteration cap, any still-active lane not passing actual residual tolerance
rejects the whole batch before any state/output write. Existing wrapper then
recomputes all four lanes through authoritative scalar arithmetic.

Fresh evidence from the preceding baseline diagnostic retained256 failures per
clock, including512 warped failures. All768 stage repairs pass strict original
residual<1e-7; generic/native/ASan+UBSan tests pass. The broader432-case1,769,472
sample trajectory has no nonfinite output/memory, with exact baseline agreement
before first capped solve in each trajectory. Repaired trajectories differ.
Native and generic cap census counts differ because compiler contracts differ;
these are not combined as one census. No leak qualification (detect_leaks=0).

Independent SIMD tests verified2048 genuinely eligible captured failures across
four lane positions, atomic rejection, scalar/wrapper equality, mixed/multiple
failures, duplicates, unsupported inputs and continuations. Observed and untouched
production backends passed648362checks. Permanent production-backend test passed
648358checks generic/sanitized. Unsupported native-FMA or disabled backends return
explicit skip77; they are NOT accelerated-path qualification. Input negatives
still reject before skip.3907 fifth-evaluation successes remained accelerated.

Five native core integration tests passed53.93s; three accelerated integration
tests passed11.55s. Final combined AVX2-enabled run passed15/15 in71.12s. An initial
build invocation missing newly registered targets was retained, followed by
regeneration; no tolerance or timeout changes. No GUI/DAW/plugin/full-suite or
cross-platform qualification has been performed on this experimental extension.

## Ordinary whole-engine timing
Three alternating native baseline/candidate pairs,48kHz/256frames/1024callbacks,
39cases/100rows each. All completedexit0 with matching keyed checksums and zero
nonfinite.33SourcePerformance held median per-case ratios .972933/.990551/1.075662;
release .990089/.997737/1.009210. Mixed results, not a consistent speedup. Candidate
metadata uses basecommit; frozen binaries and source hashes identify actual inputs.

## Failure-heavy cost: material overhead remains
Synthetic captured-state chunks restore state outside timing.31rounds pervariant,
512four-stage groups perchunk, one failure in each lane position, four failures,
and converging controls. Untimed eligibility/atomicity/parity checks precede timing.
Initial measurements overlapping other work are retained as provisional and not
used. Rerun has no planned concurrent build/test workload. Timed checksums match
between repaired scalar and wrapper; baseline accepting wrong roots intentionally
differs. This is NOT an absolute worst-case bound or whole-engine real-time test.

Single capped-lane median chunk-average cost ~1.16–1.75us repairedscalar vs
.168–.178us baseline; wrapper ~1.52–1.82us. Four cappedlanes: baseline .421us,
repairedscalar3.493us, wrapper3.781us pergroup. Paired wrapper/scalar medianratios
~1.21–1.34 across cappedscenarios. A separate untimed instrumented-helper audit
found27–37 extra equationevaluations,median35, with all512tuples bit-identical to
productionhelper. Chunk-average p99/max are NOT individual-call p99/max or WCET.

Conclusion: correctness evidence supports continued investigation, but rare-path
cost is significant. Keep this extension experimental while evaluating lower-cost
safeguarded root strategies; do not present it as a performance optimization.

Synthetic baseline uses the frozen pre-recovery stage header, not an engine
build. Four-failure worst observed512-group chunks:1.270ms baseline,4.325ms
scalar,5.123ms wrapper; substantial scheduling outliers preclude WCET claims.
