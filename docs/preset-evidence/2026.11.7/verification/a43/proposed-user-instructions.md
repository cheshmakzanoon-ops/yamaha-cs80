# Proposed installation and factory-reload instructions

Editorial handoff for A56/root. This is ready-to-adapt user copy, not an announcement that the pending build has qualified. Publish new artifact links and final results only after the four actual installation archives and required release evidence are accepted. Preserve the original archived guides.

## Install the build listed in Downloads

Choose the package in the [current download table](https://github.com/cheshmakzanoon-ops/yamaha-cs80/blob/main/README.md#downloads). Check its full source commit in `SOURCE.txt` and `manifest.json` against that table. The application version alone does not identify the build. Revision **2026.11.7** retains all **523 existing programs**; no additional preset bank needs importing.

| Target | Download to choose | Formats |
| --- | --- | --- |
| Windows x64 | Windows | VST3, Standalone |
| macOS host running natively as arm64 | Apple Silicon | VST3, AU, Standalone |
| macOS host running as x86-64 | Intel Mac | VST3, AU, Standalone |
| Linux x86-64 with compatible system libraries | Linux, built on Ubuntu 24.04 | VST3, Standalone |

The Mac packages are separate native builds. Match the DAW process architecture: an Apple Silicon computer does not by itself tell you which architecture its DAW is running. Emulation and older operating systems need their own qualification.

1. Quit the DAW and any running Standalone copy. Keep a backup of an older build outside plug-in scan folders if you need it for an existing project.
2. Extract the downloaded outer ZIP. Keep the inner archive, its `.manifest.json` sidecar and `SHA256SUMS` together.
3. Verify both checksum entries. On Linux run `sha256sum -c SHA256SUMS`; on macOS run `shasum -a 256 -c SHA256SUMS`. In Windows PowerShell, use `Get-FileHash -Algorithm SHA256` for the inner ZIP and its matching `.manifest.json`, and compare both results with `SHA256SUMS`.
4. Extract the inner `.tar.gz` with `tar -xzf "archive-name.tar.gz"` on Linux/macOS, or use **Extract All** for the inner Windows ZIP. Open the extracted `cs80-reference-…` folder. Keep each complete plug-in/app bundle intact; tar preserves the Unix permissions and internal links.
5. Copy only the formats you want:

| From the extracted package | Destination |
| --- | --- |
| Linux `VST3/CS-80 Reference.vst3` | `~/.vst3/CS-80 Reference.vst3` |
| macOS `VST3/CS-80 Reference.vst3` | `~/Library/Audio/Plug-Ins/VST3/CS-80 Reference.vst3` |
| macOS `AU/CS-80 Reference.component` | `~/Library/Audio/Plug-Ins/Components/CS-80 Reference.component` |
| Windows `VST3/CS-80 Reference.vst3` | `%LOCALAPPDATA%\Programs\Common\VST3\CS-80 Reference.vst3`, if your host scans that location |

Create a missing destination folder. A Windows host may instead require `%COMMONPROGRAMFILES%\VST3`; writing there may require administrator permission. Replace the complete old bundle and keep one copy of this plug-in identity in the host's scan locations. Restart the host and use its documented rescan procedure.

For Standalone, launch the item under `Standalone`: `CS-80 Reference.exe` on Windows, `CS-80 Reference.app` on macOS, or the executable `CS-80 Reference` on Linux. Keep it in a folder you own; `~/Applications` is suitable on macOS. These are CI bundles without publisher release signing or Apple notarization. If the system blocks one, report its exact prompt rather than changing security settings. Linux also requires compatible system libraries.

## Hear the current factory settings

For a first comparison, use a new empty project and a fresh instrument instance. Select your audio output and MIDI input; a fresh instance starts with **Brass Swell I**. An older project can restore its previous settings while showing exactly the same factory name.

1. Save a copy of any edited sound you want to keep before loading a factory program. The toolbar **SAVE** menu or the Library's **SAVE AS** button can save a user patch; use a distinct name.
2. Release all keys and the sustain pedal. Open **LIBRARY**. If old search text or filters hide the sound, press **CLEAR ALL**, then select **FACTORY LIBRARY**.
3. Select the desired factory row and explicitly press **LOAD**, or **RELOAD** if that is the displayed label. With the result row selected, Enter or double-click also loads it. The status should read `Loaded "program name".` A screen reader continues to identify this control as **LOAD** even when its visible label is **RELOAD**.
4. Play fresh notes. Body model, partial decay, resonant size, variant and material are captured when a note is accepted. Start with a few notes rather than holding the previous chord through the change.
5. Close Library to keep playing the loaded sound, and save the project to retain the new settings. **REVERT** restores the sound present when Library opened; that may be the older recipe. User patches and A/B slots retain their own saved controls.

**AUDITION** loads a newly chosen row, but can leave a row already marked loaded unchanged. Explicit **LOAD/RELOAD** is the reliable update step. The main toolbar's **< / >** arrows load adjacent programs; Library's **PREV / NEXT** buttons select rows and load them only when AUDITION is enabled.

Use Library for the revised named instruments. The panel's hardware tone tabs select their separate electrical tone recipes. If **Pressure Lead** or **Electric Leads 136** is silent, hold a note and add aftertouch; a harder strike alone is insufficient. For on-screen pressure, keep **KAS Scan Timing** off and drag downward on a held key.

The fourteen body controls are exposed to the host's parameter/automation interface. Search there for **Body I model (next note)** or **Body I material (next note)**, and the corresponding rank II names. The custom Studio and Standalone interfaces have no dedicated body-control widgets; factory loads still apply those settings. Mix and brightness respond live; the other body controls require fresh notes.

The guides inside an archive describe its source-time state and can still say qualification was pending or link to older packages. Use the [current online download section](https://github.com/cheshmakzanoon-ops/yamaha-cs80/blob/main/README.md#downloads) for the final results belonging to your package's full source commit.

Source basis and limits: [A43 review](../../reviews/a43.md). These instructions have been checked against source and packaging paths; they are not a record of installing or operating the build in a particular DAW.
