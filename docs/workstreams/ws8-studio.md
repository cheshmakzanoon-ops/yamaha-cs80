# ws8-studio — the modern UI layer

Workstream report, 2026-10-31. Branch base: integration commit `6719ce4`. Plan steps: 5.3 (toolbar, Studio
view, drawer, controllers strip), 5.4 (interaction polish), the Phase 6 MIDI learn, undo/redo and learning
tools, and the 5.7 UI tests with an accessibility review.

This workstream changes no sound. It adds no core or host parameter (the AU version hint 15 is unused): MIDI
learn and the editor state live in the plugin state. `legacy default`, `legacy tails`, `v1 circuit numerics`
and `v2 circuit-engine audio` all stay byte-identical (see Verification).

## Summary

| Step | State | Where |
| --- | --- | --- |
| 5.3 Toolbar | done | `src/plugin/StudioToolbar.h/.cpp`, mounted in `PluginEditor.cpp` |
| 5.3 Studio view (replaces Advanced) | done | `src/plugin/StudioView.h`, `AdvancedPanel.cpp/.h` |
| 5.3 Voice monitor drawer | done | `src/plugin/StudioDrawers.h/.cpp` (`ScopeDrawer`) |
| 5.3 Controllers strip | done | `src/plugin/StudioDrawers.h/.cpp` (`ControllersStrip`) |
| 5.4 Faders, typed values, context menu | done | `src/plugin/UiComponents.h/.cpp` (`FocusSlider`, `ParamKnob`) |
| 5.4 Ribbon full range | done | `UiComponents.cpp` (`RibbonStrip`) |
| 5.4 QWERTY play | done | `src/plugin/HardwareKeyboard.h` (`QwertyKeyboard`) |
| 5.4 Tooltips with Circuit notes | done | `UiComponents.cpp` (structured tooltip), `src/plugin/EvidenceRegister.h` |
| Phase 6 MIDI learn | done | `src/plugin/MidiLearn.h`, `PluginProcessor.h/.cpp`, Studio KEYBOARD tab |
| Phase 6 Undo/redo | done | `src/plugin/UndoHistory.h/.cpp`, `PluginProcessor.h/.cpp`, toolbar, shortcuts |
| Phase 6 Learning tools | done | `src/plugin/StudioTour.h/.cpp`, help cards, `EvidenceRegister.h`, Studio SETTINGS |
| 5.7 UI tests | done | `tests/StudioUiTests.h`, appended sections in `src/tools/ui_snapshot.cpp` |
| 5.7 Accessibility review | done, with open items for the panel re-layout | this document |

## 5.3 Tuck away the modern items

### Toolbar

`StudioToolbar` is a slim recessed console rail mounted over the editor's former program cluster (design rect
x 250-1114, y 2-46 of the 1600 x 1000 design space). From left to right: previous, the program display (number,
category, name, EDITED), next, LIBRARY | A, B | UNDO, REDO | ENGINE CIRCUIT / LEGACY | PURE CS-80 | STUDIO |
menu. The editor's brand badge, voice/MIDI lamps, output meter and PANIC stay where they were.

- **ENGINE** is a two-segment switch bound to the existing `source_performance` parameter (CIRCUIT = 1, LEGACY
  = 0); **PURE CS-80** is bound to `parity_mode`. Both use `juce::ParameterAttachment` with complete gestures
  (one undo step each). Host IDs are unchanged; the toolbar never reads the host-visible names, so ws2's
  renaming and default flip need no change here. While PURE CS-80 is on, the ENGINE segments are drawn masked
  (still clickable) and their description says the stored choice applies again when PURE CS-80 is off.
- Every other key goes through `StudioToolbar::Actions` (std::function callbacks). A key without a connected
  action is disabled; a menu entry without one is left out. The mount wires them locally: prev/next to the
  editor's existing `stepPreset` with its press-twice discard guard (the toolbar shows the armed `?`), LIBRARY
  and the program display to the library overlay, STUDIO to the Studio view, undo/redo to the processor.
- **A/B**: ws6 builds the real compare. Until it is connected, `CompareSlots` (in `StudioToolbar.h`) is a local
  fallback: two snapshots of every core parameter (minus physical setup and PURE CS-80) plus both rank
  selectors; the first switch to an empty slot copies the open one. Switching is one undo step.
- **Menu**: save preset (Ctrl/Cmd+S), init patch, copy A to B, undo/redo with step names, voice monitor,
  controllers strip, computer keyboard on/off, circuit notes in tooltips, clear MIDI learn (n), take the tour,
  evidence register.

### Studio view

The Advanced panel is now `StudioView` (declared in `StudioView.h`; `AdvancedPanel.h` keeps `using
AdvancedPanel = StudioView;`, so the editor and the existing tests compile unchanged). Five back-lit tabs:
MODULATION, KEYBOARD (MPE and tuning), FX, ARP, SETTINGS. The existing modules were not rewritten: each former
page builder was split into module builders that still draw at their original coordinates, and a new
`AdvancedPage::Origin` scope moves a module to its place on a Studio tab.

| Tab | Sub-pages and modules |
| --- | --- |
| MODULATION | LFO / WHEELS: LFO 1-3, Wheels, model notes (sub oscillator, ring, modern layer). SUB / RING: the former page 2 (sub oscillator, ring modulator, chorus/tremolo, analog voice) |
| KEYBOARD | Computer keyboard, MIDI learn map, Pitch/bend/tuning (moved), keyboard and touch model notes |
| FX | the former page 3 (insert effects, effect detail, macros, output) |
| ARP | Arpeggiator, Performance (key release, glide), envelope and voice model notes |
| SETTINGS | Engine mode (the former render-policy table, moved up) and the full evidence register |

Page registration for ws5 (modulation and FX pages) and ws3 (tuning page):

```cpp
studio.setPage(StudioTab::modulation, std::make_unique<ModulationPage>(processor), "MSEG / MATRIX");
studio.addPage(StudioTab::keyboard, std::make_unique<TuningPage>(processor), "TUNING");
studio.removeBuiltInPage(StudioTab::fx);   // once a registered page carries every built-in control
```

`setPage` puts the component first in its tab and keeps the built-in page as a later sub-page, so nothing
disappears until the integrator removes it. A registered component gets `StudioView::contentDesignArea()`
(below the header rail, above the footer, inside the cheeks; `getContentBounds()` in pixels) and paints over the
chassis. Up to three sub-pages per tab (the header has room for three keys; a fourth asserts).
`showPage(int)` keeps the former page numbers (0 LFO/wheels, 1 sub/ring, 2 FX, 3 render policy, now on
SETTINGS), which the existing smoke and readiness tests use.

### Voice monitor drawer

ws7 removes the 360 x 378 signal-flow plate from the panel; `ScopeDrawer` is where its content lives on: an
optional drawer (design 420 x 470 at x 56, y 70) with the channel I/II choice, the eight voice cards' activity
lamps and count, the VCO -> HPF -> LPF -> VCA path with the sine bypass, both envelopes and the HPF/LPF
response, all computed from what is sounding (PANEL, the selected tone, or the memory bank, as the processor
resolves it). It polls at 15 Hz only while open and repaints only when its inputs changed. The filter plot is
labelled "illustration of the panel settings, not a measurement": it is a resonant second-order sketch, not the
circuit engine's nonlinear IG00156 model.

### Controllers strip

The CS-80 has no pitch or mod wheel. The editor's two `PerformanceWheel`s are taken out of VOICE SETTINGS (my
mount block removes them from `optionComponents`) and the controllers strip carries its own pair, calling the
same `setPitchWheelFromUI` / `setModWheelFromUI` mailboxes. It is a small block over the left end of the keybed,
off by default, toggled from the menu, saved in the editor state, and hidden whenever PURE CS-80 is on (shown
again when it is switched off).

### Editor mount points

All editor edits are delimited `// [ws8-studio]`: one include, one member (`std::unique_ptr<ui::StudioMount>
studio`, declared last so it is destroyed first), one block at the end of the `Impl` constructor, one block in
`resized()`, and one line in `~Impl()` removing the key listeners. The block hides the replaced cluster
(`prevButton`, `nextButton`, `presetWindow`, `browseButton`, `saveButton`, `initButton`, `advancedButton` and the
four preset labels, which keep updating for the existing timer test), wires the actions, adds the drawers, the
tour, the QWERTY and shortcut key listeners, and sets the ribbon's full-range rule.

## 5.4 Interaction polish

- **Faders** (`FocusSlider`): `setSliderSnapsToMousePosition(false)`, and a linear fader's full-range drag
  equals its own track length, so the cap follows the pointer one to one and a click away from the cap never
  moves the value. Shift scales the drag by `kFineFactor` = 0.1; the scaled position is accumulated per move,
  so Shift can be pressed or released mid-drag without a jump. JUCE's velocity mode can no longer be swapped by
  a modifier, which frees Ctrl/Cmd. Double-click resets (JUCE's own path, one gesture).
- **Typed values**: Ctrl-click (Cmd-click on macOS) or the menu opens an editor over the control with the value
  as shown. `parseParamValue` reads the units the control displays: `1.2k` and `3.6 kHz` on a cutoff, `80 ms` or
  a bare `80` on an attack showing ms, `52 %`, `68/32` on MIX, `-0.5 st`, `5` on a 0-10 resonance, choice names
  (`GLISS`, unique prefixes). It derives each control's display law from two sample points of its own formatter,
  refuses a value in another unit (Hz into a time) and clamps to the range. Return commits in one gesture; Esc
  or losing focus cancels.
- **Context menu** (right-click, Ctrl-click on macOS, also on a knob's caption and readout): MIDI learn (arm /
  cancel / learn a different controller / forget CC n; unavailable in PURE CS-80), reset to default (showing
  it), type a value, copy value, paste value (showing the parsed value, disabled when the clipboard is not a
  value of this control), and "What is this control?". Every edit is one host gesture and so one undo step.
  Scope: all `ParamKnob` faders and knobs (main panel and Studio). Switches, combo boxes and the memory lid's
  miniature sliders have no menu yet (open item).
- **Ribbon**: the bend is the displacement from the first contact over the travel left in that direction, so
  sliding from any touch point to either end of the touch strip gives the full bend (a minimum travel of 12 %
  of the strip avoids an infinitely steep bend at an end). Evidence class: **declared UI inference**. The
  owner's manual (O14, O44-45) documents a relative ribbon (first contact sets zero); the hardware's
  voltage-versus-distance law is unverified (HARDWARE_COVERAGE row 19). Alternatives considered: (a) bend
  proportional to distance with the whole strip as the range (the previous behaviour, closest to a resistive
  strip), (b) a fixed sensitivity clamped at the ends, (c) the chosen full range from any point, which trades
  a position-dependent sensitivity for full reach on a pointer strip far shorter than the hardware's. PURE
  CS-80 keeps (a). The engine's ribbon law (`ribbonFrequencyRatio`) is unchanged.
- **QWERTY play** (`QwertyKeyboard`, a `KeyListener` on the editor): `A W S E D F T G Y H U J K O L P ;` play C
  to E, Z/X shift octaves (A = C4 by default), C/V change velocity by 10 %. Notes go through the on-screen
  keyboard's `MidiKeyboardState` (channel 1) into the processor's existing keyboard event queue, so they light
  the keys. Keys are ignored while a text field has focus and when Ctrl, Cmd or Alt is held; a note is
  released when its key comes up, by a 10 Hz key-state check (focus loss), on disable and on destruction. On by
  default, saved in the editor state. It stays active in PURE CS-80 (an input device, like the on-screen keys).
- **Tooltips**: a ParamKnob tooltip now gives the long name and value, the owner's-manual sentence, the manual
  page and control number (or "Not on the original instrument"), a MIDI CC line when learned, and the key
  hints. Its **Circuit notes** section (heading tinted by evidence class, then the engineering notes) appears
  while Shift is held or when "Circuit notes in tooltips" is ticked in the menu (saved in the editor state).
  F1 or the menu opens the persistent help card with everything, including the coverage codes.

## Phase 6 MIDI learn

`cs80::MidiLearnMap` (`src/plugin/MidiLearn.h`, plain C++): 128 `std::atomic<int>` slots, controller -> host
parameter index, plus an atomic "learning" target and a generation counter. One controller drives one parameter
and one parameter follows one controller (learning again moves it). Bank select (0, 32), data entry (6, 38),
data increment/decrement and NRPN/RPN selection (96-101, used by MPE zone and bend-range negotiation) and the
channel-mode messages (120-127) are never learnable.

- **Audio thread**: `processBlock` calls `applyMidiLearn(midi)` before it reads the parameters. Each 3-byte CC
  in the block goes through `handleController` (learns it if armed, a compare-and-swap), and a mapped value is
  applied with `setValueNotifyingHost(value / 127)`, so it takes effect this block and the host records it.
  `handleMidi` skips learned controllers, so CC 1 learned to a fader stops being the mod wheel. Our code takes
  no lock and allocates nothing; `setValueNotifyingHost` is the same path JUCE's own wrappers use for host
  automation on the audio thread (JUCE's parameter listener lists use an uncontended `CriticalSection` there).
- **PURE CS-80**: neither learns nor applies; the map is kept and returns when PURE CS-80 is switched off.
- **State**: `getStateInformation` appends `<MIDI_LEARN version="1"><MAP cc="74" param="ch1_lpf_cutoff"/>...`
  (parameter IDs, not indexes) and `<UI_STATE version="1" tourSeen qwerty circuitNotes controllers/>`.
  `setStateInformation` reads and removes both before the parameter tree is normalised, so they never enter the
  APVTS tree; a state without them (older sessions) restores with no mappings and the default editor state.
  Presets never carry them (`writeUserPreset` saves the APVTS tree; `loadUserPreset` strips them defensively),
  and loading a preset leaves the session's map alone.
- **Editing**: the context menu (arm, cancel, forget), the KEYBOARD tab's MIDI learn module (one row per
  controller with its live value and FORGET, the learning status with CANCEL, CLEAR ALL) and the toolbar menu
  (clear all). An armed control shows a dashed amber outline until a controller arrives.

## Phase 6 Undo/redo

The APVTS is now constructed with the processor's `juce::UndoManager` (`apvts(*this, undoManager, ...)`; the
member is declared before the APVTS). The stock APVTS copies every parameter change into its ValueTree from a
private timer, through that UndoManager, so host automation and preset loads would land in whatever transaction
the last gesture opened and undo would replay them. `cs80::UndoableParameterState` (`UndoHistory.h`) is a
subclass that takes over that timer (it overrides the timer callback; the APVTS is not final) and classifies
each parameter change from a listener on every host parameter:

- **Recorded**: a change made on the message thread while a host gesture is open (every slider, button, combo
  box, the context menu and typed values bracket their edits) or inside a `ScopedUserEdit`. The first gesture
  begin opens a transaction named after the parameter; gestures begun in the same message-loop pass join it
  (the LEGACY render-policy cell, which clears two parameters, is one step); the final values are written into
  the tree through the UndoManager when the gesture ends (a long drag also flushes on the timer and coalesces).
- **Never recorded**: host automation (any thread), MIDI-learned moves (audio thread), preset loads, state
  restores and programmatic setters. Their values are written into the tree without the UndoManager.
- **History lifetime**: loading a program clears the history (on the message thread), as `replaceState`
  already does for state restores and user presets, so stale steps never apply to a different sound. The local
  A/B switch is one named step ("Compare A/B").
- **Threads**: the tree is written on the message thread; `treeLock()` serialises those writes with the host's
  `getStateInformation` / `setStateInformation` (the processor takes it around `copyState` / `replaceState` and
  calls `syncNow()` first when it is on the message thread).
- **UI**: toolbar UNDO/REDO keys (titles name the step), menu entries, Ctrl/Cmd+Z, Shift+Ctrl/Cmd+Z and Ctrl+Y
  (`StudioShortcuts`, a key listener that leaves text fields alone).

Programmatic edits from other workstreams that should be one undo step (ws6 randomise, ws7 store/copy memory,
copy rank I <-> II) wrap themselves in `Cs80AudioProcessor::ScopedUserEdit edit(processor.getUndoableState(),
"Copy rank I to II");`.

## Phase 6 Learning tools

- **Tour** (`TourOverlay`): nine steps (welcome, programs, faders, tone selector, ribbon and keys, ENGINE and
  PURE CS-80, compare and undo, the Studio view, menu and help). The editor is dimmed except a gold-outlined
  area; a card explains it in plain words. BACK / NEXT (DONE on the last) / CLOSE TOUR, Left/Right/Return/Esc.
  Dismissing it sets `tourSeen` in the plugin state and, only when the editor is a real window, writes
  `CS80Reference/ui-settings.xml` in the user's application-data folder (`TourMemory`), so new instances do not
  show it again. It opens by itself once, only in a real window (`StudioMount::shouldOpenTourByItself`):
  headless and snapshot editors never show it, so golden images stay clean. The menu runs it any time.
- **Per-control help**: `controlHelpText` / `showControlHelp` (help card, F1 or the menu): plain words, the
  owner's-manual page and control number (or service-manual sheets), the evidence basis with a one-line
  explanation, the HARDWARE_COVERAGE status codes spelled out, and the circuit notes.
- **Evidence view**: Studio > SETTINGS lists every subsystem of the model register (legacy model, circuit-engine
  model, offline reference, evidence basis and coverage codes); each tab carries model notes for its own
  subsystems; the menu opens SETTINGS directly. The painted register also carries an invisible static-text
  element with the same rows for screen readers.
- **One place to update**: `src/plugin/EvidenceRegister.h` holds the model register (the ten rows of the
  `docs/STATUS.md` model register, marked `Origin::status`, plus five rows from `docs/HARDWARE_COVERAGE.md`) and
  the per-control help (owner index, pages, coverage codes, basis, plain words, circuit notes) for every core
  parameter. `cs80_evidence_register_tests` keeps it complete.

### Evidence classes used

The help uses the classes of `docs/ANALOG_FIDELITY_REQUIREMENT.md` reduced to what each law rests on:
"Source-derived" (law read from a schematic, manual or adjustment page), "Source-derived route, inferred law"
(topology from the sources, quantity a declared inference), "Engineering inference", and "Modern extension (not
on the original)". Coverage codes are those of HARDWARE_COVERAGE.md (IB, A, M, MR, PC; EXT marks a software
extension). No entry claims a measurement. Classification choices that are mine and worth a review: FEET, PW,
PULSE and SAW as source-derived (owner-manual nominal values and switch functions); the footswitch truth table
as source-derived (FOOT_CONTROL_EVIDENCE.md); voice dispersion, drift and noise as engineering inference rather
than modern (they model real analog variation, but with arbitrary distributions).

## 5.7 UI tests

New sections in `src/tools/ui_snapshot.cpp` (one include, one delimited call block before the final summary;
the tests live in `tests/StudioUiTests.h`):

| Section | Covers |
| --- | --- |
| studio views | toolbar keys inside the toolbar, every Studio tab and sub-page constructing, painting and resizing at 1600 x 1000 and 1120 x 700; voice monitor and controllers strip; PURE CS-80 hiding and restoring the strip |
| studio interaction | relative fader drag (no jump, 20 px drag = 20 px), Shift fine drag, typed values (units, refusals, clamping), double-click reset gesture, context menu items and actions, copy/paste between controls and refusal across units, Ctrl/Cmd-click editor, PURE CS-80 menu, tooltips and help text, the ribbon law (and PURE), QWERTY play (event path, octave, release, shortcut chords, text focus, disable) |
| studio MIDI learn | arm from the menu, learn and apply on the audio thread, protocol CCs refused, tooltip, KEYBOARD tab list, versioned state round trip into a new instance, editor state round trip, presets keep the map, PURE CS-80 neither learns nor applies, a learned CC 11 leaves the foot controller, old states have no mappings |
| studio undo and redo | typed value and whole drag as one step each, automation from the message thread and another thread, MIDI-learned moves, undo leaving automated parameters alone, redo, toolbar keys, shortcuts, A/B as one step, program load and state restore clearing history, a two-parameter action as one step |
| studio learning tools | tour auto-open rule, no auto-open headless, all steps at both sizes, keys, dismissal remembered in the session and restored, no settings-file write headless, help for every main-panel control, F1 help card, evidence route and its screen-reader text |
| studio tab order, hit targets and contrast | see the accessibility review |

Core tests: `cs80_evidence_register_tests`, `cs80_midi_learn_tests` (single-threaded semantics plus a two-thread
run with pre-registered iteration counts).

Snapshots written for review (`studio-*.png`): main view at both sizes, each tab and sub-page at both sizes,
the drawers at both sizes, the KEYBOARD tab with a learned controller, every tour step at 1120 x 700 (first
three at 1600 x 1000), the help card, and the structured tooltip.

### Mutation checks

Each new gate was run against a deliberately broken build, then restored:

| Mutation | Failing checks |
| --- | --- |
| Help entry renamed, cited document renamed, `pan` not EXT | evidence register test: 4 failures |
| `assign` without releasing the old controller; CC 0 learnable; learning without the compare-and-swap | MIDI learn core test: 9 failures |
| Snap to mouse position on; no Shift scaling; old ribbon law; QWERTY ignoring text focus | 9 UI failures (relative drag, no jump, 20 px, fine drag, four ribbon checks, text focus) |
| `handleMidi` not skipping learned CCs; MIDI learn live in PURE CS-80; MIDI_LEARN not saved | 7 UI failures |
| Non-user changes written through the UndoManager; no grouping; program load keeping history | 4 UI failures (including "undo leaves automated parameters alone", which is the pollution itself) |
| A/B keys 32 design px; library key ahead of the program display in Tab order; old lit-key face | 3 UI failures (target size, tab order, contrast 3.25:1) |

## Accessibility review

Method: keyboard traversal with JUCE's own `KeyboardFocusTraverser` on the real editor containers, a WCAG 2.2
2.5.8 target-size scan at the 1120 x 700 minimum (24 x 24 px, or the spacing exception: a 24 px circle on the
target's centre touches no other target and no other undersized target's circle), WCAG 1.4.3 contrast (4.5:1)
for the layer's text colours, accessible names on every focus stop, and visual review of the captures.

Findings and fixes:

| Finding | Fix | Result |
| --- | --- | --- |
| Toolbar A, B, UNDO, REDO 32 design px wide = 22 px at 1120 x 700 | every toolbar key at least 35 design px | 24-25 px; 136 modern-layer targets checked, 0 undersized |
| Lit toolbar key legend (dark on amber) 3.25:1 | brighter lit face | 6.02:1 |
| Masked ENGINE legend (PURE CS-80 on) 3.72:1, still an active control | stronger legend | 5.37:1 |
| Running notes on the KEYBOARD tab in the quiet axis ink | panel ink | meets 4.5:1 |
| Evidence register and model notes painted only, unreadable by screen readers | static-text element per card with the same rows; art layer marked not accessible | tested |
| Tab order | toolbar first, left to right; Studio: tabs, sub-page keys, CLOSE, then the open page only; drawer and tour as their own containers | tested |
| Accessible names | every focus stop on the panel and on every Studio page has a name; tour steps and the value editor carry descriptions; MIDI learn rows have row names and named FORGET keys | tested |
| Focus visibility | every new key draws the panel's gold focus ring | visual review |
| Hit targets of the whole editor after the ws7 merge | strict gate at 1120 x 700 with two documented exceptions (below) | 0 other undersized targets; 26 excepted, all at least 24 px at the 100 % display size |

The disabled undo/redo keys fall below 4.5:1 by design (inactive controls are exempt from 1.4.3). The piano keys
are exempt from 2.5.8 as essential.

**Documented target-size exceptions (original-panel conformance layout, ws7).** At 1120 x 700:
- The 23 front-strip levers are 27.5 design px wide on a 29.25 design px pitch, which is 19.3 px wide and
  20.5 px apart. Neither the 24 px size nor the spacing exception is geometrically possible without
  abandoning the strip that reproduces the original lever row (WCAG 2.5.8 "essential"). A larger mouse area
  cannot help, because the neighbours are closer than 24 px.
- The three stacked tremolo/chorus keys are 25 design px tall (18 px) in the 104-unit deck block.

Both groups are keyboard-operable, and the levers take typed values. At the 100 % display size (1600 x 1000,
offered by the toolbar menu and the brand mark) every one of them is at least 24 px, and the test asserts it.
The exception list is fixed: any other undersized target, or growth of the list, fails the test.

## Measured results (tool and settings)

All UI figures come from `CS80Snapshots --self-test` under `scripts/run-ui-smoke-linux.sh` (Xvfb, 1920 x 1200
x 24, DejaVu fonts), editor sizes 1600 x 1000 and 1120 x 700.

| Quantity | Before | After |
| --- | --- | --- |
| Fader click away from the cap | value jumps to the pointer (JUCE default) | no change |
| 20 px fader drag | n/a (absolute) | 20 px of cap travel (within 1 %) |
| Shift drag | not available | 0.1 x (within 0.004 of track) |
| Modern-layer targets under 24 px at 1120 x 700 | (no toolbar) | 0 of 136 |
| Lit toolbar key contrast | 3.25:1 (first draft) | 6.02:1 |
| Host automation entering the undo history | every flushed change (stock APVTS with an UndoManager) | none |

## Intentional circuit-engine sound changes

None.

## Entries for HARDWARE_COVERAGE.md (2026-10-31)

- **2026-10-31, row 19 Ribbon (UI)**: the on-screen ribbon gives the full bend from any first-contact point to
  either end of the touch strip (declared UI inference; alternatives in ws8-studio.md). PURE CS-80 keeps the
  bend proportional to distance over the whole strip. The engine law is unchanged; status stays IB/A/PC.
- **2026-10-31, 61-key playing surface**: computer-keyboard note input (A-; with octave and velocity keys)
  through the UI keyboard event path; text fields and shortcut chords excluded. Physical-controller status
  unchanged.
- **2026-10-31, Panel geometry, colors, lever directions and labels**: a toolbar replaces the program cluster;
  the Advanced panel becomes the five-tab Studio view; the signal-flow plate's content moves to an optional
  voice-monitor drawer; the pitch and mod wheels move to a controllers strip hidden in PURE CS-80.
- **2026-10-31, modern layer (new rows suggested)**: MIDI learn (per-session map in plugin state, inert in PURE
  CS-80), undo/redo (one step per gesture; automation, MIDI learn and loads never recorded), A/B (local fallback
  pending the library's), typed values and context menus, guided tour, per-control help and the in-app
  evidence register (`src/plugin/EvidenceRegister.h`). All EXT, none part of the original instrument.

## New parameters

None. MIDI learn and the editor state are plugin-state children (`MIDI_LEARN` version 1, `UI_STATE` version 1).

## Open items

- Context menu, MIDI learn and typed values exist on every `ParamKnob`; switches (`LedButton`, `AdvSwitch`),
  combo boxes, the tone selector and the memory lid's miniature sliders (a plain `juce::Slider` in the editor)
  do not have them yet.
- Undo-applied changes reach the host without begin/end gestures (JUCE's APVTS writes them through
  `setValueNotifyingHost`). Arrow-key edits are one step per key press.
- When a host calls `getStateInformation` off the message thread, the pre-save sync is skipped; a non-user
  change made in the preceding ~100 ms could then be flushed by `copyState` into the current transaction. When
  the timer writes a non-user value while the audio thread changes the same parameter, the APVTS may re-apply
  the value read a moment earlier for one block. Both are bounded and benign; noted for review.
- The local A/B compares the 26 programmer controls, globals and selectors, not memory-bank contents; ws6's
  compare replaces it through the toolbar actions.
- The live displays on the Studio pages (scopes, curves, tuning readouts) are painted and have no accessible
  text; the evidence views now do.
- The two documented target-size exceptions above (front-strip levers, tremolo/chorus keys) meet 24 px only
  from the 100 % display size up.
- `src/plugin/VoicePlate.h` (ws7) is not compiled anywhere and references `geo::plate`, which `PanelLayout.h`
  no longer defines. The voice monitor drawer (`ScopeDrawer`) shows the same content (path, envelopes, filter
  response) plus the voice cards, so VoicePlate.h can be deleted, or fixed and hosted in the drawer if the
  integrator prefers its exact drawing.
- The voice monitor's curves are illustrations of the panel settings, not taps of the engine.
- QWERTY play needs the editor to have keyboard focus, which some hosts withhold.
- The shared Xvfb display allocation (`xvfb-run -a`) occasionally collides when several engineers run the UI
  smoke at once (exit 70, Openbox cannot open the display); a rerun passes. No test change.

## Integration notes

- **Files outside the ownership list**: `src/plugin/PluginEditor.cpp` (delimited mount points only),
  `CMakeLists.txt` (below the ws8 markers), `src/tools/ui_snapshot.cpp` (one include and one delimited call
  block before the summary), and in `src/plugin/PluginProcessor.h/.cpp` beyond the MIDI-learn and undo members:
  the `apvts` member type and construction, `treeLock`/`syncNow` around `copyState`/`replaceState` in the state
  and preset paths, history clearing in `loadFactoryPreset`, the `MIDI_LEARN`/`UI_STATE` hooks in
  `getStateInformation`/`setStateInformation`/`loadUserPreset`, one line in `handleMidi` and one in
  `processBlock`. New files: `StudioView.h`, `StudioToolbar.*`, `StudioDrawers.*`, `StudioMount.h`,
  `StudioTour.*`, `UndoHistory.*`, `MidiLearn.h`, `EvidenceRegister.h`, `tests/StudioUiTests.h`,
  `tests/MidiLearnTests.cpp`, `tests/EvidenceRegisterTests.cpp`.
- **ws7 (merged, `bd524f9`)**: the mount block follows ws7's split (tone keys via `CentreRow::getToneButton`,
  the ribbon via `PerformanceDeck::getRibbon`, the channel I frame via `geo::rowFrame`). It still hides the
  editor's program cluster (`prevButton` to `advancedButton` and the four preset labels) and moves the
  pitch/mod wheels out of `optionComponents`.
  - AdvancedPanel.cpp now uses ArtCommon.h's wrinkle tile, `fillRelief` and `displayFont` (the embedded Barlow
    Condensed); its duplicate helpers are deleted. The toolbar, drawers and tour use the same typeface.
  - The toolbar menu has a Display size submenu that calls `setZoomPercent` / `getZoomPercent`.
  - The golden images `tests/golden/main-1600x1000.png` and `main-1120x700.png` were refreshed with
    `CS80_REFRESH_GOLDENS=1`. ws7's goldens differed in 0.91 % and 0.95 % of pixels, all in the toolbar strip
    where `StudioToolbar` covers the program cluster. The panel itself is unchanged. I inspected both images
    before committing.
  - The memory lid's STORE PANEL, COPY MEMORY and COPY RANK write each parameter in its own gesture within one
    click. The undo grouping makes each action one undo step; the undo test checks STORE PANEL.
- **ws6 (library)**: connect `Actions::stepPreset/stepArmed/openLibrary/libraryOpen` to the library's prev/next
  through the current filter, and `selectCompare/activeCompare/copyCompare` to the real A/B; then delete
  `CompareSlots`. Randomise should use `ScopedUserEdit`.
- **ws5 / ws3**: mount pages with `StudioView::setPage/addPage` (see above); `removeBuiltInPage` when a page
  replaces the built-in controls.
- **ws2**: nothing to wire; the toolbar binds `source_performance` and `parity_mode` by ID.
- **New code that saves or restores the APVTS tree** must take `getUndoableState().treeLock()` and call
  `syncNow()` on the message thread first, and must not put `MIDI_LEARN`/`UI_STATE` into the parameter tree.

## Verification

See the final report for the exact commands and outputs: full core suite, `cs80_ui_smoke` and the four locale
tests, `scripts/check-compat-gates.sh`. After the merge with the integration branch (`bd524f9`, which carries ws4's
intentional circuit-engine changes), any `v2` difference in the compat gates comes from ws4, not from this
workstream, which changes no audio.
