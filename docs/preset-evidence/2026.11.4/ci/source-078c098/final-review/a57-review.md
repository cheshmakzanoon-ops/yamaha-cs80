# A57 — final actual-60 roster and evidence audit

**All 60 continuation workers are actual recorded agents with unique assignments. A57's audit is complete. The ledger records 58 delivered scopes including this report; A59/A60 final delivery and root-owned final status closure are subsequent.**

Binary source: `078c0981ffa1b5472342e0fb5ab8702bc3bd2889`. Inspected documentation checkpoint: `df9a8d2994e96534eb7c4ee57c0a82ea1fb6916c`. Audit snapshot: `2026-11-23T23:07:12.376064+00:00`. This supersedes the earlier A57 58-worker checkpoint. No original CI/artifact gate remains pending in this audit; final reviewer delivery, status closure and publication are later steps.

## Roster provenance and completion

The actual roster contains exactly 60 sequential numbers and 60 unique task paths. The entire **53-row prior-stage .4 work-log** is excluded. Follow-up turns reuse agents and do not create extra workers. Concurrency is at most three workers alongside root, not 60 simultaneous workers.

Author reports or explicit root-attested conversational completion establish 57 completed assigned scopes before this A57 delivery; this report is the 58th. A59/A60 are actual agents with authored preliminary reports, and their final reviews are in progress. The captured root ledger has 57 literal `completed` rows; the normalized file preserves every original root status separately from the evidence-based current stage. Root will close final counts only after actual A59/A60 delivery.

**A02/A03/A04 remain root-attested conversational completions.** Their original standalone reports/transcripts are unavailable. A06/A09 corroborate A02's finding; A10/A32/A38/A43 corroborate A03; the committed release contract/history corroborate A04. Later corroboration is not represented as original author evidence. A10's retained report contains its original text plus a later raw-log-provenance addendum; both identities and the authorship distinction are recorded.

## Completed native, validator and actual-audition scopes

A53's final report accepts the actual Linux package and original native test evidence. A56 accepts all five original Windows/Linux validator lanes and both complete distributed VST3 payload bindings; its earlier Windows-only checkpoint remains historical. A58 accepts the actual audition archive. These outcomes replace their earlier waiting/partial stages without erasing them.

| Native target | Passed / registered | Formal CTest skips | Separate nested Python skips |
|---|---:|---:|---:|
| Windows x86_64 | 227 / 229 | 2 | 11 |
| macOS arm64 | 249 / 251 | 2 | 5 |
| macOS x86_64 | 249 / 251 | 2 | 5 |
| Linux x86_64 | 586 / 587 | 1 | 3 |
| **Total** | **1,311 / 1,318** | **7** | **24** |

All four native scopes have zero failed, disabled and unknown CTest results. Nested Python skips occur inside CTest executions and are not subtracted again. Linux's one formal skip is `cs80_stereo_pra_exact_tests` with return code 77. The supporting sanitizer run remains separate: **199 passed / 200 registered, one formal skip**; suppressed successful stdout leaves its nested Python skip count unknown.

A55's eight Mac lanes and A56's five Windows/Linux lanes total **13 qualified original strictness-10 lanes, 13 invocations and zero validation retries**, each with 523 programs. Original source/process/payload binding supports acceptance; a SUCCESS string alone is not the gate. Existing warnings and optional external SDK-validator notices remain recorded.

A58's actual ZIP has **173 regular files, 172 matching checksums, 138 WAVs / 74 programs / 870 seconds**, and **41,760,000 stereo frames / 83,520,000 scalar samples**. No nonfinite or above-full-scale samples were found. Its delivered inventory is byte-identical to the retained original export: all **523 identities, 200 parameter IDs and 104,600 binary32 values** match. This is actual archive acceptance, not just the earlier producer contract; it is not human listening or whole-catalogue WAV coverage.

## Retained bytes and source preservation

The committed **195-entry continuation manifest at df9a8d29 passes** every byte-count and SHA-256 check. The working manifest passes with no missing, duplicate, mismatched or unindexed files in its scope. The earlier **192-entry checkpoint at 6426abaa** also matches its recorded files. The retained current inventory/coverage preserve their original e32728b labels and leave historical development files intact.

The A53 **31-file** retention manifest, A58 **22-file** retention manifest, and supporting-DSP **13-file** index all pass their exact-byte checks. Sibling CI evidence is outside the continuation manifest. Root is retaining new final records and will create final scoped indexes after all reports and statuses are stable; no unavailable final index is claimed verified here.

All **300 committed paths changed after 078c098 through df9a8d29** are README/documentation/evidence/screenshots. No committed or tracked working-tree production, test, script or workflow change was found. Core/plugin/reference, tests, scripts, CMake, resources and workflows are unchanged from the binary source. Relevant export inputs are unchanged e32728b to 078c098.

Independent binary32 inventory comparison confirms **523 preserved ordered identities** with **521 musical programs** and two Init programs: **Init Patch (43)** and **Init Circuit (522)**. Both preserve their prior 198 values and append neutral sizes of 1. The 198 prior core IDs remain a prefix of 200. All 521 musical programs change 7–52 previous non-volume controls relative to .3; all 523 complete non-volume vectors are distinct. The 13 curated-family reviews cover 300 musical recipes; A24 covers 222 original addresses including one Init; structural preservation covers the remaining curated Init.

## Numerical and claim boundaries

| Claim | Verified scope |
|---|---|
| Configured continuation reach | 17 tine / 28 ranks + 51 bowed / 91 ranks; disjoint union 68 |
| Tine regression | 486 checks / zero failures; preceding model 162 failures |
| Bowed regression | 492 checks / zero failures; preceding model 174 failures |
| A06 parity | 10,137,600 binary32 bit comparisons; zero differences in retained summary |
| A10 parity | 36,115,200 numerically equal float samples; **not a bitwise assertion** |
| A09 parity | 45 matching FNV1a64 hashes; 4,320,000 float samples |
| A09 dynamics | 90 cases, three tine targets plus two controls; about 3.9–4.0 dB greater soft-to-firm body contrast |
| All-521 spectral/envelope comparison | All 24 source hashes match **historical 59d489d**, not a final-078 matched rerender |

These scopes are not added into a combined realism or listening score. A26/A28 lifecycle overlap remains explicit; truncated A31 stdout does not become an observed final assertion total. Runtime variance and added storage remain disclosed. Recipe vectors stay unchanged during the continuation DSP fixes; the prior .4 stage performed the wider revoicing.

Family reviews establish source/routing coherence and selected measured behavior. A08's cutoff proposals remain unshipped; bowed-only formants do not fix wind/brass/plucked glides. Kalimba, Xylophone and Steel Drum model approximations remain. Inert sustained-body decay and source controls replaced by full body mix are not counted as audible improvements. No all-presets listening, perfect realism or exact acoustic-equivalence claim is made.

## Published primary paths and subsequent closure

The ledger/team draft uses the required paths under `docs/preset-evidence/2026.11.4/ci/source-078c098/`: A53 `linux-x86_64/FINDINGS.md`, A56 `windows-linux-validators/FINDINGS.md`, A58 `audition/FINDINGS.md`, and A57/A59/A60 `final-review/a57-review.md`, `a59-review.md`, `a60-review.md`. Planned publication paths are separate from currently observed report bytes. A59/A60 hashes identify their preliminary snapshots only; they are not predictions of final-report bytes.

A57 has no self hash. Final manifests must be generated after report copying and root's factual status closure, without embedding a future manifest digest into its indexed reports.

**No new blocking roster, retained-byte, numerical or source-preservation finding remains.** A59/A60 final delivery, final root status/count closure, publication and remote/link verification occur subsequently and are not claimed successful by this snapshot. The concrete 60-row draft is ready for that narrow closure.

A57 wrote only scratch artifacts and made no repository/Git/external mutation or native-test, validator or audio rerun.
