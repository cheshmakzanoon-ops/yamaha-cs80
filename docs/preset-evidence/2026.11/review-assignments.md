# Review assignments — revision 2026.11

**60 requested primary assignments plus one nested follow-up reviewer were
actually spawned: 61 distinct subagents in total.** Work ran in stages with at
most three subagents alongside the coordinator. The complete primary roster,
separate nested reviewer, evidence references and outcomes are recorded in
[review-assignments.json](review-assignments.json).

## Current result and acceptance status

Original source: `d0c3c23be68ef048aa56969f1c493c221c1562c5`;
[build run 45029235535](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/45029235535).
All four native suites, all four actual install archives, complete validator-input
payload comparisons, public downloads and the actual 12-WAV audition have passed
their recorded checks. The four native suites total **1,275 passes, zero failures
and seven skips**. **Independent final acceptance is complete for accurately
disclosed original 2026.11 development-download publication.** The original Linux
validator failure remains unexplained after two completed instrumented
nonreproductions. The coordinator's publication commit/push is a separate step.

GitHub now reports successful attempt 2, but this was **not a clean first pass**.
Attempt 1 printed SUCCESS and then terminated with SIGSEGV. A separate externally
triggered Linux validator execution passed on the same artifact using a different
randomized seed and runner. Root and the agents issued no rerun. Neither the second
execution nor the separate beta comparison demonstrates that the original fault
was fixed.

The later screenshot-only descendant `7f55a95a7534b389927defe46866456560b3a878`
is not the binary source. The isolated diagnostic branch described below is also
separate from candidate qualification.

## Separate newer integration

This ledger remains scoped to the original **2026.11** binaries from `d0c3c23`
and run `45029235535`. Concurrent **2026.11.1** work has source
`6d3cc220edef5a4a2c7cd968ea8a786c7c83198c` and
[build run 45044040694](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/45044040694),
whose first attempt is **pending at this snapshot**. The later publication-only
commits `4a3bd7116fc3a5f4344fcdd5e6694ff4c2646452` and
`00339ed7c616da28e5c32011e558ea00833f041b` do not change that binary source.
This newer integration has separate qualification requirements; the original
results do not qualify its binaries. Follow-ups reuse the existing agents, with
no additional spawns.

## Linux validator attempts — retain both outcomes

| Observation | Attempt 1 | Externally triggered attempt 2 |
| --- | --- | --- |
| Job | [137263093053](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/45029235535/job/137263093053) | [137263915334](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/45029235535/job/137263915334) |
| Headless strictness 10 | Seed `0x5ba7253`: SUCCESS, then **SIGSEGV**, raw exit **−11**, wrapper 139 | Seed `0x50a3104`: SUCCESS, raw exit **0** |
| GUI strictness 10 | Skipped | Seed `0x75af02`: SUCCESS, raw exit **0** |
| Fixed GUI strictness 10 | Skipped | Seed `0x2da72db`: SUCCESS, raw exit **0** |
| Artifact | `14761700962` | Same artifact and digest |
| Interpretation | Authoritative failed gate; zero in-wrapper retries; no stack or proven cause | Separate successful execution; not a replay of the failed headless seed or a demonstrated fix |

GitHub actor and triggering-actor fields name the owner account. They do not
identify the exact human, UI, token, app or automation that initiated attempt 2.
Only the Linux validator was newly executed; the other successful jobs in the
merged attempt-2 view retain their original execution times and evidence. The
attempt reconciliation preserves the original failure files unchanged.

## Current native and deliverable checks

| Platform | Native CTest passed / failed / skipped | Capture CLI | Validator scope |
| --- | --- | --- | --- |
| [Windows x64](ci/final-windows-x86_64.json) | **218 / 0 / 2** of 220 | Eight passed; one POSIX-only skip | Both required headless strictness-10 gates passed |
| [Apple Silicon](ci/final-macos-arm64.json) | **240 / 0 / 2** of 242 | Nine passed | VST3, AU headless and AU GUI strictness 10 passed |
| [Intel macOS](ci/final-macos-x86_64.json) | **240 / 0 / 2** of 242 | Nine passed | VST3, AU headless and AU GUI strictness 10 passed |
| Linux x86-64 | **577 / 0 / 1** of 578 | Nine passed | Attempt 1 failed; separate attempt 2 passed as recorded above |

Each native platform passed the library audit, 136 voicing checks and 6,800 recall
checks. Linux also passed the repaired controller fixture (**376 checks**) and
strict panel fixture (**four checks**). Its single skip is
`cs80_stereo_pra_exact_tests`. Windows/macOS pixel comparisons are report-only;
their suites do not register the Linux-only controller handoff fixture. Platform
and internal Python skips remain explicit in the native reports.

The coordinator's actual **v2 `--require-all` verification passed all four archives**:
**114 inventory entries and ten binary headers**, with source/run/revision,
checksums, document correspondence and exact generated `SOURCE.txt` checks.
Complete regular-file VST3 inventories in the separate Linux and Windows validator
inputs match their install bundles. That payload comparison does not inspect
validator runtime outcomes or explain a crash.

| Verified public download | Artifact ID | Complete outer bytes |
| --- | ---: | ---: |
| [Windows x64](https://nightly.link/cheshmakzanoon-ops/yamaha-cs80/actions/artifacts/14758250272.zip) | 14758250272 | 6,259,679 |
| [Apple Silicon](https://nightly.link/cheshmakzanoon-ops/yamaha-cs80/actions/artifacts/14758286408.zip) | 14758286408 | 14,898,681 |
| [Intel macOS](https://nightly.link/cheshmakzanoon-ops/yamaha-cs80/actions/artifacts/14758362460.zip) | 14758362460 | 16,055,847 |
| [Linux x86-64](https://nightly.link/cheshmakzanoon-ops/yamaha-cs80/actions/artifacts/14761556554.zip) | 14761556554 | 13,220,309 |
| [Actual 12-WAV audition](https://nightly.link/cheshmakzanoon-ops/yamaha-cs80/actions/artifacts/14761870353.zip) | 14761870353 | 16,802,082 |

All five full unauthenticated GETs returned **HTTP 200**, the complete expected
byte count and the expected API SHA-256 digest. Agent 56 independently verified
the actual audition: capture complete, **12 WAVs / 48 seconds**, **301-program
inventory**, all **18 checksum entries**, decoded-audio/CSV agreement, zero
nonfinite or over-full-scale samples, and nonsilent Pressure Lead at pressure
0.65. No audio was regenerated for verification. This is measured audio evidence,
not a subjective listening-panel or hardware-equivalence result.

The Windows copied-document rule remains narrow: exact source bytes or exact
LF-to-CRLF conversion of three copied Markdown guides. Generated `SOURCE.txt`
remains byte-exact; arbitrary whitespace/content normalization is not accepted.
The original mismatch and agent 60's limited acceptance remain recorded.

## Separate comparison and completed diagnostics

The coordinator reports that official beta3 comparison run
[45040382534](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/45040382534)
passed its three cases with **25, 31 and 31 checks**, using the same candidate
artifact. It uses a different host and seeds. Its results do not replace the
pluginval 1.0.4 requirements or resolve the original SIGSEGV.

The two bounded replays of failed headless seed `0x5ba7253` are complete in
[diagnostic run 45041640467](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/45041640467),
job `137267174290`, on isolated branch `diagnostic/preset-d0c3-headless`, workflow
commit `b61d4057e83eac404c9f0db0c38f9ee857f83baf`. Agent 54 verified one GDB run
and one fault-trace run using the exact failed input artifact at strictness 10.
Both printed SUCCESS and exited **0**, without fault frames. These are **two
instrumented nonreproductions**: the original crash remains unexplained, and no
crash ownership or production fix has been established. Any general Build on that
diagnostic branch is excluded from candidate qualification.

The [public diagnostic report](ci/headless-diagnostic.json) records both outcomes
and diagnostic artifact `14761467989` (**8,995 bytes**), SHA-256
`c455d7e957ef49bd986b9ba4c02925ef742c93471d9eb2e4fdbe50d1945c6df3`.

Agent 53's finalizer/copy review is resolved, including factory-reselect/saved-state
instructions, Pressure Lead operation and qualified 67/63-sample latency wording.
Online documentation corrections do not alter the guide bytes already verified
inside immutable candidate archives. The README download section is finalized
with a prominent Linux caveat. The [joined qualification record](ci/release-qualification.json)
connects all four native suites, all four archives, complete VST3 payload identities,
the actual audition and all five public downloads while retaining the first failure.
Independent review of this evidence and unresolved crash history is now complete,
with the bounded acceptance described below.

## Independent final acceptance

Agent 60's final report accepts **accurately disclosed original 2026.11 development
downloads**, with the Linux failure unresolved and no blocking publication
discrepancy within that scope. The exact machine-readable verdict is
`ACCEPTED_FOR_ORIGINAL_2026_11_DEVELOPMENT_PUBLICATION_WITH_RECORDED_UNRESOLVED_LINUX_FAILURE`.
The [JSON ledger](review-assignments.json) retains both report identifiers and
SHA-256 hashes under `original_2026_11_final_publication_acceptance`.

The report independently joined **83 of 83 retained evidence hashes** and
**206 of 206 read-only consistency checks** at its review snapshot, including
actual outer and inner archives, all 114 inventory entries and ten headers,
complete Linux/Windows VST3 payloads, the five retained full-GET records, and the
actual 12-WAV, 48-second audition. These are evidence-review checks; the reviewer
did not execute another build, validator, capture or network request.

Acceptance closes assignment 60. It does not change the failed first invocation,
establish a production fix, qualify newer 2026.11.1 binaries, or certify that the
coordinator's later publication commit/push has occurred. A clean first pass,
release signing/notarization, universal DAW qualification, hardware equivalence
and a completed subjective listening study remain outside the accepted claims.

## Assignment completion and primary coverage

All **60 primary assignments**, including agent 60's bounded final acceptance,
and the **one nested review** are complete: **61 completed distinct subagents**
in total. Later follow-ups reuse these agents without additional spawns. The
original acceptance does not transfer to the separate pending 2026.11.1 integration.

| Primary agents | Assignments | Scope |
| --- | ---: | --- |
| 1–6 | 6 | Discovery and contracts |
| 7–14 | 8 | Family authoring |
| 15–18 | 4 | Validation, capture, CI and startup |
| 19–27 | 9 | Family render and measurement reviews |
| 28–30 | 3 | Guide, bundle provenance and identity |
| 31–42 | 12 | Controls, behavior, register, Pure mode, performance scope and evidence report |
| 43–48 | 6 | Publication and evidence review |
| 49–51 | 3 | Current d0c3c23 Windows and both macOS native/validator passes |
| 52 | 1 | Four actual bundles and complete validator-input payload identities verified |
| 53 | 1 | All install/audition public downloads passed; finalizer and copy review resolved |
| 54 | 1 | Controller fixture repair passed; two diagnostic nonreproductions completed, original crash unexplained |
| 55 | 1 | 41 voice pairs / 75 programs |
| 56 | 1 | Historical local preflight plus actual current 12-WAV audition verified |
| 57 | 1 | Install and update source compatibility |
| 58 | 1 | Independent behavioral evidence review |
| 59 | 1 | Review assignment ledger |
| 60 | 1 | Original 2026.11 development publication accepted with the unresolved Linux failure disclosed |

Agent 54's additional nested reviewer was
`/root/w09_controller_host_contract/controller_extension_contract`. It completed
a read-only review of the declared activation contract, with no edits, builds,
tests or network calls. It is listed separately from the 60 numbered primary tasks.

## Preserved historical attempts and fixture repairs

The original `7ac6d99` run `45014200523` exposed two stale integration fixtures.
Its follow-up `ba3a721` run `45016741244` then failed Linux: **575 passes, two
failures and one skip among 578 registrations**. That run produced no Linux install
bundle or published audition; dependent Linux validators were skipped. Its three
other-platform results and archives remain under their historical identities.

The two `ba3` Linux fixture causes were demonstrated independently. The controller
fixture reproduced 16 failures among 376 checks; removing only unrelated pressure
pitch modulation passed, and re-enabling it restored all 16 failures. The panel
fixture reproduced the exact CI differences with default font selection and passed
when only its DejaVu search path was pinned. Assertions, settling frames, image
references and tolerances remained unchanged. Agent 60 accepted the minimal
repairs, which were committed in `d0c3c23` and now pass native Linux. Those fixture
repairs are separate from the later, unresolved validator SIGSEGV.

## Evidence boundaries

The [engineering report](../../PRESET_REVISION_2026_11.md),
[evidence index](README.md) and [manifest](provenance.json) record measured
conditions and retained evidence. All files in the ledger's timestamped manifest
snapshot matched their recorded hashes; its count is a snapshot, not a fixed size
for the evolving bundle. Session report identifiers in JSON are historical review
references, not promises that every working report is bundled in the repository.

The voice-distinction review covered 41 pairs and 75 programs. The independent
behavior review identifies 52 behavior assertions, two reconstruction controls and
82 policy/finite/peak checks within its 136-check result across 30 renders of 20
programs. These bounded checks do not establish universal musical quality,
qualification of every DAW/controller history, physical CS-80 equivalence, or a
completed listening panel. Artifact integrity and a successful later execution do
not establish that an earlier runtime crash was fixed.
