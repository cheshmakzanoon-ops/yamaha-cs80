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
| ws1-voice | 1.1b and 1.2 done with recorded misses; 1.4 and 2.5 finished by the integrator | `ea391d7` |
| ws2-engine | all steps done except sample-accurate automation | `8da6c2c` |
| ws3-keyboard | all steps done except the vendored MTS-ESP client | `342b9be` |
| ws4-circuits | all four steps done; EXP volume optics handed off | `195e5e3` |
| ws5-modern | all five steps done | `5263172` |
| ws6-library | all steps done; memory-panel model not wired into audio | `0cb1e3a` |
| ws7-panel | all six steps done | `bd524f9` |
| ws8-studio | all six steps done | `5af3b77` |

## Integration (single project manager, 2026-10-31)

After the eight merges the integrator closed the hand-offs below; commits and
measurements are in [STATUS.md](../STATUS.md).

| Hand-off | Result | Commit |
| --- | --- | --- |
| ws3 idle path on ws1's models | ported: closed-form idle WSC (`advanceIdle`), nodal-stage re-referencing with unwarped couplings at the idle step, midpoint-centred step input, snapshot replay; truth model v2; gate tolerances unchanged (one implementation revision: 1.5 ms minimum replay) | `342b9be` |
| First chord after a load | cold oscillators start warm (declared power-on convention); the dense PANEL chord peaks at 0.980 | `3961c1c` |
| Windows stack | compact idle snapshots (Engine 417 KB -> 273 KB), 8 MiB stack for MSVC test executables | `81de004` |
| ws5 pages, ws3 tuning page | mounted in the Studio view (MODULATION, KEYBOARD, FX); new TUNING + VOICES page | `230f367` |
| Toolbar | A/B -> ws6 slots, prev/next -> library filter, INIT -> `loadInitPatch`, ENGINE/PURE -> processor API, randomise menu item; `CompareSlots` deleted | `230f367` |
| Undo scope | A/B, randomise and memory-lid commands are one undo step each | `230f367` |
| `VoicePlate.h` | deleted | `230f367` |
| Standard oversampling | 2x from the measured alias (-88 dB at 2x) | `ebbe75b` |
| Latency | 63 + the oscillator's 8 internal samples in the circuit engine | `8da6c2c` |
| KBC contour while levers glide | refreshed whenever a smoothed lever value changes | `8da6c2c` |
| FX-mix smoothing | already on `global.fxMix` (ws2) before `+ modernGlobal.fxMix` | — |
| EXP volume optics | done by ws2 | `3fcff91` |
| 1.4 VCA doc and mutations | [IG00151_VCA_MODEL.md](../IG00151_VCA_MODEL.md); 3 mutations caught | `0dfe4ee` |
| Retrigger click | Funky 3+4 outlier gone (quick audit worst 0.99); the full audit finds 1.51 on the legacy-bank "Bass Stack" rendered on the circuit engine (open) | audit at `00ceccf` |
| 2.5 tolerances and drift | [TOLERANCE_DRIFT_MODEL.md](../TOLERANCE_DRIFT_MODEL.md) | `22b6854`, `a0f6dd5` |
| CPU pass | bounded-error tan; target not met (STATUS.md) | `00ceccf` |
| Library levels | recalibrated after the last gain change | `82b9e14` |
| Goldens | re-recorded on the circuit-engine default | `41363ea` |
| Validator exit 139 | code review and teardown probe; not resolved | `80cbe8c` |
| V2 re-pin | once, to `00ceccf` | `ef9bc35` |
| Parameter count | `test_parity_capture.py` pins 139 + 1 + 2 + 9 + 39 | `a0f6dd5` |
| Factory recall and matrix routing | recall zeroes amounts, keeps routing (session state) | STATUS.md |
| Sanitizer CI | GCC 13 UBSan constant-evaluation fix; test timeouts x4 under the sanitizers | `fd0e851`, `a2b2452` |
| macOS UI smoke | first-run tour covered the real-pointer fixture's keybed; fixture and screenshot capture mark it seen; the F1 help-card check raced JUCE's background dismissal of call-outs | `b2c2302`, `a721935`, `e048dca`, `a26f583`, `df4ba22` |
| Merge to `main` | [PR #2](https://github.com/cheshmakzanoon-ops/yamaha-cs80/pull/2), merge commit, after green build and DSP runs at `e048dca` | `ec72eed` |
| Post-merge fixes | [PR #3](https://github.com/cheshmakzanoon-ops/yamaha-cs80/pull/3): F1 help-card race, tour in a screenshot, unsigned macOS package; build, release and DSP green on `main` | `55bb043` |

## ws2-engine — engine I/O and modes

**What it did** (branch tip `3f53f10`, report [ws2-engine.md](ws2-engine.md)):
- 5 ms sample-rate-independent control smoothing in the circuit engine
  (`Smoothing.h`); every continuous control and controller.
- OVERSAMPLING Eco/Standard/HQ with a Kaiser decimator per factor, 63-sample
  latency in every mode, offline HQ through `isNonRealtime()`; Legacy stays 2x.
- Engine default flip at the plugin layer, state schema 3; ENGINE and PURE CS-80
  concepts with `setEngineMode`/`setPure`/`loadInitPatch`; fixed the schema marker
  being read as a string.
- LISTENING OUTPUT (L/R, GENERAL mono, PHONES model), declared noise PSDs with a
  shared SUB29 generator, EXP optics, host parameter groups (flat order unchanged).

**How it reasoned.** Standard was one constant to change after ws1's oscillator;
tests that tick at 2x pin Eco; goldens pinned Legacy until the integrator
re-recorded them.

**Open.** Sample-accurate automation; per-voice device noise; the I2/I3 networks'
own law; the SUB NOISE function could read the shared generator.

## ws6-library — tones, memories and library

**What it did** (branch tip `6ad4dc9`, report [ws6-library.md](ws6-library.md)):
301 curated circuit-engine programs at host indexes 222+, the 222-program Legacy
Library at 0-221 (FNV digest), Program Change with bank select, library audition,
filter-aware prev/next, LEGACY scope and technique tags, A/B with session
persistence, randomise within original ranges, tone-tab review records and an
opt-in refined BA/diode boundary, and a memory-panel electrical model.

**Open.** Memory-panel loading and the refined boundary are not wired into audio;
the 25 hypothesis cells need native crops.

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

## ws5-modern — modern modulation and FX layer

**What it did** (branch tip `0df57b1`, report [ws5-modern.md](ws5-modern.md)):
- **MSEGs.** Three shapes of up to 32 points with a curve per segment. Loop modes
  off, sustain and always; trigger modes poly, mono and free. Tempo sync follows the
  host BPM, with a 120 BPM fallback.
- **LFO waveforms.** Triangle, saw up and down, square, S&H and smooth random are
  added. Sine stays bit-identical to the old law.
- **Modulation matrix.** 16 slots with bipolar amounts, an optional via source and
  a rank target. It has 24 sources (ribbon, poly/channel/shared pressure, keyboard
  contour, SUB, ring envelope, both EGs and more) and 17 destinations. Keyboard
  curves shape matrix sources only; the original touch circuits are untouched.
- **FX.** Each slot can run a BBD ensemble, a Dattorro plate, a dispersive spring,
  a tape echo, or a drive with 1×–8× oversampling. The drive at 4× measures
  −87.6 dB alias. An optional BS.1770 loudness match is off by default. The classic
  effects stay bit-identical.
- **State.** A versioned `MODERN_LAYER` child in the plugin state, and 39 host
  parameters with AU hint 12.
- **UI pages.** `cs80::ui::modern_layer`: `ModernModulationPage`,
  `KeyboardModulationPage` and `ModernFxPage`.

**How it reasoned.** Everything defaults to an exact no-op: the `+ mod.*` terms sit
at the end of expressions, and the PURE CS-80 render is unchanged when the layer is
fully configured. It moved to a double-precision LFO phase because the old float
phase measurably drifted at low rates. It made the MSEG rate a linear ±3-octave
parameter so its default survives a state round trip exactly.

**Open:**
- Mount the three pages in the Studio view.
- Decide whether factory recall should also clear matrix routing; today it zeroes
  only the amounts.
- FX-mix smoothing belongs on `global.fxMix` (ws2).
- The worst case, with all 16 slots live and 3 poly MSEGs, costs +0.14 RTF.

## ws1-voice — voice circuit

**What it did** (branch tip `3339353`, report and handover in [ws1-voice.md](ws1-voice.md)):
- **1.1b IG00156 nodal stage.** In `src/core/Ig00156Filter.h`, with its truth model
  in `src/reference/IG00156Network.h`. It models the input networks, the
  1.2 MΩ/180 pF branch, both 1 µF couplings, in-loop OTA saturation and a Newton
  solve. Equivalence with the truth model is 1.9e-7, Q anchors are within 0.6 % and
  cutoff anchors within +0.25 to +0.53 %, and it is stable at 88.2–384 kHz.
  - Miss: the full-grid small-signal gate fails at 88.2/96/192 kHz for 4 and
    12 kHz cutoffs, because of bilinear warp in the top octave.
  - Miss: the ADJ p11 HP setup point reads 1.69 Vpp against the documented 4.3 ± 1.
- **1.2 Oscillator promotion.** The worst non-harmonic at 48 kHz improved from
  −51.1 to −88.3 dB (saw) and from −53.6 to −88.8 dB (pulse). The −70 dB target is
  met at 2×. Pin anchors hold. The residual table is now shared: 5,984 bytes per
  oscillator instead of about 22 KB.
  - CPU rose from 0.296 to 0.518 RTF. This needs the structure-of-arrays pass.
- **1.4 IG00151 VCA (partial).** Separate linear and exponential inputs, the
  10 nF level node, offset plus CV feedthrough with AC coupling, and constant-power
  pan. The worst steal click improved from 1.33 to 0.93. One retrigger case still
  scores 1.26 (Funky 3+4, 44.1 kHz): two same-pitch voices whose edges align, which
  is a voice-allocation effect, not an onset click.
- **2.5 Tolerances and drift.** Not started; the design and the ordered next steps
  are in its handover.

**How it reasoned.**
- These are declared inferences:
  - HP source 75 kΩ, and LP 150 kΩ in series. The evidence ledger reads the 150 kΩ
    as a shunt, so this is flagged.
  - An affine Q-block law.
  - A cutoff calibration that undoes the frequency depression of the 180 pF branch.
- ADAA was skipped because the filter's own aliasing is already at the −88 dB
  measurement floor.
- It aborted the integration merge at the wind-down, so its branch is based on
  `6719ce4`.

**Open:**
- Merge it.
- Both oracle test files need `core/RingModulatorCircuit.h` and
  `CircuitSineSource.h`.
- Re-run `cs80_ring_modulator_circuit_tests` on the merged tree.
- Finish the 1.4 model doc and its mutation checks.
- Do 2.5.
- The oscillator adds 8 internal samples (83 µs) of latency that is not yet
  reported (ws2).

## ws3-keyboard — voices, keyboard, touch and tuning

**What it did** (branch tip `e5287ea`, report [ws3-keyboard.md](ws3-keyboard.md)):
- **2.1 Free-running voices.** All 16 chains run continuously in the circuit
  engine. The idle path advances phase exactly, steps the filters at 3 kHz, and
  replays from a snapshot when the voice wakes. Worst wake residual is −51.9 dB;
  idle costs +0.014 RTF.
  - The plan's whole-engine idle target of ≤ 2 % is not met: baseline idle is
    already about 0.08 RTF, from other always-on circuits.
- **2.2 KAS and sample/hold.**
  - NAND(TR, SUS) hold: Sustain I holds and droops, Sustain II keeps tracking.
  - The droop uses a declared 0.033 µF and 1 pA; the 10 pA case is kept in the
    sensitivity table.
  - A still-held ninth key takes the next free voice with zero touch.
  - Opt-in `kas_scan_timing` sets 1 ms scan instants.
- **2.3 Keyboard contour.** A declared 61-key map drives `KeyboardContour.h` and
  meets both anchors exactly. The worst error over 61 keys × 25 lever pairs is
  8.4e-7 V. The C2/KAS6 conflict is reported openly: read literally, it gives
  −4.018 V.
- **2.4 Touch.** TWS a (100 ms), b (held initial touch) and C (50 ms AC-coupled
  transient), and the TRG5 diode-OR pressure bus. Velocity and pressure curves are
  labelled interface-layer and are the identity at their defaults.
- **Ribbon CC.** Off by default; CC 16 when enabled.
- **Phase 6 tuning and MIDI.**
  - Scala .scl/.kbm and allocation-free MTS SysEx.
  - MPE zone negotiation with an upper zone, and CC88 high-resolution velocity.
  - Voice modes: 8 (the default), 16 (a second engine), and unison ×2/×4/×8.
  - MTS-ESP is only a shim: the ODDSound client could not be fetched.
- 9 parameters with AU hint 10. All 30 mutations were caught.

**How it reasoned.**
- Its first free-running evaluation failed. It recorded an erratum before
  re-running, without changing the tolerances.
- It composed its TRG5 bus in series with ws4's SUB converter, so pressure is not
  double-counted: full pressure still lands on 200 Hz.
- It renamed the provisional contour weights `kLegacyKeyboard*` instead of
  deleting them, to keep Legacy byte-identical.

**Open:**
- Merge it.
- Build a Keyboard/Tuning Studio page from `loadScalaTuning`, `clearScalaTuning`,
  `getScalaDescription`, `isMtsEspAvailable`, `isMtsEspConnected` and the 9
  parameters.
- `test_parity_capture.py` needs every workstream's parameter count
  (139 + 9 + 39 + …).
- The 16-voice meter and pressure ownership cover only the first engine.
- Vendor the MTS-ESP client.
