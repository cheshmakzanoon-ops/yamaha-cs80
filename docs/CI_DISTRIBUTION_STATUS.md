# CI development artifact status

These packages collect the VST3, AU (macOS) and Standalone products that the
existing CI workflow already builds. The packaging step adds no paid dependency, installer,
signing credential, automatic installation, OS-security override or new audio
code. Existing validator input artifacts remain separate and unchanged.

## Source candidate — 2026.11.5 playing-response and material refinement

This source snapshot contains the **2026.11.5** factory library: all 521
existing musical programs have revised sound controls, with all 523 identities
and both initialization programs preserved. The appended material/vowel
variants retain the preceding 200-core/306-host parameter prefix. See the
[revision report](CIRCUIT_LIBRARY_2026_11_5.md) and [preset guide](PRESET_GUIDE.md).

**Native build and archive qualification for this source is pending at the
time these bundled guides are written.** The verified artifacts, source hashes,
test totals and links in the following section refer to the preceding
2026.11.4 build. They do not qualify a package built from this candidate.
Each new archive's own manifest identifies its source. The repository's
[current download section](https://github.com/cheshmakzanoon-ops/yamaha-cs80/blob/main/README.md#downloads)
will identify the exact newly qualified source and archives after their
independent build, validator and content checks finish.

## Previous qualified revision — 2026.11.4 instrument voicing

**All six jobs passed in [build 45246863788, attempt 1](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/45246863788).**
The installation bundles contain exact source `078c0981ffa1b5472342e0fb5ab8702bc3bd2889`, tree
`d84661c6c465c53ff41361e8aba0cba5e4a53397`, and factory-library revision
**2026.11.4**. All 521 existing musical programs have revised sound controls;
both initialization programs and all 523 names, categories, tags, order and
program addresses are preserved. No programs were added. The
[revision report](CIRCUIT_LIBRARY_2026_11_4.md) separates the continuation's
tine/bowed corrections from historical 59d489d matched-audio measurements.

| Platform | Included formats | Native CTests passed / registered | Verified installation download |
| --- | --- | ---: | --- |
| Windows x64 | VST3, Standalone | 227 / 229 | [Windows ZIP](https://nightly.link/cheshmakzanoon-ops/yamaha-cs80/actions/artifacts/14856804857.zip) |
| macOS Apple Silicon (arm64) | VST3, AU, Standalone | 249 / 251 | [Apple Silicon ZIP](https://nightly.link/cheshmakzanoon-ops/yamaha-cs80/actions/artifacts/14855214289.zip) |
| macOS Intel (x86-64) | VST3, AU, Standalone | 249 / 251 | [Intel Mac ZIP](https://nightly.link/cheshmakzanoon-ops/yamaha-cs80/actions/artifacts/14858878730.zip) |
| Linux x86-64 (Ubuntu 24.04 build) | VST3, Standalone | 586 / 587 | [Linux ZIP](https://nightly.link/cheshmakzanoon-ops/yamaha-cs80/actions/artifacts/14870428441.zip) |

Across the four native suites, **1,311 of 1,318 registered CTest executions
passed, zero failed and seven were formally skipped**. The **24 nested Python
subtest skips** are counted separately. All **13 required pluginval 1.0.4
validation lanes at strictness 10** completed with original process exit 0:
three Linux VST3 lanes, two Windows VST3 lanes and four VST3/AU lanes on each
Mac architecture. There were **13 validation invocations and 0 runtime validation retries**. Fixed-seed regressions are
included. These are platform executions, not unique test definitions; console
and report copies of a validator invocation count once.
[Consolidated qualification and original evidence](preset-evidence/2026.11.4/ci/source-078c098/release-qualification.json)

All four actual installation ZIPs matched authentic GitHub API sizes and
SHA-256 digests. The source/run/revision, complete inventories, checksums, JUCE
provenance and native executable headers passed inspection. Original successful
producer/upload steps bind each artifact ID, size and digest to source 078c098,
run 45246863788 and attempt 1. The complete distributed Windows and Linux VST3
payloads equal the separate artifacts consumed by their original validators.
Mac AU pre/post hashes and full-bundle checks match the distributed binaries;
Mac VST3 tested paths are bound to the unchanged packaged build outputs.
Original successful API steps and unsuppressed exit propagation supplement
the raw parser, whose deliberately limited `release_qualified: false` output
is retained unchanged. Actual seeds, warnings and skip notices remain in the
validator evidence. Acquisition used the GitHub connector and does not establish
an independently verified unauthenticated public download route.

The verified [138-WAV audition ZIP](https://nightly.link/cheshmakzanoon-ops/yamaha-cs80/actions/artifacts/14870917591.zip) contains **870 actual seconds
across 74 existing programs**, in unnormalized **48 kHz stereo float32**. All
19 active body models are represented. The complete 173-entry archive, all
172 member checksums, settings, samples and metrics passed inspection. Its
41,760,000 stereo frames contain no nonfinite or over-full-scale samples. The
complete **523-by-200** delivered inventory matches all ordered identities,
parameter IDs and 104,600 binary32 values in the separately retained inventory;
the JSON bytes also match. The zero-pressure Pressure Lead example is
intentionally silent. No human listening approval or exact acoustic equivalence
is inferred from these checks.
[Actual audition acceptance](preset-evidence/2026.11.4/ci/source-078c098/audition/acceptance-summary.json) ·
[Delivered inventory identity](preset-evidence/2026.11.4/ci/source-078c098/audition/delivered-inventory-identity.json)

These are **CI installation bundles without publisher release signing or Apple
notarization**. Mac tools may attach ad-hoc signatures. Choose the architecture
used by the host process and preserve bundle contents. Windows pluginval is
headless. Recorded tests do not certify every DAW or interactive installation
workflow. The earliest expiry over all four advertised installers and the
audition archive is **21 February 2027 at 15:14:35 AST (19:14:35 UTC)**. These are retained Actions artifacts, not
permanent release assets. The [original run's Artifacts list](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/45246863788#artifacts)
is the fallback while retained.

The bundled guides describe the source snapshot written before dependent
validators finished. Later repository documentation does not alter archived
binaries, manifests or guides. Use **Library → FACTORY LIBRARY → LOAD / RELOAD**,
then play fresh notes. Older projects restore saved settings; clicking an
already-loaded row with Audition enabled may retain those settings.

### Development corrections and supporting checks

The exact-source supporting DSP workflow
[45246863831, attempt 1](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/45246863831)
and all five jobs passed. Six actual supporting archives passed API/hash,
source/run/attempt, original-upload and complete-entry inspection. The ASan/UBSan
core-build suite recorded **199 passed, zero failed and one formal skip out of
200 registered CTest entries**, with no reported sanitizer diagnostics. Nested
Python skips are unknown in that sanitizer log. These supporting executions
remain separate from native and pluginval counts. Source V1/audio V2
compatibility uses the original successful byte-comparison step; synthetic
reference fixtures are not hardware measurements.
[Supporting DSP evidence](preset-evidence/2026.11.4/ci/source-078c098/supporting-dsp/supporting-ci.json)

The exact-source
[screenshot workflow 45246863473](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/45246863473)
passed UI smoke and the actual 73-file screenshot artifact was verified. Both
golden-size images are byte-identical to the unchanged goldens, with **0.0000%
changed pixels**. The earlier bcc86aa screenshot run had three genuine fixture
assertion failures. Source 078c098 corrected float canonicalization expectations
and pinned layout-fixture controls to historical values; audio code, factory
values, goldens, masks, comparator and tolerances were unchanged.
[Screenshot acceptance and historical failure](preset-evidence/2026.11.4/ci/source-078c098/screenshots/acceptance-summary.json)

The six jobs and **24 CTest executions** in
[filter run 45244531626](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/45244531626)
passed at source bcc86aa. All **91 relevant input files** are byte-identical to
078c098; the executed source is not relabeled. Clang ON/DEFAULT lanes intentionally
use the scalar fallback, so these are not described as six AVX2 runs.
[Filter source equivalence and results](preset-evidence/2026.11.4/ci/source-078c098/filter/summary.json)

Earlier .4 native run 45237136125 at 59d489d remains failed, with its native
jobs canceled and validators skipped. The superseded c4fe309, e32728b and
bcc86aa native runs remain canceled. Their outcomes and earlier optional
comparison failures are preserved in the continuation evidence; none becomes a
successful release gate by a later rerun. The continuation uses **60 actual
workers in batches**, with distinct assignments, retained outcomes and clearly
labeled conversational evidence. The earlier development work-log is separate
and contributes no workers to that count.
[Completed team review](TEAM_REVIEW_2026_11_4.md) ·
[Actual continuation ledger](preset-evidence/2026.11.4/continuation/worker-ledger.json)

## Historical verified revision — 2026.11.3 instrument voicing

**All six jobs passed in
[build 45176799427, attempt 1](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/45176799427).**
The installation bundles below contain exact source
`cbbee6a940985ad3c550768414960d2e1b1e802e`, tree
`ddcbc5a150810b2e3d30c94e1c208e2e1ce81ec2`, and factory-library revision **2026.11.3**.
All **521 existing musical programs** have revised sound controls; both
initialization patches and all **523 names and program addresses** are preserved.
The [revision report](CIRCUIT_LIBRARY_2026_11_3.md) explains the sound changes.

| Platform | Included formats | Native CTests passed / registered | Verified installation download |
| --- | --- | ---: | --- |
| Windows x64 | VST3, Standalone | 225 / 227 | [Windows ZIP](https://nightly.link/cheshmakzanoon-ops/yamaha-cs80/actions/artifacts/14822019089.zip) |
| macOS Apple Silicon (arm64) | VST3, AU, Standalone | 247 / 249 | [Apple Silicon ZIP](https://nightly.link/cheshmakzanoon-ops/yamaha-cs80/actions/artifacts/14820874764.zip) |
| macOS Intel (x86-64) | VST3, AU, Standalone | 247 / 249 | [Intel Mac ZIP](https://nightly.link/cheshmakzanoon-ops/yamaha-cs80/actions/artifacts/14822214497.zip) |
| Linux x86-64 (Ubuntu 24.04 build) | VST3, Standalone | 584 / 585 | [Linux ZIP](https://nightly.link/cheshmakzanoon-ops/yamaha-cs80/actions/artifacts/14837191097.zip) |

Across the four native suites, **1,303 of 1,310 registered CTest entries passed,
zero failed and 7 were formally skipped**. The **24 nested Python subtest
skips** are reported separately. All **13 required pluginval 1.0.4 validation lanes at
strictness 10** completed with their original process exit **0**, with
**zero runtime validation retries**: three Linux VST3 runs, two Windows VST3 runs,
and four VST3/AU runs on each Mac architecture. Fixed-seed regression gates are
included. Counts refer to platform executions, not unique test definitions.
[Consolidated qualification and original evidence](preset-evidence/2026.11.3/ci/source-cbbee6a/release-qualification.json)

All four actual installation ZIPs matched the fresh GitHub artifact API sizes
and SHA-256 digests. The contents passed source/run/revision, complete inventory,
JUCE provenance and native executable-header checks. The complete distributed
Windows and Linux VST3 payloads match the separate artifacts consumed by their
successful validators. Producer logs bind each artifact ID, digest and size to
the correct source/run/attempt and successful upload step. Download acquisition
used the GitHub connector; it is not described as an independently tested
unauthenticated download route.

The verified [90-WAV audition ZIP](https://nightly.link/cheshmakzanoon-ops/yamaha-cs80/actions/artifacts/14836886459.zip) contains **594 actual seconds across
71 existing programs**, in unnormalized **48 kHz stereo float32**. All 19 active
body models are represented. All 90 WAVs, settings, sample values, metrics and
106 member checksums passed independent inspection; the complete 523-by-198
inventory matches the separately reviewed implementation inventory. Every
sample is finite and below full scale. The zero-pressure Pressure Lead example
is intentionally silent.
[Actual audition acceptance](preset-evidence/2026.11.3/ci/source-cbbee6a/audition/acceptance-summary.json) ·
[Delivered inventory identity](preset-evidence/2026.11.3/ci/source-cbbee6a/audition/delivered-inventory-identity.json)

These are **CI installation bundles without publisher release signing or Apple
notarization**. Mac tools may attach ad-hoc signatures. Choose the Mac architecture
used by the host process and preserve bundle contents. Windows pluginval is
headless; the recorded executions do not certify every DAW or installation
workflow. The earliest recorded API expiry is **21 February 2027 at 08:42:09 AST (12:42:09 UTC)**;
these are retained Actions downloads, not permanent release assets.

The bundled guides describe the source snapshot written before all dependent
validators completed. This later repository update records the completed
results and does not alter archived binaries, manifests or guide bytes. Select
a factory program again from **Library** to hear its current recipe; an old
project restores its saved settings.

### Development corrections and supporting checks

The implementation source `0b81c3d` had nine stale screenshot/host-fixture
assertions. Its native run was cancelled when the source was superseded.
`a81851b` corrected those fixtures, preserving every golden image and tolerance;
its screenshot run passed with **0.0000% changed pixels** at both golden sizes.
That source's ARM native suite then had a genuine packaging-notice failure
because a historical report link label had changed. The remaining jobs were
cancelled. Source `389a448` restored the label and passed all seven local
packaging checks. Its later Windows native run exposed three host-fixture
crashes: `cs80_circuit_source_host_tests`, `cs80_modern_recall_host_tests` and
`cs80_ui_smoke`. That failed native run is not presented as a qualified release.

Source `cbbee6a` moves the large test-fixture processors to heap ownership,
preserving their assertions, lifetimes and the configured test stack reserve.
All three formerly failing Windows gates passed in the exact-source native run
above. None of the production audio or preset code changed during these
corrections.
[Source history](preset-evidence/2026.11.3/source-fixture-correction.json)

Earlier supporting DSP and screenshot results retain their exact `a81851b`
source. Its sanitizer job had **196 passed CTests, one failed Python packaging-notice
test and one formal skip**. The failed notice assertion checked the historical
report label subsequently corrected in `389a448`. The model, engine, voicing,
character, precision and complete-library audio checks passed in that sanitizer
build, and no ASan/UBSan finding was reported. **The sanitizer job and its parent
DSP workflow remain failed historical results.**

The final source `cbbee6a940985ad3c550768414960d2e1b1e802e` was separately executed in
[DSP run 45176799301](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/45176799301)
and [screenshot run 45176799293](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/45176799293),
both at attempt 1. Both workflows and all five DSP jobs passed. The ASan/UBSan
core-build suite recorded **197 passed CTest entries, zero failures and
one formal skip out of 198 registered entries**, with no reported
sanitizer diagnostics. These counts include Python/tool fixtures as well as
instrumented core tests. The full UI smoke and golden-image checks passed at
their original tolerances. The two actual compatibility archives were inspected
and bound to their successful producers.

Filter evidence retains its earlier `0b81c3d` source and unchanged filter inputs.
Earlier outcomes are preserved separately and are not relabeled as runs of the
final commit.
[Current supporting CI review](preset-evidence/2026.11.3/ci/source-cbbee6a/supporting-ci-final.json) ·
[Historical a81851b supporting review](preset-evidence/2026.11.3/ci/source-a81851b/supporting-ci-final.json) ·
[60-agent ledger](TEAM_REVIEW_2026_11_3.md)

## Historical verified revision — 2026.11.2 circuit library

**All six jobs passed in
[build 45112404257, attempt 1](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/45112404257).**
The verified development bundles below contain source
`9c0d2baef5643a983a0e7a378b6be033371248f4` and factory-library revision
**2026.11.2**. This revision provides one circuit instrument, removes the
Legacy/Pure engine choices and preserves all **523 existing names and host
program addresses**. The original 222 programs are included in
**Library → Factory Library**. The
[revision report](CIRCUIT_LIBRARY_2026_11_2.md) describes the revoicing and the
intentional playback change for older engine policies.

| Platform | Included formats | Native CTests passed / registered | Verified installation download |
| --- | --- | ---: | --- |
| Windows x64 | VST3, Standalone | 223 / 225 | [Windows ZIP](https://nightly.link/cheshmakzanoon-ops/yamaha-cs80/actions/artifacts/14792445167.zip) |
| macOS Apple Silicon (arm64) | VST3, AU, Standalone | 245 / 247 | [Apple Silicon ZIP](https://nightly.link/cheshmakzanoon-ops/yamaha-cs80/actions/artifacts/14791856136.zip) |
| macOS Intel (x86-64) | VST3, AU, Standalone | 245 / 247 | [Intel Mac ZIP](https://nightly.link/cheshmakzanoon-ops/yamaha-cs80/actions/artifacts/14791699516.zip) |
| Linux x86-64, Ubuntu 24.04 build | VST3, Standalone | 582 / 583 | [Linux ZIP](https://nightly.link/cheshmakzanoon-ops/yamaha-cs80/actions/artifacts/14796735944.zip) |

Across the four native suites, **1,295 of 1,302 registered CTest entries passed,
zero failed and seven were formally skipped**. The **24 nested Python subtest
skips** are counted separately: 11 on Windows, five on each Mac architecture and
three on Linux. All **13 required pluginval 1.0.4 invocations at strictness 10**
completed with their original exit **0**, with **zero runtime validation
retries**: two Windows headless VST3 runs, four runs on each Mac architecture
(VST3, AU and two AU GUI runs), and three Linux VST3 runs (headless and two GUI
runs). The fixed-seed regression gates are included in these totals.
[Consolidated qualification and original evidence](preset-evidence/2026.11.2/ci/source-9c0d2ba/release-qualification.json)

All four installation ZIPs were downloaded in full without authentication and
matched fresh GitHub API byte counts and SHA-256 digests. The actual archives
also passed source/run/revision, complete inventory, native binary header,
guide and notice checks. The complete Windows and Linux VST3 bundles matched
the separate artifacts consumed by their validators. These checks apply to the
exact linked artifacts and recorded CI executions.

The verified
[68-WAV audition ZIP](https://nightly.link/cheshmakzanoon-ops/yamaha-cs80/actions/artifacts/14796467679.zip)
contains **431 seconds across 56 existing programs**, in unnormalized **48 kHz
stereo float32**, plus the complete inventory of **523 programs with 190
parameters each**.
Its public download matched the API digest, and its original upload was bound
to the same source, run and producing job attempt. All 68 WAVs, settings,
checksums and sample/CSV measurements passed the frozen verifier. No samples
are nonfinite or at or above full scale; only the zero-pressure Pressure Lead
example is intentionally silent.
[Actual audition acceptance](preset-evidence/2026.11.2/ci/source-9c0d2ba/audition/acceptance-summary.json)

These are **CI development bundles without publisher release signing or Apple
notarization**. macOS build tools may attach ad-hoc signatures. The Mac packages
contain separate arm64 and x86-64 binaries; choose the architecture used by the
host process, keep each plug-in/application bundle intact and follow the
[installation guide](INSTALL_AND_QUICKSTART.md). Windows pluginval coverage is
headless. CI editor checks on the other platforms do not qualify every DAW or
interactive installation workflow. No exact acoustic or hardware
equivalence is claimed. The recorded artifact expiry is **21 February 2027 at
04:31:50 UTC**; these retained CI downloads are not permanent release assets.

Each package's `SOURCE.txt` and manifest identify its binary source and target.
Its bundled documentation is the source snapshot written during packaging,
before all dependent validators finished. This later repository update records
the final results; it does not alter the archived guides, manifests or binary
bytes. Opening an old session retains its stored patch controls: select the
factory program again to hear the revised recipe, and save a user copy to keep
an edited patch.

Earlier failures remain recorded as failures in the sections below and the
linked evidence. In particular, the cause of the older Linux post-`SUCCESS`
crashes remains unproven; the three successful Linux invocations in this build
do not establish a permanent fix. The historical sections retain their original
source identities, outcomes and download links.

## Historical 2026.11.1 development candidates — release blocked

**All four native build/test/package jobs passed and the actual installation
archives and 24-WAV audition were verified. The complete build failed because
Linux randomized GUI validation terminated by SIGSEGV after SUCCESS.** The
strict publisher was skipped and no qualified 2026.11.1 prerelease was published.

These are **unsigned development candidates**, from source
`6d3cc220edef5a4a2c7cd968ea8a786c7c83198c`,
[build 45044040694, attempt 1 — failed](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/45044040694).
Windows and both Mac targets passed their required platform validators. Linux
passed headless and fixed-seed GUI validation, but its randomized GUI gate failed
with raw −11 / wrapper 139. The same-seed instrumented nonreproductions do not
establish a fix; the original crash core was unavailable.

| Candidate platform | Included formats | Pinned candidate download |
| --- | --- | --- |
| Windows x64 | VST3, Standalone | [Windows candidate ZIP](https://nightly.link/cheshmakzanoon-ops/yamaha-cs80/actions/artifacts/14763872941.zip) |
| macOS Apple Silicon (arm64) | VST3, AU, Standalone | [Apple Silicon candidate ZIP](https://nightly.link/cheshmakzanoon-ops/yamaha-cs80/actions/artifacts/14763761497.zip) |
| macOS Intel (x86-64) | VST3, AU, Standalone | [Intel Mac candidate ZIP](https://nightly.link/cheshmakzanoon-ops/yamaha-cs80/actions/artifacts/14763644216.zip) |
| Linux x86-64 (Ubuntu 24.04 build) | VST3, Standalone | [Linux experimental ZIP — GUI validation failed](https://nightly.link/cheshmakzanoon-ops/yamaha-cs80/actions/artifacts/14768807632.zip) |

Native CTest results are **219 passed / 221 registered** on Windows,
**241 / 243** on each Mac architecture and **578 / 579** on Linux, with zero
native test failures. Windows and each Mac target have two formal skips; Linux
has one SKIP_RETURN_CODE=77. The linked records retain embedded Python subtest
skips separately. Linux's native suite took 9,492.20 seconds. A green native
suite does not reverse the failed dependent Linux GUI gate.
[Windows native evidence](preset-evidence/2026.11.1/ci/native-windows-x86_64.json) ·
[Apple Silicon native evidence](preset-evidence/2026.11.1/ci/native-macos-arm64.json) ·
[Intel Mac native evidence](preset-evidence/2026.11.1/ci/native-macos-x86_64.json) ·
[Linux gates and original exits](preset-evidence/2026.11.1/final-ci/linux-x86_64/ci-proof.json)

The [consolidated qualification](preset-evidence/2026.11.1/ci/release-qualification.json)
records **1,286 registered CTest entries: 1,279 passed, zero failed and seven
formal skips**. Its **24 nested Python subtest skips** are separate: 11 Windows,
five on each Mac and three Linux; they are not additional formal CTest skips.

All four actual install archives passed their bounded inventory, architecture,
format, executable/bundle, guide and notice checks. All five candidate ZIPs,
including the audition, were downloaded in full without credentials and matched
fresh API byte counts/digests and the cached artifacts. **Their byte verification
does not qualify a complete release.**
[Four-archive installation evidence](preset-evidence/2026.11.1/final-ci/installation/artifact-proof.json) ·
[Five public GETs, hashes and withheld publisher](preset-evidence/2026.11.1/final-ci/public-candidates/summary.json)

The complementary [V3 archive aggregate](preset-evidence/2026.11.1/ci/final-install-artifacts.json)
records **114 inventory entries and 10 native headers**. The
[complete validator payload comparison](preset-evidence/2026.11.1/ci/final-validator-payload-identity.json)
joins every Linux and Windows VST3 file to the actual validator input artifacts;
that byte identity does not establish runtime success.

The [24-WAV audition ZIP](https://nightly.link/cheshmakzanoon-ops/yamaha-cs80/actions/artifacts/14769036590.zip) contains **117 seconds** of
unnormalized 48 kHz stereo core-engine excerpts, including all twelve refined
programs. Its 24 parameter digests, 32 checksums and actual samples were checked;
all samples are finite and below full scale. This is distinct from both the
local `99f5ebd` replay and the older twelve-excerpt 2026.11 package below.
[Actual CI audition proof](preset-evidence/2026.11.1/final-ci/auditions/artifact-proof.json)

The package manifests and `SOURCE.txt` identify the binary source, independently
of later documentation on `main`. These retained Actions artifacts have recorded
API expiry **20 February 2027 at 16:28:38 UTC**. They are not permanent release
assets, signed installers, notarized apps or universal Mac binaries. Follow the
[installation guide](INSTALL_AND_QUICKSTART.md), select the Mac architecture of
the host process and keep each plug-in/application bundle intact.

The [refinement report](PRESET_REFINEMENT_2026_11_1.md) preserves the predecessor
failure, this unresolved Linux GUI failure and the pre-existing VST3 Program
queue controller dependency. That older binary used **Library → Circuit** to
select its revised factory values; saved sessions retained serialized values. No queue-only Program
automation, arbitrary DAW integration or acoustic/hardware equivalence is claimed.

The original **2026.11** downloads and their separate Linux failure/retry history
below remain intact; those older binaries do not contain the twelve later
2026.11.1 refinements.
The [2026.11.1 60-agent assignment review](TEAM_REVIEW_2026_11_1.md) records this
task separately from the earlier 2026.11 review. Completing an investigation
does not turn its failed gate into a pass.

[Recorded download acceptance](preset-evidence/2026.11.1/ci/acceptance/FINAL-ACCEPTANCE-2026_11_1.md) ·
[Teardown source audit](preset-evidence/2026.11.1/ci/teardown-source-audit.md)

## Pinned verified development downloads — 2026.11

These retained packages verify the original 300-program redesign. Newer
2026.11.1 source and packaging changes on `main` are preserved; their separate
publication status is tracked in the [refinement report](PRESET_REFINEMENT_2026_11_1.md).
The pinned 2026.11 packages do not include those later changes.

The four installation bundles below come from source
`d0c3c23be68ef048aa56969f1c493c221c1562c5`, in
[build 45029235535](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/45029235535).
All four native build/test/package jobs passed in attempt 1. The latest build
status is **success on attempt 2**, which reran only the Linux validator and
retained the other successful executions.
[Combined verification record](preset-evidence/2026.11/ci/release-qualification.json) ·
[Latest run status](preset-evidence/2026.11/ci/run-latest-attempt.json)

**Linux has a known intermittent validator failure.** The first stable headless
pluginval invocation printed `SUCCESS` and then died with raw −11 / SIGSEGV.
The original invocation remains failed and unexplained. A later invocation used
the same VST3 artifact on a different runner with a different random seed; all
three Linux validator gates then passed with normal exit 0. The latest successful
status does not demonstrate a clean first pass or a fix. Separate beta3 passes
and two instrumented original-seed diagnostic passes did not establish the
crash's cause or ownership. The [revision report](PRESET_REVISION_2026_11.md#linux-validator-history-and-remaining-limitation)
preserves the attempt history, seed/runner changes and diagnostic limits.

| Platform | Included formats | Pinned installation download |
| --- | --- | --- |
| Windows x64 | VST3, Standalone | [Windows ZIP](https://nightly.link/cheshmakzanoon-ops/yamaha-cs80/actions/artifacts/14758250272.zip) |
| macOS Apple Silicon (arm64) | VST3, AU, Standalone | [Apple Silicon ZIP](https://nightly.link/cheshmakzanoon-ops/yamaha-cs80/actions/artifacts/14758286408.zip) |
| macOS Intel (x86-64) | VST3, AU, Standalone | [Intel Mac ZIP](https://nightly.link/cheshmakzanoon-ops/yamaha-cs80/actions/artifacts/14758362460.zip) |
| Linux x86-64, Ubuntu 24.04 build | VST3, Standalone; validator limitation above applies | [Linux ZIP](https://nightly.link/cheshmakzanoon-ops/yamaha-cs80/actions/artifacts/14761556554.zip) |

All four downloaded archives passed source/run/revision, inventory and native
binary-header checks: **114 inventory entries and 10 binary headers** in total.
The complete Linux and Windows VST3 payloads also match their separate validator
input artifacts. Every installation ZIP was fetched in full without GitHub
authentication and matched its API byte count and SHA-256.
[Archive verification](preset-evidence/2026.11/ci/final-install-artifacts.json) ·
[Validator payload identity](preset-evidence/2026.11/ci/final-validator-payload-identity.json) ·
[Public download verification](preset-evidence/2026.11/ci/final-download-links.json)

The [twelve-WAV audition ZIP](https://nightly.link/cheshmakzanoon-ops/yamaha-cs80/actions/artifacts/14761870353.zip)
was verified from the same source and downloaded in full through its public URL.
It contains **12 actual excerpts / 48 seconds**, the complete preset inventory,
metrics, provenance and checksums. Pressure Lead uses aftertouch; the other
eleven excerpts use zero pressure.
[Audition verification](preset-evidence/2026.11/ci/final-audition.json)

These are CI development bundles without release signing or notarization.
Artifact downloads expire under GitHub retention. If nightly.link is unavailable,
sign in to the build's GitHub **Artifacts** list; an expired artifact requires a
newer retained build. Follow the [installation guide](INSTALL_AND_QUICKSTART.md)
for complete-bundle extraction and native architecture selection.

## Evidence and limitations

- The archive and external manifest identify a full source commit, native
  architecture and the CI-verified official JUCE 8.0.11 source. Each archive is
  read back to verify member paths, file hashes, Unix modes and symlink targets.
- **Downloadable is not release-qualified.** Linux/Windows validators run in
  separate dependent jobs; a package may exist before those gates finish or if
  they later fail. Check all required jobs for the exact commit. macOS packaging
  follows its existing VST3/AU validators. No DAW-install pass is inferred.
- The helper does not compile, execute, sign, notarize or install the products.
  It preserves existing bundle contents. Ad-hoc signatures generated by build
  tools are not Developer ID signatures or Apple notarization. No Windows
  publisher-signature claim is made.
- No universal macOS binary, installer, driver, AppImage, portable Linux runtime,
  or automatic updater is supplied. See the install guide for architecture and
  system-library limitations.

## Licensing

The owner released the project under the GNU Affero General Public License v3
(`LICENSE`, commit `06eb8f3`), matching JUCE 8's AGPLv3 option. Every archive
carries the project `LICENSE` and `licenses/JUCE-LICENSE.md`, copied from the
verified JUCE checkout; the release installers ship both as well
([release/RELEASE_QUALIFICATION.md](release/RELEASE_QUALIFICATION.md)).
`SOURCE.txt` points to the corresponding source commit; the archives are not
complete source distributions, so the AGPL source offer is met by that public
commit. This helper makes no further legal-compliance determination (for
example a trademark review or a complete third-party notice audit). These
verified CI artifacts have no release signing or notarization. A separate
signed-installer release requires its own qualification.

## Reproduce packaging

After the existing Release build/tests and CI JUCE-source verification:

```sh
python scripts/package-ci-artifacts.py --self-test
python scripts/package-ci-artifacts.py --platform linux --build-dir build --out-dir packages
```

Use `macos` or `windows` only on that native runner. The helper requires the
`build/ci-juce-provenance.txt` PASS record for the current checkout's full commit;
it fails on missing/empty binaries, missing macOS bundle metadata, unsafe links,
lost Unix executable bits, wrong JUCE evidence or existing package output.
For Windows payloads, reparse points (including directory junctions/mount
points) are rejected at product roots and before walking nested entries or
opening payload bytes. Ordinary `is_symlink()` checks alone do not cover those
Windows objects. Linux/macOS safe internal bundle symlinks retain their existing
policy. Run `python scripts/test-package-binary-identity.py` for inert regression
fixtures; the real junction case explicitly skips outside Windows.

No copy/truncate fallback overwrites earlier packages. It does not delete the
build products. Synthetic `--self-test` fixtures are not loadable plug-ins or a
claim of native host qualification.

Output: one `.tar.gz` (Linux/macOS) or `.zip` (Windows), an external JSON manifest,
and `SHA256SUMS`. All contain/describe only the selected instrument bundles and
the explicit accompanying documentation/evidence, not the full build directory.
