# A60 — independent release review, preliminary checkpoint

## Decision

**No new blocking source or evidence defect was found in the completed scopes. Overall release acceptance remains pending.** The three completed installation/native suites, ten original validator lanes, and supporting outcomes are internally consistent and appropriately scoped. This checkpoint must not be used as final release qualification.

Reviewed binary source: `078c0981ffa1b5472342e0fb5ab8702bc3bd2889`, tree `d84661c6c465c53ff41361e8aba0cba5e4a53397`. Current main documentation checkpoint: `6426abaa2578a72d9c4df489828037fc9db8eece`. Original native run: **45246863788**, attempt **1**.

A60 performed read-only source/evidence review and wrote scratch reports only. No native test, validator, audio render, synthetic fixture, verifier self-test, source edit, workflow edit, Git mutation, rerun or external write occurred.

## Independent checks performed

I independently rehashed the **nine actual original ZIPs** for the completed platforms: installation, native-test evidence and pluginval reports for Windows, Apple Silicon and Intel Mac. All byte counts and SHA-256 digests agree with their retained individual artifact API objects, expected source/run and nonexpired state. These are checks of the downloaded evidence bytes, not a new public-download-route check.

I independently parsed actual `ctest-results.xml` from each native-test ZIP, checked unique testcase names and explicit result elements against the JUnit headers, and reconciled the results with the retained parser observations. Nested Python skips were independently recounted from terminal `OK (skipped=N)` summaries in the complete `LastTest.log`, separately from CTest executions.

| Completed target | Passed / registered | Formal CTest skips | Separate nested Python skips |
|---|---:|---:|---:|
| Windows x86_64 | 227 / 229 | 2 | 11 |
| macOS arm64 | 249 / 251 | 2 | 5 |
| macOS x86_64 | 249 / 251 | 2 | 5 |
| **Completed subtotal only** | **725 / 731** | **6** | **21** |

There are zero failed, disabled or unknown CTest results in this completed subtotal. Each target's formal skips are `cs80_filter_batch_cap_tests` and `cs80_stereo_pra_exact_tests`, with `SKIP_RETURN_CODE=77`. Original producer job metadata identifies the correct source, run and attempt and a successful original Test step. No Linux counts are inferred.

All **239 committed paths changed after the binary source** are README/documentation/evidence/screenshots. No committed or tracked working-tree production, test, script or workflow difference from the tested source was found. Thirteen specifically reviewed runtime, workflow, capture and verifier files also match the tested Git bytes directly. The scratch observation report records the exact paths, byte counts and hashes.

Primary independent record: `a60/preliminary-observations.json`. Existing source and programme-preservation review: `a43-review.md` and `a43-summary.json`.

## Completed evidence cross-review

**Installations and native suites.** A51, A52 and A54 bind each actual complete installation archive to its native producer and successful packaging/upload. The strict installation reports cover source/revision/run, exact embedded `SOURCE.txt`, complete inner inventory and checksums, JUCE 8.0.11 at the pinned clean commit, and all advertised native executable headers. A60 did not confuse those byte checks with independent execution of the binaries.

**Mac validators.** A55 establishes eight original strictness-10 lanes with 523 programs each, eight invocations and zero validation retries. Successful original steps and the exact unsuppressed `set -euo pipefail` pipelines supply normal-exit evidence; text `SUCCESS` alone is not the gate. AU executable identities agree before and after validation and with the distributed bytes, while source/registered full-bundle `diff -qr` checks preserve resource equality. VST3 uses the exact validated build path and source-bound ordered packaging. Six AU current-program warnings and two optional external-VST3-validator skip notices remain recorded. The ARM downloader's fallback transfer is not a validator retry.

**Windows validators.** A56's explicitly partial report establishes the two original headless strictness-10 lanes, two invocations and zero validation retries. The successful original steps are bound to immediate native `$LASTEXITCODE` propagation, so a crash after `SUCCESS` cannot be relabeled a pass. Full selected-bundle file, resource and topology identity agrees between actual validator input and distributable. The exact known `.exp`/`.lib` sidecars are separately scoped through the source upload and original consumer selection. The two optional external-VST3-validator skip notices remain distinct from the required pluginval lanes.

**Supporting outcomes.** A48's exact-source supporting DSP run has five successful original jobs and six actual qualified archives. The sanitizer suite records **199 passed / 200 registered, one formal skip and zero failures**, with no reported ASan/UBSan/LeakSanitizer diagnostics; nested Python skips are explicitly unknown in that suppressed-stdout run. A46's six filter jobs and 24 CTest executions belong to `bcc86aa`, with 91 relevant source inputs byte-identical to `078c098`; that executed source is not relabeled. A50's actual screenshot archive and original UI smoke pass; both golden images have equal hashes and zero changed pixels, and the preceding screenshot failure is preserved.

The retained current evidence is under `docs/preset-evidence/2026.11.4/ci/source-078c098/`, including `macos-validators/`, `windows-linux-validators/`, `supporting-dsp/`, `filter/` and `screenshots/`.

## Source, catalogue and claim boundaries

The current revision report and README correctly separate the prior `.4` reprogramming of all **521 musical programs** from the continuation's two model corrections. A43/A57 preserve all 523 ordered identities and addresses, both Init vectors and the old 198-core/304-host automation prefixes; the two size controls append at neutral defaults for old states. The continuation edits no factory parameter vectors, and its configured reach is 17 tine plus 51 bowed programs, a disjoint union of 68.

I inspected the continuation `InstrumentBody.h` diff. Its tine change uses the full-strike spectral bound without per-strike renormalization. Its bowed change applies the fixed body response at the moving harmonic frequency, preserves the previous absolute-mode bound, and keeps added work out of the sample modal-rotation loop. Existing direct-model, integration, parity, lifecycle and runtime evidence remains the appropriate support; I found no concrete new risk requiring another test run.

The all-521 matched spectral/envelope comparison remains explicitly historical `59d489d` evidence. Stationary bowed response tests compare the authored response, not a recorded violin. Source recipes and parameter changes are not mislabeled as listening tests, universal audible improvement, or exact real-instrument reproduction. The reported shared-machine runtime variation and added fixed storage are disclosed.

The current README still intentionally points to the separately qualified `.3` downloads and source. Its `.3` totals, artifacts, expiry and audition cannot be carried into the new release by inference.

## Genuine remaining gates

The following are release-blocking because their actual final evidence or final documents are still missing; none is a newly discovered source defect:

1. Original Linux native Test/package completion and actual installation/native-test evidence.
2. All three original Linux pluginval lanes, actual child termination/retry history and complete validated/distributed VST3 identity.
3. Actual final audition archive qualification, including API/producer/attempt identity, every archive entry, checksums, finite stereo float32 samples, measured frames/metrics and the delivered 523-by-200 inventory.
4. Completed actual end-stage worker outcomes, final 60-worker ledger/evidence manifests, A59 publication review and this A60 final all-gate review.
5. Root-owned aggregate qualification, all four actual platform links and audition publication, and remote verification of the reviewed source-equivalent documentation tree.

A58's source-preparation report is correctly pending. I reviewed its supplemental checker: it addresses the frozen audition helper's API/producer, all-entry metadata and stale-output limitations, compares every identity/parameter ID/binary32 value against the retained source-equivalent inventory, and requires the unchanged frozen helper to exit zero with matching result identity. **138 WAVs, 74 programs, 870 seconds, 173 regular files and 172 checksums remain expected values until the actual archive passes.**

The Linux source wrapper requires a final real normal exit zero and clean `SUCCESS`. It permits bounded retry only for status 139 after a completed test phase without `SUCCESS` or `FAILED`, while retaining failed-attempt logs. Status 139 alone does not prove SIGSEGV, crash ownership or whole-suite completion. Any original runtime retries must remain in final counts and history; diagnostics cannot substitute for original lanes.

## Documentation findings

**Resolved during review:** the scratch publication plan initially still listed sanitizer completion as pending. The parent updated it to `draft_pending_original_linux_audition_and_final_reviews` and removed that completed requirement; A60 directly read the correction. The retained CI status was already accurate. No open documentation nit or test rerun remains from this observation.

Preserve these clarifications in the final publication:

- The frozen new bundles include the source-time historical `DISTRIBUTION_STATUS.md`. Explain that the online final qualification supersedes that historical snapshot; changing online prose does not change packaged bytes.
- A52/A54 use historical handoff assignments in their authored reports. Final ownership is A55 for Mac validators and A56 for Windows/Linux. Retain history and use accurate final ledger mapping.
- The A57 58-worker snapshot is historical. The root now records 60 unique sequential real task paths, but six end-stage scopes are not fully complete at this observation. Do not count the earlier `.4` work-log roster, or describe 60 concurrent workers.
- Actual artifact acquisition through the connector does not prove an independently tested unauthenticated nightly.link route. These are retained CI bundles without publisher release signing or Apple notarization.
- Keep explicit **Library → FACTORY LIBRARY → LOAD / RELOAD**, followed by fresh notes, and the saved-state/DSP distinction.

## Publication mapping cross-review

A59's preliminary authored audit is now available at `a59/a59-review.md` and was read before this checkpoint closed. It independently agrees on all three actual installer IDs, outer sizes/hashes, manifests, `SOURCE.txt`, architectures, formats and pinned URL patterns. Linux and actual audition remain pending. The current known installation expiry converts to **21 February 2027 at 15:14:35 AST / 19:14:35 UTC**; final publication must use the earliest actual advertised-archive expiry after the remaining records arrive.

A59 records the existing distribution-history body after the old `.3` heading as **25,173 bytes**, SHA-256 `43a4c2827550c104a5570df9ac7381713854d8b906ed79928ccb3979ea1e720f`. Preserve that complete history unchanged when inserting the new current section. The parent has also added the frozen-bundle historical-guide clarification to the final publication plan.

## Final follow-up requirements

A60 must resume on the completed candidate, not turn this checkpoint into acceptance automatically. The final review will recompute four-platform native totals from actual reports; require all 13 original strictness-10 lanes and final normal exits; retain all formal, nested and validator-internal skips distinctly; check the four authentic install identities and Mac/Windows/Linux payload bindings; examine actual audition acceptance; retain supporting exact-source outcomes and failed/canceled history; and verify all 60 real worker outcomes and cited file paths.

The final README, distribution history, revision/team reports and aggregate must agree on source/run/revision, all four actual installer IDs and the actual audition URL, API expiry with correct AST conversion, honest access/signing/acoustic limits, and reload instructions. Final tracked runtime/test/script/workflow files must still match `078c098`; root owns the exact-tree main publication and remote check.

At this checkpoint A59's final all-gate report (its preliminary report was reviewed), the root aggregate `release-qualification.json`, completed final ledger, new team document and final four-platform publication candidate were not yet available. **`release_qualified` is false.**
