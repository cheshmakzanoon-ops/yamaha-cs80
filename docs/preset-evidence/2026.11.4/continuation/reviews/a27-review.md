# Agent 27 — factory recall and upgrade audit

Source reviewed: `e32728b31d97fc356cac291cb98935fdcd38f803` in `/workspace/scratch/dde4ad85ed5b/yamaha-cs80`. Read-only source audit; no build, listening session, or test execution performed by this agent. No repository edits.

## Outcome

Explicit **Library → factory entry → LOAD / RELOAD** loads the current binary’s compiled factory values, including all ten instrument-body controls. There is no disk-based factory sound cache that must be deleted. Existing project states and saved user presets deliberately retain their stored recipes; upgrading the binary does not replace those recipes merely because they carry a factory name or program index.

I found no new missing-body-parameter or stale-factory-file defect in the reviewed recall, save, browser revert, or preview pathways. A pre-existing VST3 processing-queue-only Program automation limitation remains documented and must not be represented as fixed by this preset work. The source review establishes parameter routing, not perceptual realism or the identity of a binary actually loaded in a DAW.

## Concrete user-facing instructions

1. Close the DAW and standalone application before replacing the installed plug-in with the newly qualified build. Relaunch the host and rescan if it still shows an older installed build; ensure the intended architecture and plug-in copy are selected.
2. Open **Library**, choose the desired **factory** entry, and explicitly click **LOAD** or **RELOAD**. Enter or double-click does the same. A saved user patch with the same name is a different entry and retains its saved values.
3. Play fresh notes after recall. Body model, decay, and size have note-on-latched behavior; already sounding voices are not a reliable comparison of newly selected settings.
4. If an older project already displays the factory name, explicitly **RELOAD** that factory entry anyway. The project can still contain the old parameter values. The browser can mark it EDITED relative to the current compiled factory patch. Clicking an already-loaded row with Audition enabled does not necessarily reload it; **RELOAD**, Enter, or double-click does.
5. Save the project after loading the revised factory recipe if that is the version you want the project to retain. Existing user preset files and stored A/B states likewise retain their own values.
6. **REVERT** restores the sound that was active when the Library opened. It does not mean “restore the newest factory recipe,” and can bring an older saved recipe back.
7. If the DAW’s Program automation does not change the audible sound, select the preset through the plug-in’s Library. Native VST3 processing-queue-only Program delivery has a known limitation; internal MIDI bank/program tests do not certify that distinct wrapper path.

Factory recall intentionally preserves the player/session setup, including velocity and pressure curves, ribbon controller setup, tuning mode, output level/jack, oversampling, drift model, footswitch state, and related controls. These can influence a comparison without indicating that the factory patch failed to load. There is no reason established here to delete user presets or their directory.

## Factory bank and index mapping

`src/plugin/PluginProcessor.h` exposes `cs80::programLibrary()` directly as the factory collection. `src/core/FactoryPresets.cpp:266–312` constructs a static in-process bank by concatenating original-address presets and the curated library. The calibrated volumes are applied when the compiled curated bank is constructed. No filesystem lookup or previously saved user patch supplies factory values.

The total host program count is 523. Zero-based original addresses remain 0–221; curated entries start at 222 and end at 522. The original-address designation is historical provenance, not a separate current DSP engine. `setCurrentProgram(index)` invokes `loadFactoryPreset(index)` and the host program name is read from the compiled bank.

Raw MIDI mapping is separate: bank MSB 0 selects curated entries and MSB 1 selects original-address entries. Bank LSB selects a 128-program page; program is the zero-based slot in that page. Curated in-library index is `LSB * 128 + program`, then the host index is offset by 222. Invalid page/program combinations return -1.

## Recall, complete parameter coverage, and browser behavior

`PluginProcessor.cpp:676` (`applyValues`) preflights and iterates every one of the 200 core parameter slots. It skips physical setup and reserved policy slots by explicit classification, not by a truncated upper bound. None of the ten body controls is a physical-setup parameter. Normal factory selection resets the two hardware sources to PANEL before applying the complete recipe, preventing old tone-button or MEMORY source selections from overriding it. APVTS values are published to the DSP on the next processing block.

The ten body controls are model, mix, brightness, decay for each rank at core positions 190–197, then body size for each rank at 198–199. Host exposure is 306 parameters; the eight preceding body fields occupy host positions 296–303, and the two size controls are 304–305. This audit concerns ten body parameters within 200 core parameters, not 200 body parameters.

`PresetBrowser.cpp:2857` rebuilds entries from the compiled bank. Factory visual signatures are cached in-process for display; those signatures are not the sounding preset state or a persisted audio cache. User XML files are assembled separately. `loadEntry` routes a factory entry to `loadFactoryPreset(factoryIndex)` and a user entry to `loadUserPreset(file)`.

Loaded-row resolution uses factory origin/index or user origin/file, rather than display-name guessing. `selectedRowsChanged` only automatically loads with Audition on and when the entry is not already marked loaded. Searching, filtering, and reopening deliberately restore selection without loading it. The explicit LOAD action remains enabled for the selected entry and changes its visible label to RELOAD when that entry is marked loaded. This existing action handles the older-session/same-factory-index case correctly when the user invokes it.

`capturePresetBrowserSound` captures `currentValues()` over all 200 slots, source selectors, memories, modern configuration, and metadata. `restorePresetBrowserSound` validates and reapplies that full sound state; it does not omit the body suffix. Browser edited/revert comparisons also iterate the complete parameter range, excluding the deliberate setup/policy fields. User-file signature/value parsing builds its ID map over all core parameters.

## State and user-preset compatibility

`getStateInformation` serializes APVTS values together with preset metadata and library/A/B state. `setStateInformation` restores normalized values and metadata; it does **not** reinterpret the stored factory index as a request to load today’s factory recipe. This protects authored sessions. `normaliseRestoredState` checks all current parameter definitions, clamps/rounds malformed values, and supplies defaults for fields absent from older states.

User save/load uses the complete APVTS state. Older pre-body recipes acquire neutral body defaults (Off); earlier eight-body-control recipes retain those controls and receive neutral size 1.0. Newer body-aware saved states keep their own body values. A DSP algorithm correction can still change rendering of an old recipe that uses that model; preserving the recipe is not a promise of bit-identical audio across algorithm changes.

## Existing tests reviewed, not rerun

`tests/InstrumentBodyHostTests.h` contains source-level tests for the 200-core/306-host layout; stable prior parameter prefix hashes; AU version hints; all ten body controls’ ranges/defaults; session and user-file roundtrips; schema migrations; malformed value handling; active and inactive A/B states; undo/redo; and factory selection over all 523 programs replacing every body control, including neutral defaults.

The same suite iterates internal MIDI bank/program recall over all programs and compares host values with the engine. It tests same-block program-change/note behavior and transitions involving active body voices. This is relevant recall coverage, but its existence is not an execution result and its raw MIDI route is not the native VST3 Program queue route.

## Known bug and recommendations

**Known existing integration defect:** `docs/preset-evidence/2026.11.1/program-queue/method.txt` and `docs/PRESET_REFINEMENT_2026_11_1.md:270–286` document that a VST3 Program change sent only through the processing queue fails to select the sound in the tested native modules, while explicitly updating the controller selects it. The pinned JUCE auxiliary Program parameter path suppresses the relevant controller callback; Program is not an APVTS lane accepted by the generated automation bridge. Ordinary parameter automation is a separate passing path in that diagnosis.

Do not claim queue-only Program automation, sample-accurate program switching, or arbitrary-DAW selection certified by this source review. Repairing that path is a separate focused wrapper/host-integration task with native regression evidence, not a preset-value change.

**Release communication recommendation:** Include the explicit **RELOAD the factory entry after opening an old project** instruction beside the new download links. This is the principal user-facing trap identified here. The existing UI already offers RELOAD and an edited-state explanation; no production UI edit is required for this audit.

**Verification recommendation:** During final build qualification, check that the installed artifact reports the expected release/source identity, then use Library RELOAD and fresh notes. Do not judge successful factory migration from the preset name alone. Keep the current session-compatibility behavior rather than silently replacing saved recipes.
