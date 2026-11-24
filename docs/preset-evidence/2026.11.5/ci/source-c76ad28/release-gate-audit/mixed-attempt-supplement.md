# A58 mixed-attempt release-gate supplement

**The frozen all-attempt-1 preparation is preserved. Final native qualification must use each selected job's actual attempt. No replacement ARM job or platform acceptance is asserted by this supplement.**

The [machine-readable contract](mixed-attempt-supplement.json) narrowly supersedes the native attempt assumption in [the original requirements](gate-requirements.json). It retains every other native, supporting-workflow, payload, audition and publication gate. DSP remains its separate c76 run/attempt; earlier filter and screenshot executions keep their original source identities.

## Selection and parser composition

| Platform | Native producer selection | Validator context |
|---|---|---|
| Windows | Original job 138299557166, attempt 1; acceptance pending | Independently bind the actual separate consumer job and attempt. |
| Linux | Original job 138299557138, attempt 1; acceptance pending | Independently bind the actual separate consumer job and attempt. |
| Intel Mac | Original job 138299557267, attempt 1; acceptance pending | All four lanes and packaging must belong to that job. |
| Apple Silicon | Future replacement job and actual attempt are unknown; attempt 2 is anticipated only | Require all four lanes and packaging from the independently qualified replacement job. |

The unchanged native parser merely copies caller provenance into `caller_provenance_claims_unverified`. Its `--require-complete` switch checks target coverage, not attempt provenance. Aggregate four platform-local raw outputs externally, retaining their input-manifest and parser hashes and actual producer/consumer mappings. Do not apply a single global attempt 2 to this evidence. A58's archive checker also treats `attempt_context` as a supplied context; original job/upload gates supply the proof.

Every selected artifact needs its actual ZIP/API identity and original producing upload ID, size, digest and timing. Same-name artifacts and current run metadata cannot supply the producing attempt. Windows/Linux consumers must have the exact selected producer's VST3 receipt/digest/completion, regardless of whether their attempt equals the producer's. All source-pinned helper gates and A58 F1/F2 closures remain required; their raw false qualification flags stay unchanged.

## Preserved failure and correct totals

[The original ARM failure](../macos-arm64/attempt-1-failed/failure-summary.json) records 264 passed of 266 native registrations, two formal skips, five nested skips, and three validator invocations: two headless successes and the failed AU GUI invocation. The fixed-seed lane and packaging never executed. Keep those historical counts separate from selected release totals; do not borrow the two successes for the replacement job. The native-only parser's green requested-observation flag includes no validators and cannot qualify this failed platform.

The selected release requires 13 final validator lanes and the complete native rosters, with 1,378 registrations expected across four platforms. Those are requirements, not observed accepted totals. Any eligible Linux runtime retries add actual invocations within their lanes. A new GitHub ARM job execution is a distinct category; the rejected rerun request created no execution.

The [retained original connector response](../rerun-request-while-running.json) explicitly reports HTTP 403 because the containing workflow was still running. Its exact request time is unknown; its retention time is separately recorded. This was a GitHub run-state restriction, not an approval denial. A clean rerun's new random seed will not deterministically reproduce seed `0x3c99748`; the required fixed seed `0x2b4eef5` remains mandatory. The driver-named assertion and reported process exit 9 do not establish cause ownership.

## Format correction and preservation checks

I compared the retained before/after A50 failed-attempt inspectors and A51 qualifiers. Each changes only the exact terminal-line predicate to accept both `100% tests passed, 0 tests failed out of 266` and the actually observed `100% tests passed out of 266`. JUnit/LastTest roster and outcome checks remain intact. The frozen ARM qualifier still has its original phrase: create a fresh replacement-attempt binder with the narrow accommodation, its actual job/attempt, and original successful Test-step binding.

Independent bounded verification confirmed all eight original A58 checkpoint files and all 28 failed-ARM retained manifest files. Both base64 archives decode to the exact API sizes and SHA-256 digests; all 11 members match their recorded lengths, hashes, CRC values and rosters. No product tests were run or source changes made. The [supplement integrity record](mixed-attempt-supplement-integrity.json) retains the new files separately.

Final actual gate closure remains pending. The coordinator reports only unreferenced evidence blobs have been uploaded; this does not establish a final tree, commit, published download or main update. Those outcomes require separate final verification.
