# A46 independent review: serial sanitizer shards

**Decision: approve the bounded CI scheduling change.** All 29 independent controls have the expected outcome on the final helper. The review is frozen. This is a review of the workflow and its evidence checks; it does not qualify a product binary, sanitizer run, native build, or release.

The source before this change is `ef1c472b161c37c490945f7c469086508aae6aef`. A33's reviewed helper has SHA-256 `b9dfbaefe08c80470c0d49845bfb0103223bf545385dd04c8fe562e2a7e2bbfb` and Git blob `3e9098ffe65a4aec63ed3649028888f0e36e8d5c`. The patch has SHA-256 `2aae609a8c3441ebe263dbf779048554ecb8ba0ce40ad195ba96480353f10994` and Git blob `82907b175fae341817dd76e7f013f92faec5ecd4`. Complete reviewed input hashes are in `review-receipt.json`.

## Preserved contract

The configured source roster contains 232 unique tests, all marked `RUN_SERIAL`. Its registration-order partitions contain 78, 77, and 77 tests, with a complete union and no overlap. `workflow-partition-review.json` records every name and partition. Three separate Ubuntu runners execute these partitions with explicit CTest parallelism of one and matrix `fail-fast: false`.

The configure and build steps, sanitizer environment, 75-minute job limit, timeout scale 4, existing workflow concurrency, and other existing jobs are unchanged. Address and undefined-behavior sanitizers, leak detection, and halt-on-error behavior remain enabled. The change does not patch native `build.yml` or CMake registrations. Both existing capability skip declarations remain intact: `cs80_filter_batch_cap_tests` and `cs80_stereo_pra_exact_tests`.

Each shard must preserve the original child return status, source/run/attempt identity, exact selected names, JUnit, LastTest, and file hashes. The always-running aggregate requires all three jobs to succeed and verifies those records. An always-running upload also includes the original build-directory LastTest directly, so cancellation evidence does not depend on Python reaching its copy step. Current source tests have no fixture or dependency properties; future additions require explicit review instead of silently enlarging a shard.

## Independent execution and corrections

`probe_shards.py` ran a separate, inert 232-test CTest project. Its three shards completed with 231 passes and one declared skip. A real failing test produced original CTest exit 8 and failed the helper. A fixture dependency experiment showed CTest expanding a 77-test selection to 79; the helper rejected that dependency before execution. These are actual helper executions with synthetic test names and synthetic CI identity, not product sanitizer evidence.

The fixed 29-control set covers missing or duplicate cases, invalid shard indices, nonzero or signaled execution, undeclared or malformed skips, failed tests, and incomplete logs. An initial unknown JUnit wrapper finding was fixed. The final four findings were nested skip and failure markers, a duplicated complete LastTest session prefix, and a missing output opener. All now reject the malformed input. Final JUnit checks enforce direct cases and descendant status markers; LastTest checks enforce one complete session, the expected global indices, and per-test output and terminal framing. Draft, revised, and final results and helper snapshots preserve this progression. A33 separately reports 20 passing selftests.

## Scope of remaining qualification

The previous sanitizer run `45439302238`, job `138605852686`, was cancelled at 75 minutes: 173 cases completed (172 passed and one declared skip), while case 174 was running. It did not finish normally. Its complete original log is Git blob `bf57f4ee42a866fa0c16fba3849e9bd951d92cc3`; its bytes and SHA-256 are recorded in the receipt.

The new source still requires the three actual sanitizer shards and successful aggregate, plus all four original native targets. Earlier source evidence cannot satisfy those new-source requirements.

The optional read-only coverage crosscheck independently reproduces 31 changed vectors since revision .7, 28 mechanism programs, 15 overlap, and 44 unique programs. Against revision .4 it reproduces 521 musical programs with changed old nonmaster controls, comprising 493 with further changes and 28 with base-VCA-only changes. Both Init programs remain unchanged. This compares binary32 parameter values and declared coverage; it makes no audio-quality claim.
