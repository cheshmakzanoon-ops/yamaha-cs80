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
