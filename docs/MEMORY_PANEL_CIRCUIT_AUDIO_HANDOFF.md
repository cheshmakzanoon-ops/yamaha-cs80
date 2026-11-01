# Memory/PANEL circuit-audio integration handoff — 2026-11-01

**State:** truth-network and control-frame pot mapping are qualified; the
runtime audio path is **not yet connected**. Hold `Engine.cpp` edits until the
ongoing CPU and VCA/retrigger changes have been committed and retested.

## Existing authority and physical evidence

- `src/core/MemoryPanels.h` models the M1/M3/PANEL selected rail on rank I and
  M2/M4/PANEL on rank II, 24 analogue wipers and isolation diodes, follower
  buffers on HP/LP cutoff rows 7/9, and two logical waveform switch rows.
  The offline double-precision Newton reference is
  `src/reference/MemoryPanelNetwork.h`.
- The selected-rail, wiper and diode *topology* is traced to Yamaha CS-80
  service BA27/BA28, MBK63 (source image titled MKB) and S31–S38 (see
  `docs/PRESET_BA_CUTOFF_BRANCHES.md`, `docs/PRESET_AL_RECEIVER_GRAPH.md`,
  `docs/RANK_II_RECEIVER_PORTS.md`, `docs/workstreams/ws6-library.md`).
  The 50 kΩ linear pot with 1 Ω end resistance, 0 V reference, ideal follower
  output and nominal 2SC458/1S1555 surrogates are **declared inferences**;
  the documented alternatives are 10/100 kΩ pots and forward-active drivers.
  This is **not a hardware measurement**.
- `Cs80AudioProcessor::readParameters()` already selects the exact existing
  bank and copies its physical host parameter values into `cached` when
  `hardware_source_N` is 11/12. Selector 13 keeps its rank's PANEL values.
  `ControlFrame` already carries `{effectiveValues, hardwareSources}` to both
  primary and 16-voice extension engines. A new host parameter, state tree,
  or second memory authority is unnecessary.
- The four memory banks are existing APVTS parameters under
  `memory_{1..4}_<channel-id>`. Bank pairing is M1/M3 for rank I and M2/M4
  for rank II; sources 0–10 remain the existing T51–T54 factory columns.
  `CP_feet`, `CP_pan` and the two synthetic filter-bypass toggles are *outside*
  the 26 original matrix outputs and stay current rank controls.

## Source input contract (preregistered before runtime integration)

`src/core/MemoryPanelControlBridge.h` provides a 26-entry immutable mapping:

| Original output | Plugin controls | Interpretation |
| --- | --- | --- |
| 1–6 | PWM speed/depth, PW, pulse/saw switches, noise | selected source's six VCO controls |
| 7–10 | HP cutoff/Q, LP cutoff/Q | cutoff wipers are buffered, Q unbuffered |
| 11–16 | filter IL/AL/A/D/R, VCF level | filter envelope and filter-path controls |
| 17–22 | sine, VCA A/D/S/R, level | source path and VCA controls |
| 23–26 | initial brilliance/level, after brilliance/level | touch buses |

Each pot uses the host's physical `ParamDef` range and the **inverse of its
existing JUCE NormalisableRange skew**, then clamps into `[0,1]` for the
electrical wiper. Its direction is a declared software-to-hardware wiper
convention, not a recovered knob taper/PCB endpoint. Pulse/saw are exact 0/1.
This adapter has no Engine include and cannot change audio by itself.

The preregistered acceptance file is
`tests/parity/tolerances-v2/memory-panel-audio-input-v1.json` (commit
`3162c1b`, committed **before** evaluating the bridge). The registered
`cs80_memory_panel_audio_input_tests` fixture uses both ranks × selectors
11/12/13 × uniform, asymmetric and edge-position pot sets. It uses the
*independently generated* physical positions to solve the full 94-node
reference, rather than feeding the compiled positions back into the oracle.

| Gate | Pre-registered limit | Initial test result |
| --- | --- | --- |
| Physical float value → wiper fraction | ≤ `2e-5` | `1.28292e-7` |
| Every selected source's 24 analogue buses vs offline truth | ≤ `2e-3 V` | `0.000183967 V` |
| Current from inactive sources | ≤ `1e-8 A` | `2.52e-9 A` |
| Truth-model Newton residual | ≤ `1e-12 A` | `1.11e-16 A` |
| Moving unselected memory/PANEL bank | ≤ `1e-6 V` at selected buses | passed |
| Selected waveform gates | exact booleans | passed |
| Deliberate mutation: exchange HP/LP wipers | must breach 2 mV gate | caught |

These numbers are model-to-model consistency, not analogue tolerances. The
existing `memory_panel_v1` limits are unchanged.

## Why existing sound still bypasses the circuit

`source_factory::route(rank, selector, sourceModels)` returns
`Route::panel_or_memory` for selectors 11–13, even when the circuit/PURE policy
is active. `Engine::setControlFrame()` marks only
`source_complete`/`source_inferred` factory routes as `sourceActive`. The
source-inactive path in `Engine::buildSettings()` then uses digital panel
settings and `source_factory::bridgeBoundary()` rather than selected memory
rail voltage and receiver loading. Nothing calls `memory_panels::compileBuses()`
for audio today.

## Integration sequence once Engine.cpp is free

1. **Route as original circuitry, never Legacy.** Add an explicit compiled
   memory/PANEL route to the circuit engine for 11, 12 and 13 while keeping
   `RenderPolicy::legacy`, `Engine::setParams()` and every factory selector
   unchanged. PURE CS-80 *includes* original memory circuitry; the modern
   feature-neutrality rule is not a reason to disable it there.
2. **Prepare once per changed control frame.** Convert the selected effective
   rank settings using `control_bridge::positions()`, run
   `memory_panels::compileBuses()` at block/control boundaries, then cache the
   resulting 26 buses and switch states by selector/positions. Do not execute
   a 4-pass/24-row diode solve every audio sample or every unchanged block.
   Invalid/nonfinite frames must leave the prior compiled controls intact.
3. **Attach the same source adapter laws as the factory path.** Outputs 1–26
   feed WSC switches/PW/PWM, HP/LP cutoff and Q ports, both envelopes, LI gains,
   LC level, and touch voltage receivers. Do not treat the 24 bus voltages as
   ordinary `CP_*` knob values. Reuse named conversion helpers from
   `SourceFactoryTones.h` in a *new* memory adapter without reordering the
   production `compileTone()` arithmetic (the factory V1 numerics are frozen).
4. **Retain source impedance, not just DC voltage.** The compiled memory bus
   result is loaded DC voltage, whereas `source_factory::ToneControls::sources`
   expects open-circuit Thevenin equivalents. For cutoff/Q/LC, derive an
   effective source impedance from the selected pot Thevenin resistance and
   the linearised isolation diode (the existing `solveBranch()` returns its
   dynamic resistance), including the 10 kΩ assembled shunt and selected-rail
   sag. Add a *separately preregistered* small-signal/current-injection truth
   gate before accepting an approximation to cross-row coupling. A temporary
   0 Ω stiff bridge would match zero-drive buses but erase dynamic touch and
   filter-EG coupling; do not present that as a completed circuit model.
5. **Publish complete, continuous rank state.** Make source-active readouts and
   `getSourceToneInfo()` distinguish memory vs PANEL from factory source
   provenance. On selector changes, change control sources without forcibly
   resetting oscillator/filter/EG capacitor states; preserve the existing
   zero-frame continuity behavior. The extension engine receives the same
   frame in 16-voice mode, so use the same compiled source law there.
6. **Add engine-level integration regressions before audio refactor.** Compare
   every memory/PANEL selected bus at zero envelope/touch with the offline
   reference under the already committed 2 mV gate; verify the HP/LP and LC
   per-voice readouts respond in the correct direction; prove changing an
   unselected bank leaves output bytes identical; assert exact pulse/saw and
   rank-bank ownership; test zero-frame switches for both ranks. Mutation-test
   an intentionally misrouted memory bank and an omitted source impedance.
7. **Land with audio compatibility discipline.** Run the legacy/default/tails
   and factory V1 byte gates; expect a deliberate circuit-engine sound change
   even at default PANEL (selector 13), measure CPU/phrase/alias impact, then
   push the audio SHA and make the **separate** full-SHA `SOURCE_AUDIO_V2`
   re-pin commit. Run `check-compat-gates.sh --strict-v2` and update STATUS,
   HARDWARE_COVERAGE and the V2 change ledger.

No user-facing claim of improved hardware authenticity is warranted until
the source-impedance and revision uncertainties are resolved. The first
deliverable is therefore a **source-driven audio path qualified against its
declared truth model**, not a measured Yamaha CS-80 calibration.
