# Workstream handover

The eight parallel workstreams are wound down and the remaining work moves to a
single project manager with no sub-agents. This page records, per workstream,
what it did, how it reasoned, and what it left open. Full detail, evidence
classes and mutation checks are in each `wsN-*.md` report beside this file.

Integration branch: `claude/adoring-fermi-alhg4x`. Every workstream branched from
`6719ce4` (scaffolding: append-only markers, `isPureCs80()`, and
`scripts/check-compat-gates.sh`).

| Workstream | State | Merged as |
| --- | --- | --- |
| ws1-voice | running at hand-over time | — |
| ws2-engine | running at hand-over time | — |
| ws3-keyboard | running at hand-over time | — |
| ws4-circuits | all four steps done; EXP volume optics handed off | `195e5e3` |
| ws5-modern | running at hand-over time | — |
| ws6-library | running at hand-over time | — |
| ws7-panel | all six steps done | `bd524f9` |
| ws8-studio | all six steps done | `5af3b77` |

## ws4-circuits — original modulation and effect circuits

**What it did** (branch tip `368fd9e`, report [ws4-circuits.md](ws4-circuits.md)):
- **SUB (IG00150).** An exponential speed converter that adds touch as octaves.
  Full SPEED plus full touch reaches the documented 200 Hz point; reading that
  point as the combined maximum is a declared inference. Saw and square resets are
  band-limited (alias at 173 Hz: −28 → −84 dB).
- **Ring modulator.** An exponential converter through the ADJ p19 anchors
  (rendered 150.01 ms and 5.000 ms periods). The multiplier has carrier and signal
  feedthrough and a DC offset (declared values). PURE CS-80 masks the 500 Hz
  extension at 200 Hz.
- **OE1/OE2.** Per-transfer BBD loss, output-pair hold, independent noise per
  chain, band-limited clock feedthrough, true L/R wet paths, and an OE1 four-VCA
  tremolo. Phase error against the truth models at 10 kHz: 0.60 → 0.098 rad.
- **PRA wah.** An optical cell that brightens in 12 ms and darkens in 60 ms
  (declared LDR model, checked against an ODE truth model).

**How it reasoned.** Tolerances were committed before each implementation and
every gate was mutation-checked. It left the V2 audio pin to the integrator. It
did not touch the EXP volume path because that gain sits on the output line ws2
was changing for CC11 smoothing.

**Open.** EXP volume optics: route the circuit-engine expression value through
the same log-R cell before the output gain, then update the expression oracles
in `OutputTapIntegrationTests` and `PraWahIntegrationTests`. Re-pin
`SOURCE_AUDIO_V2`.

## ws7-panel — original-panel UI

**What it did** (branch tip `b9b22f2`, report [ws7-panel.md](ws7-panel.md),
[PANEL_CONFORMANCE.md](../PANEL_CONFORMANCE.md)):
- **Code health first, pixel-identical:**
  - `ArtCommon.h/.cpp` holds the single copy of the panel helpers.
  - Dead code is removed.
  - The editor is split into ChannelRow, CentreRow, PerformanceDeck and MemoryLid.
  - The fader-cap variant is keyed by parameter ID.
  - PaintCache is now LRU.
  - Parameter listeners replace the 8 Hz polling.
- **Conformance audit:** owner's-manual index 1–46, with seven source conflicts
  recorded openly.
- **Re-layout:**
  - The live signal-flow plate is off the main view; channel rows span the panel.
  - The keyboard-contour levers are on the front strip.
  - The foot assigners and pedal are in the left deck; the rear-panel block is labelled.
  - Tremolo and chorus are separate keys over the unchanged parameters.
  - Tone tabs carry their full names.
  - One layout table replaces the `midShift` hack.
- **Readability:**
  - Embedded Barlow Condensed (OFL, with source and checksum).
  - 10.5 px legend floor.
  - WCAG fixes (3.1:1 → 4.9:1, 4.0:1 → 5.4:1).
  - Zoom presets 75–200 %.
  - HiDPI textures at physical scale.
- **Memory lid:** STORE PANEL, COPY MEMORY and COPY RANK in `MemoryEditing.h`.
- **Golden images** at both sizes, strict on Linux.

**How it reasoned.** The panel drawings online (therogoffs.com, synthfool.com,
yamaha.com) were refused by the proxy, so the audit rests on the page citations
already in the repo's evidence docs. Spacing and proportions are labelled
declared inference. It kept memory editing available in PURE CS-80 because it only
produces settings reachable by hand.

**Open:**
- Verify against the S2 drawing once `inspect_circuit_sources.py` can fetch it.
- Global lever direction.
- The "both off/both on" tremolo-chorus states need an engine parameter.
- Strict goldens on macOS and Windows.
- `VoicePlate.h` is unused and no longer compiles against `PanelLayout.h`
  (superseded by ws8's voice monitor drawer).

## ws8-studio — modern UI layer

**What it did** (branch tip `a7863af`, report [ws8-studio.md](ws8-studio.md)):
- **Toolbar:**
  - Program display and prev/next.
  - LIBRARY, A/B and UNDO/REDO.
  - ENGINE CIRCUIT | LEGACY (`source_performance`) and PURE CS-80 (`parity_mode`).
  - STUDIO and a menu with a display-size submenu.
- **Studio view:** replaces Advanced, with five tabs built from the existing
  modules. It adds model-notes cards, a voice monitor drawer, and a controllers
  strip with the pitch and mod wheels, hidden in PURE CS-80.
- **Interaction polish:**
  - Faders drag relative to the cap, with Shift for fine moves; Ctrl/Cmd-click types a value; double-click resets.
  - A context menu offers MIDI learn, reset, typed value, copy/paste and help.
  - The full ribbon range is reachable from any touch point (declared UI inference; PURE keeps the hardware law).
  - QWERTY play.
  - Owner's-manual tooltips with "Circuit notes" showing the evidence class.
- **MIDI learn:** a lock-free map, saved as `MIDI_LEARN v1` and inert in PURE.
  Protocol controllers cannot be learned.
- **Undo/redo:** through the APVTS UndoManager, one step per gesture. Host
  automation, learned MIDI, preset loads and restores are never recorded.
- **Learning tools:** a 9-step tour, F1 help cards citing the owner's-manual page
  and evidence class, and an evidence view compiled in `EvidenceRegister.h`.
- **UI tests:** tab order, WCAG 2.5.8 24 px targets, contrast, context menu, MIDI
  learn round trip and undo.

**How it reasoned.** It reused working AdvancedPanel UI instead of rewriting it.
It merged the integration branch itself after ws7 landed, adapting its mount
points to ws7's split. The hit-target test is now strict for the whole editor,
with two documented "essential" exceptions that reproduce the original lever
row: the 23 front-strip levers at 19.3 px and the 3 tremolo/chorus keys at 18 px.

**Open:**
- Mount the ws5 and ws3 pages through `StudioView::setPage` / `addPage`.
- Wire `StudioToolbar::Actions` to ws6's library and A/B, then delete `CompareSlots`.
- Wrap randomise in `ScopedUserEdit`.
- Context menu and MIDI learn for switches, combo boxes and the lid miniatures.
- Undo-applied values reach the host without begin/end gestures.
- The voice monitor illustrates panel settings rather than tapping the engine.
