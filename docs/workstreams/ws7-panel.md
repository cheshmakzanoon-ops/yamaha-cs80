# ws7-panel: original-panel UI

Workstream report, 2026-10-31. Plan steps: 5.6 editor code health, 5.1 panel conformance audit, 5.2 main-view
re-layout, 5.5 readability and rendering, memory-lid editing (from Phase 6 "Editing"), 5.7 golden-image diffs.
Branch base: integration commit `6719ce4`.

No DSP, engine or host-parameter change is made by this workstream: no new parameters, no circuit-engine sound
change, and the compatibility gates are unaffected (see the final section).

## Step 1: 5.6 editor code health (done)

All changes are behaviour-preserving. Verification method: the UI self-test captures (`CS80Snapshots --out
<dir> --self-test`, Xvfb + Openbox via `scripts/run-ui-smoke-linux.sh`) before and after each commit, compared
per pixel with a scratch PIL script. The live readouts (output meter and voice/MIDI lamps, design rectangle
1118-1460 x 0-48, and the keybed, whose held host keys depend on render timing) are masked.

| Change | Where | Pixel result |
| --- | --- | --- |
| Deterministic fader-cap variant (keyed by parameter ID or title, not heap address) | `UiComponents.cpp` `drawLinearSlider` | Intentional one-time re-shuffle of the four cap mouldings. Two consecutive runs now give identical PNGs (masked max diff 0); before, they differed run to run |
| `ArtCommon.h/.cpp`: one copy of the panel typeface (`displayFont`, `spacedFont`, `textWidth`), the design-space floor (`FloorScope`), the wrinkle tile with `fillRelief`/`fillTile`, and the printed, etched and machined painters | new `src/plugin/ArtCommon.*`; `PluginEditor.cpp` and `UiComponents.cpp` use it | Identical |
| Dead code removed: `makeRelief`, `louvre`, `stamped`, `matteSheet`, `RovingFocusGroup` (no callers anywhere in `src/` or `tests/`), later also the unused `hgradShadow` | `PluginEditor.cpp`, `UiComponents.h/.cpp` | Identical |
| `PaintCache` evicts least-recently-used images at 700 entries instead of clearing all | `UiComponents.cpp` | Identical |
| Editor `Impl` split into `ChannelRow`, `CentreRow`, `PerformanceDeck` and `MemoryLid` components, with a `PanelSection` base that places controls in design units and rounds them in editor space (no control moves by a pixel); `KeyButton`/`ChannelTag` to `PanelWidgets.h`, geometry to `PanelLayout.h`, the voice-flow plate to `VoicePlate.h`. No Toolbar component (ws8 owns it) | new `src/plugin/{PanelWidgets,ChannelRow,CentreRow,PerformanceDeck,MemoryLid}.*`, `PanelLayout.h`, `VoicePlate.h` | Identical. `PluginEditor.cpp` 4,390 -> 2,142 lines |
| Memory-lid art cached per size, physical pixel scale and lit-row set | `MemoryLid.cpp` | Inside the lid 30,916 pixels differ, 30,691 of them by 1-2/255, max 8/255 (8-bit premultiplied compositing through the cache image). Outside the lid identical |
| 8 Hz full-parameter polling in `syncPresetDisplay` replaced by `ParameterChangeWatch` (parameter listeners that only raise an atomic flag); the display recomputes only when a parameter or the program metadata changed | new `src/plugin/ParameterWatch.h`, `PluginEditor.cpp` | Identical |

One capture (`memory-programmer-selected`) differs in the status strip between runs at every commit: the
toolbar labels follow the 8 Hz timer and the 100 ms flush before that capture sometimes misses a tick. This is
pre-existing; the golden-image test (step 6) waits for a refresh instead.

Behaviour notes for the integrator:

- Keyboard focus now walks section by section (row I, row II, centre strip, deck) instead of one global y/x
  sort that interleaved the plate keys with the row faders. No test pinned the old order.
- Before the parameter watch, every idle tick copied ~300 host values, compared every core parameter and
  scanned the 222-program bank by name. Now an idle tick does none of that.

Tests added: `tests/PanelEditorTests.h` (run by the UI self-test) — host automation of one control marks the
program as edited and restoring it clears the mark. Mutation check: a `ParameterChangeWatch::consume()` that
never reports a change makes the self-test fail with "host automation of one control marks the program as
edited" (1 failure); restored, it passes.

## Step 2: 5.1 panel conformance audit (done)

[docs/PANEL_CONFORMANCE.md](../PANEL_CONFORMANCE.md): physical areas, a control-by-control table for owner index
1-46 (position, group, legend, cap colour, lever direction, switch type, editor), the tone-selector wording and
seven recorded source conflicts.

Reachability: the owner's manual scans, the S2 panel drawing and the Yamaha museum photo were **not reachable**
from this session (the proxy refuses therogoffs.com, synthfool.com and yamaha.com with HTTP 403 by policy;
`inspect_circuit_sources.py` has no S2 entry and the CI workflow cannot be triggered from here). The audit
therefore rests on page citations already recorded in the repository's evidence documents and labels every
position, spacing and proportion that S2 would settle as a declared inference.

Main findings that drive the re-layout: the upper-left element is the hinged block-diagram lid over the
memories; keyboard contour VR19-22 is on Panel 2 (the front strip); the assigners and EXP / EXP-WAH are on Panel
3 with sustain and portamento (left of the keyboard); the footswitch jack and output switch are on Panel 5 (jack
panel); the adjustment procedure names separate TREMOLO, CHORUS and ON/OFF tabs.

## Step 3: 5.2 main-view re-layout (done)

The main view now follows the audit. One table, `src/plugin/PanelLayout.h`, holds every block in final design
coordinates; the `midShift`/`shifted()` 24 px hack is gone (the old shifted numbers became the real ones).
Sections compute their columns and group brackets from that table, so a bracket always spans the columns it
names.

| Change | Evidence class |
| --- | --- |
| Live signal-flow plate removed from the main view; the upper-left element is now the closed memory lid (hinge, printed block diagram, OPEN MEMORY LID and VOICE SETTINGS keys). Software voice settings open as a tray over the diagram | D placement (HR: S2 summary, Yamaha museum); I proportions (288 x 378); the printed diagram is a stylised drawing of this instrument's path, not the original print |
| Channel rows span from the lid to the panel edge (x 348-1552; 43 px column pitch, was 40) | D order; I spacing |
| Keyboard contour (BRILL LOW/HIGH, LEVEL LOW/HIGH) moved from the deck to the front strip, after RESONANCE | D board (KCE: Panel2 p64 VR19-22); I position (owner index 7-10 consecutive) |
| Footswitch assigners and EXP / EXP-WAH (with the on-screen pedal) moved to the deck's left block with sustain, portamento and tremolo/chorus | D (FCE: Panel3 p66; HR: left of keyboard) |
| Footswitch jack (PLUG), contact (HOLD) and output HIGH/LOW moved to the deck's right block, labelled REAR PANEL | D board (FCE J5, OLR PN5 p67 SW1); I rear location |
| OFF/CHORUS/TREMOLO 3-way selector replaced by separate ON/OFF, TREMOLO and CHORUS tabs over the unchanged `chorus_on` / `chorus_mode` (no ID or semantics change) | D tab names (BA617: adjustment p21); C with the owner reading, recorded |
| Tone tabs print the full hardware names (STRING 1 ... ELECTRIC PIANO ... HARPSI-/CHORD 1 ... PANEL) instead of STR/BRS codes | D (S1, O37) |
| The engraved I/II numerals are labels now; their pilot lights while the row is heard (selector on PANEL) | software indication |
| Ribbon lengthened to 860 px between the two deck blocks; nameplate centred over it | D (long ribbon above the keys) |

Front-strip width is the binding constraint: 24 levers, two rotaries and a 14-column selector with full names
share 1,496 px, so the strip levers are 27.5 px wide on a 29.25 px pitch (row levers 40 px). Small editors
abbreviate their captions (DET, ATK, FUNC, SPD ...); the full words remain in titles, tooltips and accessibility
names.

`VoicePlate.h` keeps the live voice-flow display class unmounted, as the mount point for the Studio drawer
(Phase 5.3, ws8). The voice-plate touch tracking, view keys and channel selection were removed with it.

Tests added (`tests/PanelEditorTests.h`, UI self-test), at 1600 x 1000 and 1120 x 700:
no two visible main-view controls overlap; every control lies inside the editor; contour levers inside the front
strip; assigners, EXP/WAH, pedal and the three tremolo/chorus tabs inside the left block; PLUG, HOLD and OUTPUT
inside the rear block; the lid key on the lid; rows right of the lid; full tone names present; the three tabs
drive `chorus_on`/`chorus_mode` with one balanced gesture each, keep the choice when switched off, and follow host
automation. Mutation checks: strip lever pitch below the lever width fails the overlap check at both sizes;
TREMOLO writing mode 0, the core "E. PIANO" name, and PLUG moved to x 600 each fail their check (3 failures in
one run); all pass when restored. `src/tools/ui_snapshot.cpp` now looks the populated-capture tabs up by their
new text ("BRASS 1").

## Step 4: 5.5 readability and rendering (done)

**Typeface.** Barlow Condensed 1.408 (Medium for plain, SemiBold for bold), SIL OFL 1.1, Copyright 2017 The
Barlow Project Authors. Fetched 2026-10-31 from the google/fonts repository
(`https://raw.githubusercontent.com/google/fonts/main/ofl/barlowcondensed/`), unmodified, committed with its
`OFL.txt` and a `SOURCE.txt` (URLs, SHA-256) in `assets/fonts/barlow-condensed/`. Embedded with
`juce_add_binary_data` (target `cs80_panel_fonts`, linked to `CS80Reference` and `CS80Snapshots`) and loaded
once by `cs80::art::panelTypeface()`; `cs80::art::displayFont()` falls back to the system sans serif if a face
cannot be loaded. Everything that draws through `ArtCommon` / `UiComponents` uses it. `AdvancedPanel.cpp` (ws8)
and `PresetBrowser.cpp` (ws6) still have their own font helpers and keep the system face until they adopt
`ArtCommon`.

**Legend floor.** `cs80::art::kLegendFloorPx = 10.5` screen px at every size: the design-space floor
(`FloorScope`, was 9 px), fader captions (was 9.5), tone tabs (was 9), fader end legends (was 9), LED, rocker and
panel-key legends (were 7.5-9), ribbon and wheel legends (were 8-9), the status strip (was 9.5), the output-meter
readout (was 9) and the lid readout. Fader value readouts have a 10 px floor (was 9). Words that do not fit are
abbreviated rather than shrunk: at 1600 x 1000 every front-strip caption prints in full except FUNCTION (FUNC);
at 1120 x 700 the strip prints short forms (DET, ATK, DEC, DPT, SPD, FNC, FT I, BRLL, BND, ...). Tone tabs keep
the floor by squeezing to 0.70 at most, so ELECTRIC and HARPSI- still print whole at the minimum size. Not
covered: the toolbar's program-display tag and index (12 design px boxes, 7.5 px floor), which ws8 is
rebuilding; the shared LookAndFeel label/combo/button fonts used by the Advanced and library pages.

**Contrast (WCAG 2.x, AA 4.5:1).** Computed with the WCAG relative-luminance formula against the nominal
backgrounds; text printed on the panel is measured against the brightest of five empty crinkle patches in the
1600 x 1000 capture (#343331), i.e. the worst case.

| Readout | Ink | Background | Ratio |
| --- | --- | --- | --- |
| Fader value readout | #c4ba96 | panel #343331 | 6.5:1 |
| Fader caption | #e6dcbc | panel #343331 | 9.2:1 |
| Drag / hover value pill | #ffd47a at 95 % | pill glass #0b0a08 | 12.7:1 |
| Status strip | #eee4c4 | strip #0b0b0a | 15.5:1 |
| Output meter peak / OVER-RANGE / NaN | #c9bd98 / #ffd47a / #ff7166 | meter glass #0e0f0c | 10.3 / 13.7 / 7.1:1 |
| Preset name / factory number | #ffcf70 / #ffb347 | lit well #3a2410 | 10.0 / 8.2:1 |
| Preset number, user (USR) | **#c28c42** (was #9a6a2c, 3.1:1) | lit well #3a2410 | 4.9:1 |
| Preset category tag / index | #d09a3c / #b88a3a | well #1a0f07 | 7.5 / 6.0:1 |
| Memory-lid readout / hint | #ffc060 / #b6aa86 | readout #0e0d0b | 12.0 / 8.4:1 |
| Memory-lid NOT ACTIVE | **#b3aa8a** (was #9a9174, 4.0:1) | panel #343331 | 5.4:1 |

**Zoom presets.** 75, 100, 125, 150 and 200 % of 1600 x 1000 (1200 x 750 ... 3200 x 2000), chosen from a
DISPLAY SIZE menu on the toolbar's CS-80 brand mark or through `Cs80AudioProcessorEditor::setZoomPercent()`.
The choice is a user preference (`EditorPreferences`, a JUCE `PropertiesFile` in the user's application-data
folder), not a host parameter or session state; new editor windows open at it when it fits the screen with the
same host-chrome margins as the automatic size, else at the automatic size. A window dragged to its own size
keeps that size. The maximum resize limit rises from 2400 x 1500 to 3200 x 2000 so 200 % fits
(`tests/EditorInitialSizeTests.h` pinned the old maximum and is updated with a comment). The snapshot tool points
the preference store at a scratch file, so tests never read or write the user's real preferences.

**HiDPI.** The chassis, the closed lid and the opened lid tile the crinkle at the graphics context's physical
pixel scale (device pixels per design unit) instead of the logical editor scale, and the walnut cheeks are built
at it too, so a 2x display gets one texel per physical pixel. Measured on a plain deck patch, mean neighbouring
brightness difference: 1x render 0.0201, 2x render 0.0217 with the fix, 0.0065 without it (texture magnified 2x).

Tests added (`tests/PanelEditorTests.h`): both typeface weights load and legends use them; a design-space
legend never prints below 10.5 px at 1120 x 700; zoom presets resize exactly, are remembered, ignore non-preset
values, report 0 for a dragged size, reopen at the stored preset, fall back when it does not fit, and never touch
the APVTS state; a 2x render keeps at least 80 % of the 1x crinkle detail. Mutation checks, one build: floor back
to 9 px, typeface bypassed, texel scale back to the logical scale, preference not saved; 6 failures, all
passing again after reverting.

## Step 5: memory-lid editing (done)

`src/plugin/MemoryEditing.h` (header-only, message thread) adds four processor-side helpers over the 26
programmer controls (`Cs80AudioProcessor::isMemoryControl`), writing the existing APVTS memory and panel
parameters with one balanced gesture per parameter that actually changes:

- `storePanelToMemory(processor, channel, bank)`: a rank's main-panel row into a memory;
- `copyMemory(processor, fromBank, toBank)`: one memory into another (any distinct pair);
- `copyRank(processor, fromChannel, toChannel)`: one rank's row onto the other; FEET, pan and the filter on/off
  extensions are not part of the row and are left alone;
- `recallMemoryToPanel(processor, bank, channel)`: a memory into a rank's row (the reverse of store; the Phase 6
  "copy memory and panel" item).

Invalid arguments return false and change nothing. Evidence class: these are editing conveniences with no
counterpart on the original, whose memories are miniature panels set by hand. They only produce settings an
owner could reach by moving those sliders and write no non-original state, so they are available in every
engine mode, PURE CS-80 included (declared choice; the alternative is to grey the keys out in isolation).

Lid UI: STORE PANEL (store a row into either of its rank's memories, or load a memory into its rank's row),
COPY MEMORY (from each memory to any other) and COPY RANK (I to II, II to I) keys in the lid header open menus
of explicit commands; `MemoryLid::runEditCommand()` performs one and the readout reports it ("STORED PANEL I IN
MEMORY 3 ... YOUR HOST'S UNDO REVERTS IT") for 2.5 s. The readout window narrows to make room and now scales
uniformly. Host undo works through the gestures; the APVTS UndoManager (ws8) will see them as ordinary edits.

Tests (`tests/PanelEditorTests.h`, UI self-test): with independently chosen values in both rows and all four
memories, each helper makes its target equal to its source for all 26 controls and leaves the source, the other
memories and the other row unchanged; rank copy leaves FEET and pan; every write is a balanced gesture with no
unbracketed change; six invalid requests are refused with the full state byte-identical; the lid shows its three
keys and its command entry point stores panel II into memory 4 and refuses unknown or invalid commands.
Mutation checks, one build: store writing the next bank, and rank copy including FEET; 4 failures, passing
again after reverting.

## Step 6: 5.7 golden-image diffs (done; strict on Linux only)

`testGoldenImages` in `tests/PanelEditorTests.h` (UI self-test, so it runs under `ctest -R ui_smoke`) captures
the main view of a fresh processor and editor (Init Patch, no audio rendered, no pointer or focus) at
1600 x 1000 and 1120 x 700 and compares it with `tests/golden/main-1600x1000.png` and `main-1120x700.png`. The
output meter and voice/MIDI lamp well is masked. Tolerances, named constants fixed before the goldens were
recorded: a pixel counts as changed above 40/255 in any channel; a view fails when more than 0.30 % of its pixels
change or when any 8 x 8 block's mean absolute difference exceeds 8/255. Each run writes
`golden-candidate-*.png` and `golden-diff-*.png` into the capture directory. The golden directory is passed to
`CS80Snapshots` as `CS80_GOLDEN_DIR` (CMake, ws7 block).

**Refresh:** after an intended visual change, run on Linux
`CS80_REFRESH_GOLDENS=1 scripts/run-ui-smoke-linux.sh build/CS80Snapshots_artefacts/Release/CS80Snapshots --out <dir> --self-test`,
inspect the files written to `tests/golden/`, and commit them with the change. The same instructions are in
docs/UI_DESIGN_SPEC.md.

**Cross-platform:** the comparison fails the self-test on Linux only. On macOS and Windows it prints its metrics
and does not fail. Reason: the goldens are recorded on Linux and the other platforms' text rendering has not been
characterised against them from this session. The gate is sensitive to text: rendering every legend 1 % larger
moves 0.32 % (1600) and 0.43 % (1120) of the pixels, with worst block means of 173 and 92, partly because some
captions cross an abbreviation threshold. A strict cross-platform assertion would therefore be guesswork until a
CI run on each platform shows its actual difference.

Measured: two consecutive Linux runs and the post-mutation rebuild match the goldens exactly (0.0000 % changed,
worst block mean 0.00). Mutation check: legend height x1.01 fails both views as above; restored, both match
exactly.
