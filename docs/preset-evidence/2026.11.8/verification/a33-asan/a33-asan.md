# A33 follow-up — Isolated serial sanitizer shards

Source base: `ef1c472b161c37c490945f7c469086508aae6aef`. Canonical agent: `/root/a33_ci_scheduling_gate_review`; reused worker, no new agents. Root owns integration, commits and fresh CI. This patch concerns supporting ASan/UBSan CI, not native qualification or product sound.

## Deliverable

`a33-asan/sanitizer-shards.patch` changes exactly three paths: `.github/workflows/dsp-measurements.yml`, new `scripts/ci-sanitizer-shards.py`, and new `scripts/test-ci-sanitizer-shards.py`. Root's sparse checkout omits `.github`; patch validation therefore used `git apply --check --cached`, successfully. No tracked files, index, refs or shared build were changed by this worker.

The patch leaves native `build.yml`, root CMake, scheduling policy, product code, existing assertions, per-test timeouts, sanitizer compiler/linker flags, leak detection, UBSan runtime options, timeout scale 4, build parallelism 4 and each sanitizer job's 75-minute budget unchanged. The other four supporting DSP jobs and compatibility pins are unchanged. Added both exact helper paths to `push.paths` so future helper changes trigger qualification.

## Original failure evidence

Read original complete job log from immutable Git blob `bf57f4ee42a866fa0c16fba3849e9bd951d92cc3`. It contains 173 completed cases: **172 passed and one declared stereo-PRA skip**, with 3,922.21 seconds of completed passing-test time. Cancellation occurs while test 174, `cs80_plucked_material_preset_tests`, is active. No failing assertion, ASan/UBSan diagnostic, or failed completed case appears. Source-factory integration alone took 1,223.62 seconds. This supports a total scheduling-budget problem, not a sanitizer pass claim.

## Scheduling and coverage contract

Three separate Ubuntu runners use matrix indices 1–3 with `fail-fast: false`. Each runner configures/builds independently and runs one CTest invocation with explicit `--parallel 1`; there is no retry or within-runner test concurrency. A private configure-only audit reproduces **232 unique tests**, partitioned by CTest registration index as `1,232,3`, `2,232,3`, `3,232,3`: **78 / 77 / 77** names.

Plans retain the full CTest JSON roster, every name/property, all three partitions, selected names, CMake cache, and source/run/attempt identity before building. Before execution, another full roster and selected show-only roster must match exactly. Fixture/dependency additions, disabled tests, changed flags, missing/duplicate names and out-of-range indices fail closed. No post-run CTest inspection can overwrite `LastTest.log`.

Each original invocation records its actual return code, completion state, exact command/environment, JUnit, console log, LastTest, and evidence hashes. Original attempts cannot be overwritten. Always-upload includes the **raw build-directory LastTest path directly**, in addition to the helper's evidence folder, so cancellation before Python finishes its receipt/copy can still retain the original log when Actions reaches upload.

The required `sanitizers-complete` job runs after the matrix, even on failure. It requires the matrix result `success`, all three original normal-zero receipts from the same source/run/attempt, exact matching full rosters, correct distinct shard indices, and an exact 232-name union without overlaps/omissions. It validates direct JUnit test cases, suite counts/status, declared skips, and complete per-test LastTest start/output/result/end/time framing plus final footer. A missing/partial artifact, nonzero exit, signal, duplicate, new undeclared skip, failed/cancelled job or malformed report fails qualification. Aggregate decision evidence is always uploaded.

Existing skip policies are preserved: the configured roster declares return-code 77 for `cs80_filter_batch_cap_tests` and `cs80_stereo_pra_exact_tests`. Reports distinguish passed from skipped; they do not call all 232 passed when a declared skip occurs.

## Verification and limits

**20/20 bounded self-tests pass**, including real CTest execution of 232 inert fixture cases across all three shards and negative coverage for failure/signal/cancellation, missing/duplicate artifacts, invalid indices, altered identity/flags/scale, fixture/dependency/disabled registrations, malformed/duplicate/omitted JUnit, undeclared skips, and truncated per-test log framing. They run as a workflow step; no new CTest registration changes the 232-product-test contract. A46 independently reviews the patch and maintains its own fault probes. Its discovered nested-JUnit marker/wrapper and incomplete log framing counterexamples were fixed and added to the retained self-tests.

Observed historical durations mapped by current names put library audit on shard 1, pad/poly refinement on shard 2, and source-factory integration on shard 3. Using current observed times where available and previous observations otherwise gives partial sums 2,785 / 2,021 / 2,349 seconds, excluding two unmeasured new tests and build time. These numbers justify separating the largest workloads; they are **not runtime predictions or a guarantee** that any shard fits 75 minutes. The fresh original CI run remains necessary.

Evidence: `a33-asan/proof-packet.json` contains all 232 partition names and exact source/log bindings; `workflow-preservation.json` checks unchanged workflow contracts; `self-tests.log` retains the final test output; `fixture-proof/` retains inert CTest example evidence; `configured-plans/` records the actual source's configure-only plans, explicitly using local fixture run ID 1 rather than claiming a GitHub run. Immutable artifact blob mapping is recorded separately. No full sanitizer/native product suite was executed locally.
