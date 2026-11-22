# Actual candidate builds, downloads and failed Linux gate

**The four native builds, installation archives and actual 24-WAV audition are
verified. The required Linux randomized GUI validator failed after printing
SUCCESS. The whole build failed, and the formal release was withheld.**

The fixed binary source is
`6d3cc220edef5a4a2c7cd968ea8a786c7c83198c`, preset revision **2026.11.1**,
[build 45044040694, attempt 1](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/45044040694).
Later documentation commits do not identify different binary bytes.

## Native execution and original validators

| Target | Registered CTest entries | Executed passes | Formal skips | Failures | Required strictness-10 validation |
| --- | ---: | ---: | ---: | ---: | --- |
| Windows x64 | 221 | 219 | 2 | 0 | Both headless VST3 lanes passed |
| macOS Apple Silicon | 243 | 241 | 2 | 0 | VST3, AU and AU GUI passed |
| macOS Intel | 243 | 241 | 2 | 0 | VST3, AU and AU GUI passed |
| Linux x86-64 | 579 | 578 | 1 | 0 | Headless and fixed GUI seed passed; randomized GUI failed with SIGSEGV after SUCCESS |

The Linux skipped case is `cs80_stereo_pra_exact_tests`, with return code 77.
Its JUnit status is also `notrun`; those classifications describe the same one
case. Windows and Mac records explain their two return-code-77 skips, and retain
embedded Python subtest skips separately. Mac and Windows successful fail-on-error
steps establish successful validator pipelines; no separately printed numeric
child-exit field is invented.

[Windows qualification](windows-x86_64/windows-qualification.json) ·
[Apple Silicon qualification](macos-arm64/ci-proof.json) ·
[Intel Mac qualification](macos-x86_64/ci-proof.json) ·
[Linux native result](linux-x86_64/native-proof.json) ·
[Linux JUnit](linux-x86_64/ctest-results.xml)

## Original Linux failure and diagnostic limits

Job **137324977642** used official pluginval **1.0.4**. Its randomized GUI lane,
seed **`0x4369262`**, printed SUCCESS at **2026-11-22 19:24:05.997 UTC**, then
original PID **2883** terminated with raw return code **−11 / SIGSEGV**.
The wrapper returned **139** and correctly refused a post-SUCCESS retry.
There were **zero internal retries**; every original lane had one child attempt.

The original headless seed `0x3e72560` and required GUI state-regression seed
`0x2da72db` exited zero. Scheduled GDB and fault-thread diagnostic replays both
used the failing seed and exited zero. They are nonreproductions, not replacement
passes. The runner's kernel pipe collector prevented a local original core;
the fault-thread replay log has handler-install notices but no crash stack.
Crash ownership and a causal repair are unproven.

[Exact failed qualification](linux-x86_64/ci-proof.json) ·
[Complete failed job log](linux-x86_64/validator-job.log) ·
[Original child termination](linux-x86_64/validator-reports/pluginval-gui-core.log) ·
[Independent failure review](failure-review/README.md)

The automatic [alternate-validator comparison, run 45060638632](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/45060638632)
actually executed three successful lanes on the identical Linux plugin payload:
official **2.0.0-beta3**, one 25-test headless subset and two 31-test GUI-inclusive
lanes. All child/logging exits were zero. It used a different validator and
different seeds, including neither the current failing seed nor an identical
host command. It supports compatibility with that tested configuration; it does
not repair the required 1.0.4 gate or identify the original crash's owner.
[Actual comparison joins and reports](failure-review/summary.json)

## Five verified public candidate downloads

| Candidate | Public artifact | Bytes |
| --- | --- | ---: |
| Windows x64 VST3 / Standalone | [14763872941](https://nightly.link/cheshmakzanoon-ops/yamaha-cs80/actions/artifacts/14763872941.zip) | 6,261,285 |
| macOS Apple Silicon VST3 / AU / Standalone | [14763761497](https://nightly.link/cheshmakzanoon-ops/yamaha-cs80/actions/artifacts/14763761497.zip) | 14,900,540 |
| macOS Intel VST3 / AU / Standalone | [14763644216](https://nightly.link/cheshmakzanoon-ops/yamaha-cs80/actions/artifacts/14763644216.zip) | 16,057,766 |
| Linux x86-64 VST3 / Standalone — GUI validation failed | [14768807632](https://nightly.link/cheshmakzanoon-ops/yamaha-cs80/actions/artifacts/14768807632.zip) | 13,225,022 |
| Actual 24-WAV CI audition | [14769036590](https://nightly.link/cheshmakzanoon-ops/yamaha-cs80/actions/artifacts/14769036590.zip) | 41,014,591 |

Each received an actual unauthenticated HTTP 200 GET. All received lengths and
SHA-256 values match fresh GitHub API metadata and the shared cache. These are
unsigned development artifacts; the Linux candidate carries the failed GUI
validation above. Their recorded API expiry is **2027-02-20 16:28:38 UTC**.
Availability can end earlier if an artifact is removed.

[Independent public-byte proof](public-candidates/summary.json) ·
[Download verification method](public-candidates/README.md)

The archive aggregate checks **114 inventoried entries**, all product roots,
native identities, source-generated guides, **172 pinned project links**, eight
local guide links, 32 notice comparisons and eight Unix executable modes. The
eight Windows notice differences are exclusively CRLF/LF checkout line endings;
their original archive bytes remain untouched. No product was installed or
executed locally for these download/archive checks.
[Actual four-archive inspection](installation/artifact-proof.json)

The actual audition contains **24 WAVs / 117 seconds / 5,616,000 stereo frames**
at 48 kHz, IEEE float32. All 32 checksums, 24 independently recomputed preset
parameter digests and the final 301-program inventory pass. No nonfinite or
full-scale samples occur. Its highest peak is **−14.5758332388 dBFS**.
This verifies signal/provenance data, not subjective sound quality or hardware
equivalence. [Actual audition proof](auditions/artifact-proof.json)

## Publication was withheld

Publisher [45060638607](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/45060638607)
and its job completed **skipped** after the failed build. The public
`presets-2026.11.1` release endpoint returned **404**. The seven planned release
assets, receipt and release tag are not certified or presented as available.
The strict verifier remains unchanged and rejects the actual failed run with
`Build did not complete successfully`.
[Recorded strict refusal](public-candidates/strict-refusal.json)

That publisher ran at documentation commit `96c466f`, whose source comparison
contained 232 allowed paths. This later, larger evidence archive does not weaken
the publisher's first-attempt, successful-job or fewer-than-300-path guards.
The original [preparation integration plan](integration-plan-before-final-outcome.json)
anticipated a successful release; the actual failed-gate outcome instead led to
this explicitly unqualified candidate handoff.

## Preserved records and reproduction

Preparation, pending and failed observations retain their original bytes and
stage. The source [dossier manifest](../provenance.json) inventories every copied
text payload. Embedded reviewer checksum lists can include omitted outer report
ZIPs or original working-directory paths; the dossier manifest is authoritative
for the compact archive. Three omitted alternate-validator report ZIP identities
are recorded there, while all 57 extracted report members are preserved.

Restore a checker's original relative paths from the manifest's `origin` entries
before rerunning it. Supplied checkers may require current API snapshots and
cached artifacts; their historical observations are not silently regenerated.
The Linux strict qualifier must fail on the retained original failure. The
candidate-byte checker certifies download identity and explicitly keeps release
qualification false.

The [preexisting Program-queue controller dependency](../../../PRESET_REFINEMENT_2026_11_1.md#native-recall-and-host-program-qualification)
also remains. No arbitrary-DAW, signed/notarized build, calibrated physical-CS-80
match or listening-panel result is asserted.

## Final review closure

All 60 distinct agent assignments returned their scoped results. The [published-document check](public-documents/published-candidate-verification.json) verifies 814c9014 and five live candidate links. The [independent final review](acceptance/final-review.md) records 101 evidence checks against that commit and its 522-payload dossier. **Candidate handoff is VERIFIED_RELEASE_UNQUALIFIED; original formal release acceptance remains BLOCKED.** The later documentation-only seal records completion without changing the binary source, required gates, five candidate URLs or original failure. [Completed team ledger](../../../TEAM_REVIEW_2026_11_1.md).
