# A44 — release package layout and installation instructions

Reviewed source: `078c0981ffa1b5472342e0fb5ab8702bc3bd2889`.

Scope: read-only review of `scripts/package-ci-artifacts.py`, `.github/workflows/build.yml`, `docs/INSTALL_AND_QUICKSTART.md`, `docs/PRESET_GUIDE.md`, `docs/CI_DISTRIBUTION_STATUS.md` and the existing README download section. No repository edits, builds, downloads or installation were performed. This report describes the source contract; it does not assert that final-source artifacts or validation results are available.

## Outcome

The bundled installation and preset guides correctly identify library revision **2026.11.4**, the four separate native targets, complete-bundle copying, and the explicit factory **LOAD / RELOAD** upgrade action. The package producer implements the stated inner formats and product layout. No instruction correction blocks publication once the actual artifacts and native gates are qualified.

README and `docs/CI_DISTRIBUTION_STATUS.md` still identify **2026.11.3 / cbbee6a** as the last verified download. Replace their current-download record only after final-source qualification. Preserve the historical `.3` evidence, numbers, identifiers and expiry; none can be reused as `.4` results. The frozen new packages will contain the source-time historical distribution-status text. A later online qualification record must explicitly distinguish that historical bundled snapshot from the newly completed qualification; changing online documentation does not replace bundled bytes.

## Exact source-defined package structure

Every Actions installation artifact is an **outer ZIP** containing exactly the native inner archive, a matching `<root>.manifest.json` sidecar, and `SHA256SUMS`. The packager writes checksums for both inner archive and sidecar. The native archive has one root named below, containing the whole advertised products and notices.

| Computer / host process | Actions artifact name | Native inner archive and root at this source | Products below the root |
| --- | --- | --- | --- |
| Windows x64 | `cs80-windows-install-bundle` | `cs80-reference-windows-x86_64-078c0981ffa1.zip`; root without `.zip` | `VST3/CS-80 Reference.vst3/`; `Standalone/CS-80 Reference.exe` |
| Linux x86-64, Ubuntu 24.04 build | `cs80-linux-install-bundle` | `cs80-reference-linux-x86_64-078c0981ffa1.tar.gz`; root without `.tar.gz` | `VST3/CS-80 Reference.vst3/`; `Standalone/CS-80 Reference` |
| macOS Apple Silicon, native arm64 | `cs80-macos-15-install-bundle` | `cs80-reference-macos-arm64-078c0981ffa1.tar.gz`; root without `.tar.gz` | `VST3/CS-80 Reference.vst3/`; `AU/CS-80 Reference.component/`; `Standalone/CS-80 Reference.app/` |
| macOS Intel, native x86-64 | `cs80-macos-15-intel-install-bundle` | `cs80-reference-macos-x86_64-078c0981ffa1.tar.gz`; root without `.tar.gz` | Same three Mac product names, with x86-64 binaries |

The packager's source-derived inner filenames above are expected names, not evidence of completed native artifacts. Root must compare them with actual qualified bytes.

Common inner-root files are `manifest.json`, `SOURCE.txt`, `INSTALL_AND_QUICKSTART.md`, `PRESET_GUIDE.md`, `DISTRIBUTION_STATUS.md`, and `LICENSE`. The `licenses/` directory contains JUCE, Barlow Condensed, MTS-ESP and libxml2 license/source notices; `evidence/juce-provenance.txt` retains the configured JUCE record. There are 13 added evidence/guide/notice files plus the embedded manifest, independently of the product files/directories. Guides use local links to other bundled guides; other relative repository links are pinned to the package's exact source commit.

`SOURCE.txt` explicitly states that the archive **is not a source distribution** and links the complete repository source. The manifest and `SOURCE.txt` must both identify revision `2026.11.4` and the qualified full source SHA. Application version `1.0.0` alone does not identify these bytes.

## Concise installation table for the final download record

Quit the host, preserve an earlier copy outside its scan folders, unpack the outer ZIP, verify `SHA256SUMS`, then unpack the native inner archive. Preserve each complete plug-in/application bundle. Use `tar -xzf` for Linux/Mac inner archives so executable modes and bundle links survive; Windows uses Extract All for the inner ZIP.

| System / format | Copy the complete item to | Standalone |
| --- | --- | --- |
| Windows VST3 | `%LOCALAPPDATA%\Programs\Common\VST3\CS-80 Reference.vst3`; if the host requires its documented system location, `%COMMONPROGRAMFILES%\VST3\CS-80 Reference.vst3` | Keep `CS-80 Reference.exe` in a folder you own and launch it |
| Linux VST3 | `~/.vst3/CS-80 Reference.vst3` | Keep the executable `CS-80 Reference` in a folder you own and launch it |
| macOS VST3 | `~/Library/Audio/Plug-Ins/VST3/CS-80 Reference.vst3` | Keep the complete `CS-80 Reference.app`, for example in `~/Applications` |
| macOS AU | `~/Library/Audio/Plug-Ins/Components/CS-80 Reference.component` | Same standalone app as above |

Create the destination if needed, restart the DAW and perform its normal plug-in rescan. Avoid duplicate copies with the same plug-in identity inside scan locations. Standalone requires no DAW scan; select the audio output and MIDI input in its settings.

## Material platform qualifications

- **Signing:** describe these as **CI installation bundles without publisher release signing or Apple notarization**. Do not call every Mac byte unsigned: the workflow explicitly ad-hoc signs the AU (`codesign --force --sign -`) before registration and packaging. VST3/app signature state is not independently certified by the packager. The guide correctly avoids advising security-bypass operations.
- **Mac architecture:** these are separate arm64 and x86-64 packages, not qualified universal installers. Match the **host process architecture**. Native runner builds do not establish older-macOS or emulation compatibility. The actual AU distributed must retain the source/registered-tree equality and before/after executable-hash evidence required by the workflow.
- **Linux:** the Ubuntu 24.04 x86-64 package is dynamically linked, not a static binary or AppImage. ALSA, X11, font/rendering and other required system libraries must be compatible. Do not promise older-distribution compatibility, a universal minimum glibc version or bundled audio drivers from source inspection alone.
- **Windows:** the build is native x64 on the `windows-2022` runner. Do not infer qualified older-Windows or ARM-native support. Some hosts require the system VST3 folder; copying there may require administrator permission.
- **Distribution lifetime:** these are retained Actions artifacts, not permanent release assets. The final download record must use each actual API expiry and its exact artifact ID/size/digest. No `.3` expiry or identifier should carry over.
- **Qualification:** package creation checks binary target headers, product presence, complete inventory and declared provenance; it does not establish the 13 required pluginval lanes, all DAWs, signing or acoustic realism. Actual artifact/producer binding and normal validator exits remain required.

## Exact upgrade instruction to retain

> After installing, open **Library → FACTORY LIBRARY**, choose the existing factory preset and explicitly press **LOAD / RELOAD**. Enter or double-click also loads it. Play fresh notes, then save the project if you want it to retain the revised recipe.

An old project can display the same factory name while restoring its previous settings. With **Audition** enabled, clicking the already-loaded row can leave those settings unchanged. **REVERT** restores the state present when Library opened; it does not fetch the newest factory recipe. Body model, partial decay and size latch at note onset, hence fresh notes are required. Saved parameter values also do not freeze the previous DSP's sound: body-enabled patches run the changed synthesis code. Retain the old binary/reference bounce if exact prior-session audio is required.

The Library route avoids the separately documented VST3 Program-processing-queue-only controller dependency. This is a recall instruction, not a claim that every host Program implementation is qualified.

## Publication wording suggestion

Use the four-platform download table with newly verified URLs, followed by:

“Unpack the outer ZIP, verify its checksums and unpack the inner archive while keeping each plug-in/app bundle intact. Choose the Mac build for your host process architecture. These are CI bundles without publisher release signing or Apple notarization; the Mac AU is ad-hoc signed, and Linux requires compatible system libraries. The exact source, test results, archive hashes and retention dates are recorded in the linked qualification report. After upgrading, use **Library → FACTORY LIBRARY → LOAD / RELOAD** and play fresh notes to apply the revised factory recipe.”

No new download URL, native pass, expiry or artifact identity is asserted by this review.
