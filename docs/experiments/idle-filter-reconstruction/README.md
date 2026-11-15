# Fresh reconstruction of bounded idle-filter convergence recovery

This is new work based on published source 28999229df3662d8cf032e6dc38a0b12403aca06.
It is not recovery of the missing local 4695707d implementation or 594-test
qualification. The separately recovered staged tree is mixed and is not merged.

The fresh reset-origin diagnostic covered 432 cases and 1,769,472 samples,
with exact equality of three outputs and twelve named memory fields between
baseline and diagnostic instrumentation. No nonfinite results. Capped solves
above residual tolerance: 6,420 unwarped 3 kHz; 740 warped 88.2 kHz; 631 warped
96 kHz. Only the first 256 per clock were retained; omitted counts are explicit.
Independent bisection solved all 768 captured equations at residual <1e-7.
This is a bounded stress experiment, not ordinary musical failure frequency,
hardware fidelity, or proof about all omitted failures.

Production repair is deliberately limited to lastUnwarped idle steps. Original
Newton arithmetic is retained until a finite capped failure. A caller-local
noinline helper uses finite guards, bounded bracket expansion and 64 bisections.
Only strict actual residual <1e-7 accepts a complete tuple; clamp discontinuities
make width-only convergence invalid. Failure leaves the original tuple intact.
No allocation, lock or global mutable state is added. Warped scalar and SIMD
production arithmetic are unchanged; their observed convergence gap remains open.

Generic and sanitized trajectory runs cover 1,769,472 samples with finite
outputs/memory and unchanged warped output/memory bits. Idle trajectories change;
finite continuation is not an accuracy or global stability proof. Leak checking
was disabled. Helper standalone inclusion and reciprocal finite guard were fixed
following independent review and all three fixture build modes rerun.

## Native compiler-context finding, preserved rather than erased

Original candidate native registered run: 8/9 tests passed, exit 8. Two exact
state assertions in offline reference controlledReferenceApi failed; convergence
certification passed. Unchanged baseline under identical native flags passed.
Default Release candidate nodal test passed. This is not a pre-existing baseline
failure. Three noinline runtime API barriers in the test harness prevent caller
specialization from differing across the compared APIs. Exact state comparisons,
certification and tolerances remain unchanged. Revised baseline: 121 checks, zero
failures; revised candidate native suite: 9/9 passed, 64.19 s. Precise FMA causation
is not established. Initial manual invocations with wrong tolerance-file cwd are
retained separately and are not numerical failures. Original binaries retained
locally; hashes accompany archived logs. No production/reference arithmetic was
changed to resolve the harness issue.

## Paired whole-engine diagnostic cost

Three alternating baseline/candidate pairs, unchanged benchmark, 48 kHz/256 frames,
1024 held callbacks, performance suite: each run39 cases/100 phase rows. All six
runs exited0; paired keys and checksum strings match, nonfinite0. Candidate
metadata names its base commit, not an immutable candidate commit; frozen binary
and production header hashes identify the actual tested inputs.

Median per-case candidate/baseline held-time ratios across33 Source Performance
cases: 1.018496, 1.018602, 0.998120. Release ratios: 1.022913, 1.021865, 0.999337.
Across39 held cases including six anchors, pair2 is1.015310; do not conflate these
aggregations. Two rounds show modest overhead and one approximately neutral.
This is a correctness repair, not a speedup or zero-cost claim. Shared-host timing
and tail outliers do not establish audio-driver dropouts or realtime readiness.
No compilation was deliberately run during the measured pairs.

Qualification is core-only. GUI, external DAW, plugin binaries, platform builds,
and broad full-suite coverage have not been re-established on this new lineage.

## Final expanded regression checkpoint

The permanent fixture enforces exactly768 finite, schema-validated records and
256 per clock, rejects missing/truncated input with exit2, and verifies named
fields instead of struct padding. It checks accepted tuple consistency, tiny
positive-level reciprocal rejection, unchanged failure output, idle-stage repair,
warped-stage preservation and finite memory during continuation/re-reference.
Augmented generic/native/ASan+UBSan builds each passed768 captures,256 idle
repairs and512 warped comparisons. The final registered native run passed
12/12 tests, exit0,64.92seconds, including missing/truncated input negatives.
A missing executable was separately verified to fail the rejection wrapper, not
produce a false pass. Independent review audited the revised harness, benchmark
checksums/hashes and these final results. No universal native-caller exactness,
audio-fidelity or full-product qualification follows from these scoped checks.
