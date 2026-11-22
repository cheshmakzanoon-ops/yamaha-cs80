# CI development artifact status

These packages collect the VST3, AU (macOS) and Standalone products that the
existing CI workflow already builds. They add no paid dependency, installer,
signing credential, automatic installation, OS-security override or new audio
code. Existing validator input artifacts remain separate and unchanged.

## Current development downloads — 2026.11.1

**All four installation archives and the actual 24-WAV audition are available
and verified. Linux remains experimental and unqualified because its GUI
validator failed; the versioned prerelease is blocked.** The source is
`6d3cc220edef5a4a2c7cd968ea8a786c7c83198c`, from
[build 45044040694, attempt 1](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/45044040694).
Windows and both Mac native suites and required platform validators passed on
that attempt. Linux's native build/test/package job passed, while its separate
validator job failed.

| Platform | Included formats | CTest registered / passed / failed / formal skips | Required validator status | Development ZIP |
| --- | --- | --- | --- | --- |
| Windows x64 | VST3, Standalone | 221 / 219 / 0 / 2. [Record](preset-evidence/2026.11.1/ci/native-windows-x86_64.json) | Both strictness-10 VST3 gates passed | [Windows ZIP](https://nightly.link/cheshmakzanoon-ops/yamaha-cs80/actions/artifacts/14763872941.zip) |
| macOS Apple Silicon (arm64) | VST3, AU, Standalone | 243 / 241 / 0 / 2. [Record](preset-evidence/2026.11.1/ci/native-macos-arm64.json) | VST3, AU and AU GUI strictness-10 gates passed | [Apple Silicon ZIP](https://nightly.link/cheshmakzanoon-ops/yamaha-cs80/actions/artifacts/14763761497.zip) |
| macOS Intel (x86-64) | VST3, AU, Standalone | 243 / 241 / 0 / 2. [Record](preset-evidence/2026.11.1/ci/native-macos-x86_64.json) | VST3, AU and AU GUI strictness-10 gates passed | [Intel Mac ZIP](https://nightly.link/cheshmakzanoon-ops/yamaha-cs80/actions/artifacts/14763644216.zip) |
| Linux x86-64, Ubuntu 24.04 build | VST3, Standalone | 579 / 578 / 0 / 1. [Record](preset-evidence/2026.11.1/ci/native-linux-x86_64.json) | **Experimental / unqualified:** GUI gate failed; headless and fixed-seed gates passed | [Linux ZIP](https://nightly.link/cheshmakzanoon-ops/yamaha-cs80/actions/artifacts/14768807632.zip) |

The native suites total **1,286 registered CTest entries: 1,279 passed, zero
failed and seven formal skips**. They also report **24 nested Python subtest
skips**, separately counted: 11 on Windows, five on each Mac and three on Linux.
Those nested skips are not additional formal CTest skips or passing subtests.

The unchanged version-3 four-target archive check passed **114 inventory entries
and 10 native binary headers**, including source/run/revision identities,
checksums, exact source-pinned packaged guides and `SOURCE.txt`. Complete Linux
and Windows VST3 file inventories match their separate validator input artifacts.
Every installation ZIP was fetched in full through its public URL without
authentication and matched its API size and SHA-256. These checks establish
archive integrity and input identity; they do not override a failed runtime gate.
[Four-platform archive verification](preset-evidence/2026.11.1/ci/final-install-artifacts.json) ·
[Validator payload identity](preset-evidence/2026.11.1/ci/final-validator-payload-identity.json) ·
[Public downloads](preset-evidence/2026.11.1/ci/download-links.json)

The [24-WAV audition ZIP](https://nightly.link/cheshmakzanoon-ops/yamaha-cs80/actions/artifacts/14769036590.zip)
was verified from this source and downloaded in full through its public URL:
**24 actual excerpts / 117 seconds**, with the final 301-program inventory,
metrics, provenance and 32 verified checksum entries. Ten standard examples,
one Pressure Lead example and six refined examples each last four seconds
(three-second hold, one-second release); seven slow examples each last seven
seconds (five-second hold, two-second release). Captures are unnormalized 48 kHz
stereo float. Pressure Lead uses aftertouch 0.65; all others use zero pressure.
[Actual audition verification](preset-evidence/2026.11.1/ci/final-audition.json) ·
[Audition public download](preset-evidence/2026.11.1/ci/audition-public-download.json)

### Linux validator failure and blocked prerelease

The first-attempt Linux GUI/Xvfb gate in job `137324977642`, seed `0x4369262`,
printed `SUCCESS` and then terminated by **SIGSEGV: raw −11, wrapper exit 139**.
The headless gate at seed `0x3e72560` and fixed GUI regression gate at
`0x2da72db` exited normally with code 0. GDB and fault-trace replays of the
failed seed also exited normally; they are instrumented nonreproductions, not
a fix or replacement for the failed gate. The original failure remains
unresolved, with no established plug-in-versus-host fault ownership.
[Linux verification](preset-evidence/2026.11.1/ci/final-linux-verification.json)

**The `presets-2026.11.1` versioned prerelease is BLOCKED.**
[Publisher run 45060638607](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/45060638607)
was skipped, and the final API check found no exact tag or release. The packages
above remain CI development downloads; no successful overall build or qualified
four-platform release is claimed.
[Release qualification](preset-evidence/2026.11.1/ci/release-qualification.json)

These bundles have no release signing or notarization. Preserve the complete
plug-in/app bundle during extraction, and select the Mac build for the host
process architecture; Linux requires compatible system libraries. Follow the
[installation guide](INSTALL_AND_QUICKSTART.md). The
[refinement report](PRESET_REFINEMENT_2026_11_1.md) retains the predecessor failure,
serialized-session behavior and VST3 Program-queue controller dependency.
Explicit selection from **Library** loads the revised factory voice; saved
sessions retain their serialized sound. The retained 2026.11 history below
remains separate and does not qualify these newer binaries.

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
