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
