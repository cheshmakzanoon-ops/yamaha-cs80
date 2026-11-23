# CI development artifact status

These packages collect the VST3, AU (macOS) and Standalone products that the
existing CI workflow already builds. They add no paid dependency, installer,
signing credential, automatic installation, OS-security override or new audio
code. Existing validator input artifacts remain separate and unchanged.

## Current source revision — 2026.11.2 circuit library

The current source provides one circuit instrument and revoices the existing
523-program catalogue. It removes the Legacy/Pure choices and includes the
original 222 addresses in **Library → Factory Library**. The
[current revision report](CIRCUIT_LIBRARY_2026_11_2.md) explains the sound changes
and the intentional playback change for older engine policies.

For an installed package, its own `SOURCE.txt` and manifest identify the binary
source, native target and workflow run. Consult that exact run and the
[repository download table](https://github.com/cheshmakzanoon-ops/yamaha-cs80#downloads)
for its validation status. This document is embedded during packaging, before
all dependent validation jobs finish; packaging alone does not establish a
qualified release. The following sections preserve the status and download links
of earlier revisions and do not describe a 2026.11.2 binary.

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
