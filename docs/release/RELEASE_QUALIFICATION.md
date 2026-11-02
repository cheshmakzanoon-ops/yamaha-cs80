# Release qualification (Phase 7)

Phase 7 of [IMPROVEMENT_PLAN.md](../IMPROVEMENT_PLAN.md). This page separates what
the repository now automates from what only the owner can decide or run.

## 1. Packaging, signing and the universal macOS binary — automated

[`.github/workflows/release.yml`](../../.github/workflows/release.yml) runs on
`workflow_dispatch` and on `v*` tags. It does not replace `build.yml`: release
only a commit whose `build`, `DSP numerical measurements` and `screenshots`
workflows passed.

| Platform | Output | Signing |
| --- | --- | --- |
| macOS 11+ | One universal (arm64 + x86_64) VST3, AU and Standalone; `lipo -archs` must list both architectures for every binary. Installer `.pkg` from [`installer/macos/make-pkg.sh`](../../installer/macos/make-pkg.sh). | Developer ID with hardened runtime, then `notarytool` and `stapler`, when the secrets exist. Otherwise ad hoc and the package is renamed `*-UNSIGNED.pkg`. |
| Windows x64 | Inno Setup installer from [`installer/windows/cs80-reference.iss`](../../installer/windows/cs80-reference.iss): VST3 to `Common Files\VST3`, Standalone to `Program Files`. | Authenticode with `signtool` when the secrets exist; otherwise `*-UNSIGNED.exe`. |
| Linux x86_64 | `.deb` from [`installer/linux/make-deb.sh`](../../installer/linux/make-deb.sh) (VST3 in `/usr/lib/vst3`, Standalone as `cs80-reference`) plus the existing provenance tarball. | None (unsigned, as is usual for direct `.deb` downloads). |

Every installer ships the JUCE licence. The secrets the workflow reads are listed
at the top of `release.yml`; none are stored in the repository.

**AAX** is not built. It needs the Avid AAX SDK and a PACE/iLok signing agreement,
which only the owner can obtain. With both, add `AAX` to `CS80_PLUGIN_FORMATS`,
point `juce_set_aax_sdk_path` at the SDK and sign with PACE `wraptool`.

## 2. Intermittent Linux pluginval exit 139

Root-cause analysis runs on the integrated code base after the Phase 5 editor
changes, because the editor and its teardown are the likely owner of a crash
that happens after `SUCCESS`. Findings are recorded in
[VALIDATOR_DIAGNOSTICS.md](../VALIDATOR_DIAGNOSTICS.md). A rerun that passes never
counts as a fix.

## 3. Real-DAW sessions — owner action

These need the hosts and licences on the owner's machines. Run each host on each
platform it supports, with the release candidate installed from its installer.

| Check | Pass criterion |
| --- | --- |
| Scan and load | Appears under its own name; loads with no host warning. |
| Fresh instance | Starts on the circuit engine; the editor opens at 1600×1000 and resizes down to 1120×700. |
| Old session | A project saved with a pre-plan build reopens on Legacy and sounds identical. |
| Automation | Record and play back cutoff, resonance and a tone-tab change; no zipper noise, no jumps. |
| Program Change | Host program list and incoming Program Change select factory programs. |
| Offline bounce | Bounce equals realtime apart from oversampling quality (HQ offline); latency is compensated. |
| MPE / MIDI learn | MPE controller drives per-voice bend and pressure; a learned CC survives save and reload. |
| Undo / A-B | Host undo and the plugin's own undo do not fight; A/B survives save and reload. |
| Sleep and transport | Stop, loop and sample-rate changes (44.1 → 96 kHz) produce no clicks or stuck notes. |

| Host | macOS | Windows | Linux | Result |
| --- | --- | --- | --- | --- |
| Ableton Live | VST3, AU | VST3 | — | not run |
| Logic Pro | AU | — | — | not run |
| Reaper | VST3, AU | VST3 | VST3 | not run |
| Bitwig Studio | VST3 | VST3 | VST3 | not run |
| Cubase | VST3 | VST3 | — | not run |
| FL Studio | VST3, AU | VST3 | — | not run |

## 4. Licence — decided: AGPLv3

The owner chose the **GNU Affero General Public License v3** (2026-10-31). The
full text is in [`LICENSE`](../../LICENSE). This matches JUCE 8's free AGPLv3
option, so no commercial JUCE licence is needed. Anyone who receives a binary is
entitled to the complete corresponding source: every package names its exact
source commit (`SOURCE.txt` / `manifest.json`), and every installer ships both
`LICENSE` and the JUCE licence.

## 5. Product name and trademarks — legal review required

"Yamaha" and "CS-80" are names this project does not own. A lawyer should review
each use below before any public release. Nothing here is legal advice.

| Where | Current use |
| --- | --- |
| Product name, `CMakeLists.txt` `PRODUCT_NAME` | "CS-80 Reference" (plugin name in every host) |
| Bundle ID, `BUNDLE_ID` | `com.referenceinstruments.cs80` |
| Editor nameplate, `PluginEditor.cpp` | Engraved "YAMAHA" wordmark and "CS-80" lettering |
| About text, `AdvancedPanel.cpp` | "A circuit-informed recreation of the Yamaha CS-80 (1977)" |
| Documentation and factory program names | Model references throughout `docs/` |

Common options are a non-referential product name with a descriptive
"inspired by" statement, removal of the wordmark from the nameplate art, and a
bundle ID without the model number. Changing the bundle ID or plugin codes breaks
saved sessions, so decide before the first public release.


## Reviewed-main dispatch request (2026-10-31)

The existing `release.yml` remains the installer workflow. A repository
maintainer may dispatch it through the small
`.github/workflows/dispatch-reviewed-release.yml` relay when their connected
GitHub client has branch-write operations but does not expose workflow dispatch.
Create/update `.github/release-request.json` on the dedicated
`codex/cs80-release-dispatch` branch with full `expected_main_sha` and
`validated_sha` fields. The request branch must start from reviewed main.
The relay uses the repository's ephemeral Actions token, with `actions: write`
and `contents: read`; it neither adds secrets nor exports credentials.

Before dispatch, the guard checks the main SHA, the latest successful main
push runs of `build.yml`, `dsp-measurements.yml` and `screenshots.yml`, and the
ancestry of any automatically refreshed screenshot-only commit. A retry
recognizes an existing running or successful release of the same main SHA.
The dispatch target is always `main`. Code changes after the validated revision,
a failed/pending check, or a changed main head stop the request for review;
there is no forced branch update or retry-to-green behavior.

The contract was registered in `tests/release/dispatch-contract.json` before
implementation. The standalone fixture passes 17 checks. Five temporary
mutations are rejected: missing main lease, unvalidated code changes, ignored
screenshot failure, wrong dispatch branch, and duplicate release dispatch.
Run `node tests/release/dispatch-reviewed-release.test.cjs` to repeat the guard
fixtures. The relay executes the same fixtures before invoking GitHub's API.
GitHub explicitly permits workflow-dispatch events created by `GITHUB_TOKEN`:
<https://docs.github.com/en/actions/how-tos/write-workflows/choose-when-workflows-run/trigger-a-workflow>.
