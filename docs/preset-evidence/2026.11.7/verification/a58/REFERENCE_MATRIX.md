# A58 — aggregate release contract and evidence references

**Prepared contract and partial inspection only. No final release receipt is
created.** The explicit inspection snapshot is [phase1/snapshot.json](phase1/snapshot.json),
followed by the [Windows/refused-rerun supplement](phase1-supplement.json).
Numbers under “required” describe acceptance conditions. They are not inferred
results of missing native jobs, validators, packages or auditions.

## Identity and attempt rules

The frozen binary source is `ee0dabcacb4cdff93f8803844f60348e0ceda6ed`, tree
`17115913cbb29157bd00539485bcc5f38b57219c`, revision **2026.11.7**, native run
**45397189410**. This partial handoff contains producing attempt **1** only.
Later documentation HEAD is recorded independently. The retained sound-review
source is `c5549f7d095a5c5f49a5ad22ec0ccc819245d201`; A40 establishes continuity
of sound source, with no transfer of platform or executable acceptance.

Each selected artifact must carry its own source/run, original producing job,
**producing attempt**, exact name/ID, bytes, SHA-256, expiry, original upload
step and acquisition reference. The artifact API does not supply a separate
producing-attempt field. A run's latest attempt must never be assigned to an
older artifact without its original job/upload evidence.

If a complete same-source Intel rerun is later selected, the failed original
job **138455976307 / attempt 1** and artifact **14925757474** remain explicit
history. New successful evidence receives its actual attempt/job/IDs. No claim
that “attempt 1 passed every platform” is permitted. A single-test rerun or
diagnostic cannot replace the complete required native/validator/package
sequence. An attempt-2 execution also cannot be passed through an unchanged
attempt-1-specific helper by relabeling input provenance: prepare a distinct,
reviewed context-only successor when necessary, preserving old helper bytes,
assertions, native rosters and all former receipts.

A production, test, workflow, capture or other protected input change creates
a new binary source. Every actual native, validator, package, payload and
audition acceptance must identify that new source. Sound-source continuity can
preserve the stated scope of earlier audio reviews; it cannot qualify new
executables or archives.

## Required gate/reference matrix

Paths below are relative to `docs/preset-evidence/2026.11.7/`. “Pending” means
the final accepted receipt is not in A58's explicit partial handoff. This table
does not assert that a future path already exists.

| Gate | Exact acceptance condition | Owner / retained reference | Current A58 checkpoint |
| --- | --- | --- | --- |
| Source and bank identity | Full source/tree; unchanged protected inputs after freeze; 523 ordered existing programs × 204 parameters = 106,692 values; zero added presets. | A40 `verification/a40/source-ee0dabca/source-continuity.json`, frozen input manifest and helper hashes; A12 inventory; A42/A54/A55. | Frozen tree and current protected path equality checked; A12 inventory hash reproduced. Earlier sound reviews retain their actual source. |
| Linux native | Exact 613 names, complete numbered JUnit/LastTest agreement, zero failed/disabled/unknown, all four new tests passed, completed UI body identity/recall phase, successful original serial CTest. | A47 platform qualification; A53 all-platform native/skip review; frozen A40 native contract. | Actual archive absent from this handoff; actual counts are null. |
| Windows native | Exact 255 names and same complete contract. | A46 actual platform/component receipts; A53 `phase1-complete/windows-x86_64/`; producer 138455976256. | Original 253 passes + 2 formal skips; 11 nested Python skips. Subsequently completed A46 platform receipt reconciled in the supplement. |
| ARM native | Exact 277 names and same complete contract. | A48 `ci/source-ee0dabca/macos-arm64/platform-qualification.json`; A53 ARM review; producer 138455976227. | Original 275 passes + 2 formal skips; 5 nested skips. ARM platform receipt accepted externally. |
| Intel native | Exact 277 names and same complete contract; retain all failed attempts. | Original failure directory `ci/source-ee0dabca/macos-x86_64/attempt-1-failed/`; A49/A48 future actual review. | **Failure:** 274 passes + 1 UI-smoke failure + 2 formal skips; 5 nested skips. Native checker refuses the actual attempt. |
| Native aggregate | All four exact name sets; observed selected registrations sum to 1,422. Sum actual passes/formal skips independently, with zero failed/disabled/unknown in the selected accepted set. Internal assertions never add CTest executions. | Frozen A40 content checker plus unchanged A52 `--require-complete` parser and per-target archive binders; A53. | Three original archives observed: 809 registered, 802 passed, 1 failed, 6 formal skips, 21 nested skips. This is a failed partial observation, not a qualified total. |
| Validator lanes | Fifteen exact required lanes, pluginval 1.0.4, strictness 10, 523 programs in each, required GUI/fixed-seed exposure, complete console/report and original normal-zero exits. Preserve every prior invocation, failed attempt, permitted retry and post-SUCCESS termination. | A46 Windows original consumer binding; A47 Linux; A48 Macs; unchanged `.6` A52 parser. | Seven lanes reconciled: ARM 5 and Windows 2; seven invocations, zero validation retries. Eight required lanes unaccepted. |
| Installation content | Four actual API/ZIP identities; exact inner checksums/manifest/inventory, source/revision, native architecture, metadata and source-time guides/notices. | Per-platform receipts plus A57 final four-package matrix; unchanged `.5` A41 checker. | ARM and Windows actual archive identities agree with A57's independent checks. Intel/Linux pending. |
| Validated payload | Windows/Linux installed VST3 equals the exact product ZIP consumed by the original validator. Mac AU full before/after/copy comparisons, packaged bytes and source-path sequence; keep VST3 tree-hash limitation explicit. | A46/A47 A36 payload receipts and actual consumer logs; A48 original execution/payload bindings. | ARM and Windows accepted externally; original/payload component receipts retained. Intel/Linux pending. |
| Actual audition archive | Actual source-bound archive: 238 WAVs, 92 existing programs, 54 folders, 1,361.2 seconds, 65,337,600 stereo frames, 522,700,800 raw audio bytes, 297 regular files, 296 checksums. Exact 523 × 204 inventory agreement; complete roster, finite/unclipped samples and recomputed CSV metrics. | A50 actual content acceptance using frozen A40 successor of A28, plus original Linux capture/upload CI binding. | Prepared contract only. Actual archive, metrics and observed counts are null; no listening claim. |
| Five public full GETs | Four installation routes plus audition; unauthenticated full GET, HTTP 200, exact API byte count/hash, no partial Content-Range, source/name/ID/upload-attempt binding. Keep any failed GET attempts. | A51 final five-download mapping and individual original GET receipts. | ARM GET reconciled. Four remaining actual GETs pending; no all-five minimum expiry asserted. |
| Build/provenance | Exact clean JUCE 8.0.11 pin `c352e2489000ffeb9e397051377d75eef2efe641`; original commands, compiler/architecture/configuration and executable/artifact flow. Separate sound source from built binary source and online documentation HEAD. | A52 final provenance matrix and all original producer/consumer logs/APIs; A40 protected-input audit. | Partial original acquisition records rehashed. Full A52 final matrix pending. |
| Skips and warnings | All four platform-specific formal and nested skips reviewed against source; preserve nonverbose dynamic uncertainty, report-only screenshots, optional capability omissions and validator warnings. | A53 final reconciliation; A48/A46 retained warnings. | ARM/Windows skip reviews match reparsed artifacts. Intel failure and observed skip summaries retained without granting Intel acceptance. |
| Sound claims and coverage | Complete bank/family/address accounting, cumulative `.4` versus `.7` mechanisms kept distinct, Init preservation and explicit evidence limits. | A54 claims, A55 coverage, A42 compatibility and A43 install/reload limits. | Reviewed, with no exact acoustic equivalence, all-program listening, all-DAW or complete long-note performance certification. |
| Online publication | Update actual accepted source/run/per-artifact attempts, four downloads/audition, native/validator counts, hashes and minimum expiry together after acceptance. Preserve source-time packaged guide bytes and previous published/failure history. | A56 phase 2, based on actual aggregate and A51/A57 final mapping. | A56 phase 1 preserves `.4` downloads; no `.7` public promotion authorized by this snapshot. |
| Sixty distinct assignments | Exactly A01–A60 with distinct task identity and substantive completed reviews; reconcile all 60 retained review paths/hashes and actual worker ledger. A file count alone is insufficient. | A59 final team-ledger audit. | Final completion/ledger acceptance pending. A58 does not fabricate missing reviews. |
| Final release decision | A58 actual-input reconciliation plus A60 independent final signoff; every required gate closed or truthfully reported as blocked. | Root's concrete final handoff, A59 and A60; final aggregate receipt only after that handoff. | **Release and cross-platform qualification false.** No final aggregate written. |

## Fifteen exact validator lanes

| Target | Required lane / source workflow step | Required seed | Required GUI |
| --- | --- | --- | --- |
| Linux | `Validate VST3 headless` | Recorded randomized seed | No |
| Linux | `Validate VST3 GUI under Xvfb` | Recorded randomized seed | Yes |
| Linux | `Validate GUI state regression seed 0x2da72db` | `0x2da72db` | Yes |
| Windows | `Validate VST3 headless` | Recorded randomized seed | No |
| Windows | `Validate state regression seed 0x5037a0c` | `0x5037a0c` | No |
| ARM and Intel, each | `Validate VST3 with pluginval` | Recorded randomized seed | No |
| ARM and Intel, each | `Validate AU with pluginval` | Recorded randomized seed | No |
| ARM and Intel, each | `Validate AU with pluginval GUI` | Recorded randomized seed | Yes |
| ARM and Intel, each | `Validate AU GUI fixed-seed regression` | `0x2b4eef5` | Yes |
| ARM and Intel, each | `Validate AU GUI regression seed 0x3c99748` | `0x3c99748` | Yes |

The table contains three Linux, two Windows and five on **each** Mac target:
3 + 2 + 5 + 5 = **15**. Console/report copies are one lane. Transport retries
while acquiring pluginval are separate from product validation retries.

## Final handoff format

Supply the actual selected four platform receipts and original acquisition
records, the actual audition archive/content/CI receipts, A51's five full-GET
mapping, A52 provenance, A53 skip/native reconciliation, A57 four-package
matrix, A59 ledger and A60's review status. Include every superseded failed
source/attempt and exact source continuity evidence if the selected source
changes. All supplied files must have actual paths and hashes, without empty
placeholder records, invented IDs or predeclared acceptance flags.

A58 will then reconcile each selected row, compute observed totals and the
earliest actual expiry across the five selected artifacts, and produce the
allowed source-specific final receipt. Supporting DSP/screenshot results, if
cited, retain their actual source/run/attempt and never substitute for these
native, validator, payload or audition gates.

## Preserved user-facing limits

- No new presets: all 523 addresses remain; `.7` has zero new recipe-value
  edits relative to `.6`, while cumulative `.4` sound/control evidence is
  separately described by A54/A55.
- Authored synthesized interpretations are not certified exact acoustic
  reproductions. Engineering measurements and archive acceptance are not
  human listening approval.
- Current saved body-enabled controls may render differently with changed DSP.
  Factory LOAD/RELOAD applies current sound values and PANEL sources, retains
  physical setup and separate modern configuration, and is not a complete
  session reset. Release keys and sustain before playing fresh notes.
- The processing-queue-only VST3 wrapper Program controller dependency remains
  unresolved. Real processor/MIDI fixtures do not qualify that separate path or
  arbitrary DAWs and historical project formats.
- Complete-bank default measurements, targeted expressive scenes, slow-attack
  gaps and CPU/memory limits keep their stated scopes. Distinct vectors and
  numerical changes do not establish universal perceived improvement.
- CI archives have temporary retention. Successful public GETs establish
  availability at their observation times only. No publisher signing,
  notarization, interactive installer certification or separate end-user
  Standalone launch is inferred.
