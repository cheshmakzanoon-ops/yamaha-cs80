# CS-80 hardware coverage and acceptance ledger

> **Development policy updated 2026-10-29:** [The current requirement](ANALOG_FIDELITY_REQUIREMENT.md) and [roadmap](PARITY_ROADMAP.md) permit implementation and reviewed integration using public evidence plus explicitly labeled engineering inference; unavailable hardware is not a development prerequisite. In this dated evidence record, measurement-required/open gates limit claims of measured fidelity, not continued modeling. Preserve source conflicts and uncertainty; offline-only statements describe the implementation at the recorded checkpoint, not a permanent integration prohibition.

Audit date: 2026-10-28. Code baseline: `56127be8ddfc04a09fc2a2ee18f342e1fe56330e`. Later work must update the affected rows with its exact tested revision. This document inventories the original owner's numbered control groups, connections, physical interface and circuit chain. It is not a claim that the inventory has been independently reconciled against every service-manual component.

**Required outcome:** the closest 1:1 recreation of every physical feature and analog circuit behavior achievable from public knowledge plus explicitly labeled engineering inference, as specified in [ANALOG_FIDELITY_REQUIREMENT.md](ANALOG_FIDELITY_REQUIREMENT.md). The current instrument does not meet that outcome. A control existing on screen does not establish electrical or sonic equivalence.

## Verified increment and source correction

**2026-10-31, circuit engine filters (rows 26–29):** Source Performance/Isolation
now run each HP/LP stage as the IG00156 model in
[IG00156_REALTIME_MODEL.md](IG00156_REALTIME_MODEL.md): in-loop OTA saturation
(inferred level), the source-derived 4.7 MΩ feedforward contribution (22.6 Hz added to
each integrator), no input clip. Q law, gm law and the 1.2 MΩ/180 pF, Q-block and
1 µF interstage elements remain open; status stays A.

**2026-10-31, circuit engine envelopes (rows 32–34, 37–40):** the Source
Performance/Isolation envelopes now use the inferred single-RC law in
[ENVELOPE_RC_MODEL.md](ENVELOPE_RC_MODEL.md): time-to-−60 dB decay/release, an
overshoot-target attack that peaks at exactly T, no ×5 time-window bends, and filter
envelope continuity on voice reuse. Status stays A (approximate): it is a declared
engineering model, not a recovered IG00152/IG00159 law. Legacy rows are unchanged.

At `71f0483d32657fc48bac1a0693e9c725e71e74dd`, the AT VCF route and zero-touch velocity invariance pass the full Linux/macOS/Windows build/host matrix, numerical/sanitizer tests and UI screenshots. These are software checks, not calibrated analog equivalence. TWS17 plus TRG19/23 now show that initial brilliance/level use a held b signal while pitchbend uses a separate AC-coupled C signal. The earlier inference that all initial-touch destinations should decay together was incorrect and is withdrawn. Affected rows below are updated; other baseline gaps remain open.

## 2026-10-31 integration of the eight workstreams

The entries below were written by the eight parallel workstreams
([docs/workstreams/](workstreams/README.md)) and folded in by the integrator on
the integration branch (`claude/adoring-fermi-alhg4x`, merged SHAs in
[STATUS.md](STATUS.md)). They update the rows named; rows not named keep their
text above. Every law is source-derived or a labelled declared inference; nothing
below is a hardware measurement. Legacy (compat) behaviour is unchanged in every
row (compatibility gates byte-identical).

**Voice circuit (ws1-voice, plan 1.1b, 1.2, 1.4, 2.5)**
- *Rows 26-29 VCF (IG00156):* circuit engine runs the nodal stage (input networks,
  1.2 MOhm || 180 pF branch, both 1 uF couplings, in-loop OTA saturation, declared
  Q-block and cutoff calibration; [IG00156_NODAL_MODEL.md](IG00156_NODAL_MODEL.md)).
  Status stays A (inferred). Open: top-octave small-signal accuracy at 2x/4x, the
  ADJ p11 HP setup point (1.69 Vpp against 4.3 +/- 1).
- *VCO / WSC (IG00153/IG00158):* circuit engine uses the reconstructed event-driven
  oscillator with the PWL sine ([OSCILLATOR_PROMOTION.md](OSCILLATOR_PROMOTION.md));
  worst saw/pulse non-harmonic -88.3/-88.8 dB at 48 kHz.
- *Row 41 VCA level / IG00151:* linear LI and exponential EI inputs separated, LI node
  RC, declared offset and feedthrough, AC coupling ([IG00151_VCA_MODEL.md](IG00151_VCA_MODEL.md)).
- *Row 11 Sub -> VCA:* circuit engine exponential (EI) law, +/- 1 octave at full depth.
- *Voice dispersion and drift (software rows):* circuit engine declared 5 % R / 10 % C
  component draws mapped into tune, cutoff, Q, level and envelope times, 1/f thermal
  drift with warm-up, and the historical sine as a "worn unit" option
  ([TOLERANCE_DRIFT_MODEL.md](TOLERANCE_DRIFT_MODEL.md)). Status A (declared).

**Engine I/O (ws2-engine, plan 0.2, 1.6, 1.7, Phase 3, Phase 6 host groups)**
- *Row 2 Main volume:* circuit engine smooths VOLUME with a declared 5 ms one-pole.
- *Row 19 Ribbon / performance controllers:* circuit engine smooths ribbon, bend,
  CC1/CC74 and poly/channel pressure over 5 ms; a new note starts from its own touch.
- *Rows 17/18 Foot controller EXP (volume):* the expression gain runs through the PRA
  I2/I3 optical cell (ws4's declared LDR law). Open: the networks' own steady-state law,
  polarity, VR11/12 trims.
- *Row 25 White noise level:* one SUB29 generator for every voice (source-derived
  fan-out), declared Gaussian PSD from the SUB29 component values, rate independent.
- *GENERAL mono output and headphone amplifier:* LISTENING OUTPUT selects the GENERAL
  node or a declared PHONES model (SUB29 -1.5 stage into an inferred 32 Ohm load).
- *Output amplifiers:* ANALOG NOISE hiss is a declared mix-bus op-amp PSD per channel.
- *Engine / numerical backend (software row):* OVERSAMPLING Eco 2x / Standard 2x (set
  from the measured alias after ws1's oscillator) / HQ 8x, offline HQ, reported latency
  63 host samples plus the oscillator's 8 internal samples; fresh instances default to
  the circuit engine (state schema 3).

**Keyboard, touch and tuning (ws3-keyboard, plan 2.1-2.4, Phase 6)**
- *Oscillators/filter state between notes:* all 16 chains free-run in the circuit
  engine (closed-form idle oscillator, decimated nodal stages, snapshot replay at
  wake; FreeRunningVoiceTests against FreeRunningVoiceTruth.h). Cold circuits start
  warm (declared power-on convention).
- *Cyclic assignment and ninth held key:* IB/A; the circuit engine reconsiders a
  still-held ninth key when a circuit frees (declared YM26600 model, zero touch).
- *Keyboard scanner and allocation timing:* A (declared); optional `kas_scan_timing`.
- *Per-voice sample/hold, droop:* A; SH7 NAND(TR,SUS) hold, glide frozen in Sustain I,
  declared 0.033 uF / 1 pA droop.
- *Rows 9-10 Keyboard brilliance and level:* A; KBC1/KBC2 networks with a declared
  61-key code map meeting both adjustment anchors (C2/KAS6 conflict reported). A lever
  move reaches held notes while it is smoothed.
- *Rows 42-45 touch:* TWS b sample/hold (declared acquisition), TWS a 100 ms follower,
  TRG5 diode-OR bus for the shared SUB speed, C = -HPF(b) with ideal clamp for the
  initial pitch bend. Sensor force law and calibration open.
- *Tuning and MIDI (EXT):* Scala/KBM and MTS SysEx; MTS-ESP client not vendored (shim);
  MPE zones with an upper zone and CC88; voice modes 8/16/unison; ribbon from MIDI CC.

**Original modulation and effect circuits (ws4-circuits, Phase 3)**
- *Row 11 Sub oscillator speed and function:* IG00150 exponential converter anchored at
  0.7/60 Hz and 200 Hz full SPEED plus full touch (declared); band-limited saw/square.
- *Row 12 Aftertouch -> sub speed:* touch added in octaves; composes in series with the
  TRG5 bus.
- *Row 16 Ring speed/depth/amount:* exponential converter through the ADJ p19 anchors
  (150.01 ms / 5.000 ms); declared -40 dB carrier and signal feedthrough and DC offset;
  500 Hz extension masked at 200 Hz in PURE CS-80.
- *Row 15 Tremolo/chorus, LEFT and RIGHT outputs:* OE1 four-VCA tremolo; OE2 true L/R
  wet paths with per-transfer loss, output-pair hold, independent noise and band-limited
  clock feedthrough. Added 2026-10-31: the append-only `chorus_tabs` parameter
  makes both-off, individual on, and both-on engine states representable;
  Historical remains the neutral default and preserves the previous toggle.
- *Row 17 EXP / EXP-WAH:* PRA optical cell, declared asymmetric LDR law.

**Modern layer (ws5-modern, Phase 6; all EXT, inert in PURE CS-80)**
- LFO waveforms (sine bit-identical), MSEG 1-3, 16-slot modulation matrix tapping the
  original controllers without changing their circuits, keyboard modulation curves for
  matrix sources only, modern FX (BBD ensemble, plate, spring, tape echo, oversampled
  drive, loudness matching; classic slot FX bit-identical). Mounted in the Studio view.

**Tones, memories and library (ws6-library, Phase 4)**
- *Row 3 tone selectors:* 25 unknown cells re-examined; all stay labelled hypotheses;
  opt-in refined BA/diode boundary (V1 numerics unchanged).
- *Row 46 four memory panels:* electrical model with a truth model; as of
  2026-10-31 the selected memory/PANEL receiver drives circuit-engine control
  values while preserving the legacy APVTS path and the unchanged V1 gate.
  Voltage and taper curves remain declared schematic inferences, not hardware
  measurements (`d66d298`).
- *Factory programs:* 301-program curated circuit-engine library; the 222 shipped
  programs are the read-only Legacy Library at their old host indexes.
- *MIDI / editing:* Program Change with bank select (inert in PURE CS-80); A/B compare
  and randomise within original ranges (one undo step each).

**Original panel UI (ws7-panel, Phase 5)**
- *Physical presentation:* main view follows [PANEL_CONFORMANCE.md](PANEL_CONFORMANCE.md)
  (memory lid upper left, rows to its right, contour levers on the front strip, foot
  assigners and EXP left, REAR PANEL block). Global lever direction still drawn upward:
  open.
- *Rows 3, 9-10, 13-15, 17, 46, HIGH/LOW:* full tab names; levers and switches moved to
  the documented blocks; separate tremolo/chorus keys (both-off/both-on states need an
  engine parameter: completed 2026-10-31, `6f1c4aa`); memory-lid STORE PANEL /
  COPY MEMORY / COPY RANK.

**Modern UI (ws8-studio, Phase 5/6; EXT)**
- *Row 19 Ribbon (UI):* full bend from any first-contact point (declared UI
  inference); PURE CS-80 keeps the proportional law.
- *61-key playing surface:* computer-keyboard note input.
- *Presentation:* toolbar, five-tab Studio view, voice-monitor drawer, controllers
  strip hidden in PURE CS-80.
- *Modern layer:* MIDI learn, undo/redo, typed values and context menus, guided tour,
  per-control help and the in-app evidence register.

## Status and evidence rules

- **IMPLEMENTED_BEHAVIOR (IB):** a specified discrete or qualitative behavior exists in code. This says nothing about unmeasured transfer curves.
- **APPROXIMATE (A):** a route or audible effect exists, but its law, range, topology or state evolution is provisional.
- **MISSING (M):** the original function is absent, or a materially different digital function occupies its place.
- **MEASUREMENT_REQUIRED (MR):** hardware or component characterization is needed to validate the claim.
- **PHYSICAL_CONTROLLER_REQUIRED (PC):** software alone cannot reproduce the original force, travel, surface or mechanics; an appropriately characterized controller is required.
- Multiple statuses are intentional. No percentage-complete metric is used.

Every acceptance result must include test stimulus, control positions, source/reference identity, measured error and exact code revision. Existing test names below are regression locations, **not claims that this audit executed or passed them**. See exact-revision Actions results. A pending worker's implementation is not an implemented row until integrated and reviewed.

## Reference and code keys

Primary Yamaha source locators are maintained in [HARDWARE_REFERENCES.md](HARDWARE_REFERENCES.md), [CIRCUIT_MODEL_SPEC.md](CIRCUIT_MODEL_SPEC.md) and [PRESET_CIRCUIT_LEDGER.md](PRESET_CIRCUIT_LEDGER.md). They contain source links and distinguish documented facts from inference.

- **O:** Yamaha original owner manual, printed pages; index p1 identifies numbered controls1–46. [Original scan, part1](https://www.therogoffs.com/cs80/CS80E_1.pdf), [part2](https://www.therogoffs.com/cs80/CS80E_2.pdf), [part3](https://www.therogoffs.com/cs80/CS80E_3.pdf).
- **S:** [Yamaha service-manual page index](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/index.html). S1 specifications, S2 panel, S39 control summing, S41 voice board, S42/S52 IC descriptions. TKC/TSB keyboard/touch drawings must be read directly before quantitative touch claims.
- **ADJ:** [Yamaha adjustment procedures and updates](https://www.therogoffs.com/cs80/CS80%20Adjustment%20Procedures%20and%20updates.PDF). Source calibration conditions are mandatory, not optional context.
- **P:** `src/core/Params.h`; **E:** `src/core/Engine.cpp`; **D:** `src/core/Dsp.h`.
- **UI:** `src/plugin/PluginEditor.cpp`, `src/plugin/UiComponents.h`; **HOST:** `src/plugin/PluginProcessor.cpp`.
- **T:** `tests/CoreTests.cpp`; **UT:** `src/tools/ui_snapshot.cpp`; **HC:** `src/core/HardwareControl.h`, `tests/HardwareControlTests.cpp`.

## Original numbered global controls

|Owner index / feature|Current status and implementation|Evidence and unresolved issue|Next acceptance action|
|---|---|---|---|
|1 AC power switch|M/MR; host lifecycle substitutes for power|O6,39; E prepare/reset is not a supply ramp or warm-up model|Separate user power/mute behavior from modeled rail startup, bias settling and thermal tuning; measure startup without asserting unsafe fault simulation|
|2 Main volume|A/MR; P volume, E final gain/soft clip|O6,38; gain taper and output headroom uncalibrated|Map potentiometer and output amplifier gain; compare level/distortion across travel|
|3 Two tone-selector rows|IB/A; HOST source selectors, UI14 positions each|O7,37; correct rank-specific labels/selection, sound values provisional|Verify every switch combination routes correct resistor network and does not overwrite PANEL|
|4 MIX I–II|A; E linear rank gains|O7,37; physical pot taper/loading and null/endpoints unverified|Extract passive mixer and measure each rank separately and together|
|5 FEET I and II|IB/MR; P six nominal harmonic ratios; T testFeetRatios|O8,37; nominal ratios not measured switch/trim tolerances|Check all six settings on each rank, reference key and CV, including fifth stops|
|6 DETUNE CH II|A; P/E positive cents offset|O8,37; actual center, direction, range and law need circuit reconciliation|Trace detune voltage injection and measure endpoints/intermediate settings|
|7 Overall brilliance|A; E adds shared effective cutoff offset|O8,38; HP and LP loaded paths differ; universal equal scaling is unverified|Solve both summing paths and check lever direction against panel photograph|
|8 Overall resonance|A; E combines global/per-filter amounts then provisional Q map|O8,38; downwards physical increase; gains/interaction uncalibrated|Measure Q and peak amplitude for full global×local control grid|
|9 Keyboard brilliance LOW|A/MR; `kbd_brilliance_low`, per-key octave-tap weights add to both filters' effective CV (PC)|O9,38; evidence ledger. Weights, 2-octave full scale, equal HP/LP split and smooth interpolation are provisional; key map is software, not KAS/KBC|Resolve the KAS B-code to key map, solve loaded FH/FL nodes, then replace constants; see PERFORMANCE_CONTROLS.md|
|9 Keyboard brilliance HIGH|A/MR; `kbd_brilliance_high`, same route, centre-neutral (exactly zero)|O9,38; separate high-end control; lever polarity unverified|Verify lever direction against terminals/photo and key-dependent HP/LP contribution|
|10 Keyboard level LOW|A/MR; `kbd_level_low`, additive VCA control offset (separate KB branch), 0.4 full scale provisional|O9,38; KBC2 to KB branch via 270k to LC; filters untouched|Derive level-control injection from the LC node and test all keys|
|10 Keyboard level HIGH|A/MR; `kbd_level_high`, same route|O9,38; symmetry with LOW is an assumption|Measure high-end slope/endpoint and interaction with LOW|
|11 Sub oscillator function selector|IB/A; original sine, falling/rising saw, square, noise, optional mono EXT modulation; legacy shapes retained separately|O8,37,49; SUB29/30 and PN2 switch routing; ideal waveforms, not nonlinear IC equivalence|Calibrate EXT physical voltage/headroom and waveform shape/noise spectrum/amplitude; see ORIGINAL_SUB_OSCILLATOR.md|
|11 Sub oscillator speed|IB/A; original0.7–60Hz, separate legacy0.05–30Hz automation untouched|O49 endpoints; intermediate taper and10ms original-mode rate smoothing provisional|Derive quantitative IG00150 exponential conversion and full combined speed/AT response|
|11 Sub → VCO|A; E semitone modulation|O8,37; real pitch-CV injection may not be exponential semitone FM|Compare carrier deviation/sidebands across pitch, depth and waveform|
|11 Sub → VCF|A; E equal shared offset|O8,37; HP/LP path gains/loading unverified|Trace separate destinations and verify modulation depth and polarity|
|11 Sub → VCA|A; E linear level addition/clamp|O8,37; actual VCA control law unverified|Measure tremolo law, minimum level and clipping at full modulation|
|12 Initial pitchbend|A; E depth-independent nominal50ms positive onset, live PB scaling|S17/23: conditional1µF×(100k||100k) pole; FET acquisition/clamp/retrigger still approximate|Derive the separately AC-coupled TWS C path and compare pitch trajectory at multiple velocities|
|12 Aftertouch → sub speed|IB/A; maximum held pressure, provisional bounded multiplier|TRG5 diode-OR; SUB29 adds control currents into IG00150 exponential converter, not proof of additive Hz|Calibrate complete touch-bus/current-to-frequency law; legacy behavior unchanged by original-mode addition|
|12 Aftertouch → sub VCO|A; E per-voice pressure multiplier|O10,38; correct destinations do not prove gain law|Trace touch bus and measure depth response per key/voice|
|12 Aftertouch → sub VCF|IB/A in 71f0483; appended P_subAtVcf and host parameter|O10,38; independent physical green control; software CI passes, numerical depth provisional|Calibrate pressure/depth transfer and receiving circuit; see AFTERTOUCH_SUB_VCF.md|
|13 Sustain amount|IB/A; E extends both filter/amplitude release|O11,39; nominal release priority implemented; physical foot switching incomplete|Test at all release/control combinations with and without actual footswitch|
|13 Sustain I/II selector|IB/A; E held-tail mode handling; T testSustainTimeAndWheelRouting|O11,39; mode edge cases and release cancellation timing unmeasured|Compare new chords, repeated notes, ribbon moves and pedal transitions|
|13 Sustain footswitch assigner|IB/A; `footswitch_plugged/pressed`, `sustain_assign`, truth table `!assigned or !plugged or pressed`; gate removes only the SUSTAIN release term and Sustain II tail privilege; falling edge shortens tails without restarting them|O11,39; FOOT_CONTROL_EVIDENCE.md. Latching virtual switch; cancellation policy and edge transient are provisional; CC64 untouched|Measure switch edge and the real release-cancellation transient|
|14 Portamento/glissando amount|A; E exponential glide and pitch rounding|O12,39; original stepped control and smoothing chain not reconstructed|Extract control circuit; compare step timing, glide trajectory and note-release behavior|
|14 Portamento/glissando selection|A; P three-state digital choice|O12,39; gliss/porta relationship and sustain interactions need hardware confirmation|Test exact physical switch states rather than assume generic mono-synth glide|
|14 Portamento footswitch assigner|IB/A; `glide_assign` with the same shared switch; closed gate suppresses new glides, snaps running ones and disables glissando rounding|O12,39; independent of the sustain assigner (tested for all 16 combinations)|Compare step timing and release behavior with a physical unit|
|15 Tremolo/chorus on/off|IB/A; P/UI switch, D Chorus bypass|O13,39; bypass state continuity and level change uncalibrated|Measure switching transient, phase continuity and gain|
|15 Tremolo versus chorus|A; D uses amplitude-only tremolo or modulated delay chorus; source policies use the OE2 BBD delay path for chorus ([model](BBD_CHORUS_MODEL.md))|O13,39; not a reconstruction of full original phase/amplitude network|Trace service circuit, reproduce both modes and stereo transfer|
|15 Speed and depth|A; P/D free ranges and gains|O13,39; mode-related rates/tapers uncalibrated|Measure stereo phase/amplitude versus speed/depth in each mode|
|16 Ring speed|A; E sine carrier; range retains500Hz extension|O13–14,37,45; service calibration anchors exist|Map original control domain separately from extension; reproduce ADJ period targets|
|16 Ring depth|A; E provisional base-frequency-dependent excursion|O45; no calibrated excursion law|Derive envelope-to-VCO transfer and measure speed×depth matrix|
|16 Ring attack and decay|A; D envelope; E gate handling|O13–14,45; ADJ attack target available, full taper unknown|Match calibration setup then contour/retrigger behavior at extrema|
|16 Ring modulation amount|A; E ideal multiplication/dry interpolation|O13–14,45; multiplier gain, carrier bleed/distortion and mix law unmeasured|Characterize multiplier DC/audio transfer and sidebands|
|17 Foot pedal selector EXP / EXP-WAH|A/MR/PC; `foot_mode`; dedicated post-mix PRA stand-in (resonant band-pass, 350 Hz + 2.9 oct, Q 4.5) crossfaded against the dry branch; CC11 or on-screen PEDAL drives it|O14,39; PRA53 shows a dedicated filter/bypass network. Coarse placement verified in OE1_SIGNAL_ROUTING_EVIDENCE.md; sweep, Q, gain and optical dynamics remain unmeasured|Solve the RC network with measured optical resistances and validated source/receiver loading|
|18 Pitch tuning|A; P master_tune/tuning_ref|O14,37; digital A4 reference is not original knob circuitry|Calibrate tuning range/taper and effect on shared key-CV destinations|
|19 Ribbon|IB/A/PC; UI relative first contact, E ribbonFrequencyRatio; T testHardwareRingAndRibbon|O14,44–45; touch mechanics, voltage curve, contact noise and exact routing unverified|Capture contact start/drag/release, full travel and sustain modes; characterize suitable physical controller|

## Both programmable synthesis rows

Each row below applies independently to rank I and rank II. Existence in both main panels and the appropriate four miniature panels must be checked, not inferred from a shared label.

|Owner index / feature|Current status and implementation|Evidence and unresolved issue|Next acceptance action|
|---|---|---|---|
|20 PWM speed|A; P/E rank PWM phase|O15,32; frequency law/waveform and analog independence uncalibrated|Measure PWM rate and shape per rank, state continuity and spread|
|21 PWM depth|A; E fixed0.35 modulation and clamps|O15,32; gain/clipping not circuit-derived|Measure duty-cycle extrema versus depth and PW|
|22 PW duty|IB/A; P50–90% setting, D pulse generator|O14,32; ON/OFF duty convention and actual pulse polarity require verification|Compare duty, polarity, amplitude and narrow reset components|
|23 Square wave on/off|IB/A; P pulse switch|O14,32; waveform switch loading/bleed absent|Verify isolated square and mixed square+saw voltage/spectrum|
|24 Sawtooth on/off|IB/A; P saw switch; T testOscillatorDiscontinuities|O14,32; bandlimited falling saw not full IG00153 model|Match original ramp/reset and per-note free-running phase|
|25 White noise level|A; E pseudorandom broadband source|O15,32; noise PSD, common versus independent sources and level taper unknown|Trace source fanout, characterize spectrum/correlation and gain|
|26 HPF cutoff|IB/A; E nominal key-voltage/cutoff law; D SVF|O18,32; loaded control mapping and board gain incomplete|Apply ADJ setup, measure frequency/phase/level over control/key grid|
|27 HPF resonance|A; provisional Q mapping|O18,32; global/local summing and internal nonlinear feedback unknown|Measure resonance, damping, peak gain and signal dependence|
|28 LPF cutoff|IB/A; same nominal law, separate instance|O18,32; circuit differs from HP path|Validate LP-specific source impedances, calibration and transfer|
|29 LPF resonance|A; provisional Q mapping|O18,32|Measure independently and jointly with HP resonance|
|30 Filter IL|IB/A; D FilterEnv negative initial offset|O19,33–34; voltage excursion and loading provisional|Measure TP1 and both downstream cutoff offsets|
|31 Filter AL|IB/A; positive attack peak|O19,33–34; same voltage/loading boundary|Verify initial→peak→zero shape and control law|
|32 Filter attack|A; software time coefficient|O19,33–34; present range exceeds nominal hardware and curve is not derived|Derive timing law; verify minimum/maximum and interrupted attack|
|33 Filter decay|A; D FilterEnv|O19,33–34; bent/threshold behavior provisional|Compare transient at multiple AL/IL and timing settings|
|34 Filter release|IB/A; release toward IL, global sustain priority|O19,33–34; actual contour law/foot gating unresolved|Test release during every stage and after long hold|
|35 VCF level|A; E scales filtered signal before sine mix|O22,35; gain/polarity/headroom uncalibrated|Match CP output levels and simultaneous waveform cancellation/reinforcement|
|36 Sine level|A; E bypass mix; D sine plus fixed third harmonic|O22,35; third-harmonic coefficient is not measured IG00158 characterization|Measure WSC shape, amplitude, polarity and level taper|
|37 VCA attack|A; D Adsr|O21,34; endpoint/curve/transition threshold assumed|Compare amplitude voltage and acoustic envelope separately|
|38 VCA decay|A; D Adsr fast-bend heuristic|O21,34; arbitrary bend is not circuit proof|Derive IG00159 timing and validate full dynamic range|
|39 VCA sustain level|A; D sustain fraction|O21,34; control voltage→gain mapping unverified|Measure held gain and attack/decay transition at all sustain settings|
|40 VCA release|IB/A; release and sustain priority implemented|O21,34; exact timing/shape unmeasured|Compare release from attack, decay and sustain, retriggered and stolen-tail cases|
|41 VCA level|A; E levelCv multiplication/clamp|O22,35; IG00151 linear/exponential pins not separately reconstructed|Solve control summing and characterize both gain-control inputs|
|42 Initial-touch brilliance|A; E holds velocity offset, qualitatively consistent with held TWS b path|S17/19: b feeds IBI/IBII-controlled VCA banks; earlier shared-decay interpretation withdrawn|Derive sample/hold transfer, loading and brilliance injection; do not force late cutoff to baseline|
|43 Initial-touch level|A; held velocity route qualitatively consistent with TWS b path; unconditional gain removed in 71f0483|S17/19/23: b feeds ILI/ILII-controlled VCA banks; zero-touch invariance regression passes|Calibrate held level transfer; keep separately derived pitch transient distinct|
|44 Aftertouch brilliance|A; E per-voice pressure offset|O23,36,44; sensor law, HP/LP ratios and aggregation unverified|Measure pressure→two filter CVs including saturation/hysteresis|
|45 Aftertouch level|A; E per-voice pressure level addition|O23,36,44; sensor/VCA law uncalibrated|Test polyphonic independence and gain response across force|
|46 Four memory panels|IB/A; HOST104 controls plus two selectors, UI lid|O23,36; logical banks and persistence implemented, physical divider/loading not|Verify each bank's26 controls, switch networks, rank pairing and source changes|

### Programmer and preset completeness

The selectors preserve the documented rank-specific order listed in HARDWARE_REFERENCES. Memories1/3 feed rank I and2/4 rank II. HOST/UT cover source resolution, four-bank state, migration and automation; [MEMORY_AUTOMATION.md](MEMORY_AUTOMATION.md) records the compatibility contract. These are digital integration checks.

HC encodes the verified subset of passive preset branches and distinguishes absent/unknown/direct/resistor paths. It is **not connected as a complete calibrated factory preset generator**. All22 named factory-tab studies remain provisional. The200 additional programs are modern additions, not original factory presets. Remaining acceptance: transcribe every preset node, switch, resistor, diode and receiving impedance; solve loaded CVs; map into IC laws; test both ranks and compare authorized reference captures. Owner panel recipes are useful cross-checks, not substitutes for the preset resistor matrices.

## Keyboard, connectors and physical presentation

|Feature|Status / code|Gap and concrete acceptance action|
|---|---|---|
|61-key playing surface|IB/A/PC; UI keyboard|Verify physical key labels/range against S1/S2; GUI note range alone is not proof of original octave convention or mechanics|
|Key travel, mass, force, return and initial contact timing|M/PC/MR|MIDI velocity is only an interface proxy. Characterize original travel/contacts and controller response; distinguish mechanics from sound engine|
|Independent pressure per key|IB/A/PC; E polyPressure, T testMpeExpression|External MIDI poly pressure supported; on-screen keys now have a drag-down pressure gesture (mailbox to the audio thread). It is a pointer stand-in: original sensor curve/crosstalk are not modelled|
|Eight dual-rank voice circuits|IB/A; E8×2 voice structure|Test independent envelope/control states, simultaneous16 VCO behavior and analog voice spread|
|Cyclic assignment and ninth held key|IB/A; T testHardwareVoiceAssignment|Held-note nonstealing/cycling implemented; reconsideration of an ignored still-held ninth physical key after release remains unresolved|
|Keyboard scanner, debounce and allocation timing|M/MR|MIDI events replace scan electronics. Derive KAS/TKC timing and event priority; do not substitute multiplex scan periods for touch decay|
|Per-voice sample/hold, droop and retained key voltage|M/A/MR; note state only|Trace actual gates, hold intervals and sustain-II differences; model leakage and settling with evidence|
|Oscillators/filter state between notes|A with mismatch; E startVoice resets phase/filters, inactive voices skipped|Implement proven free-running/held-state behavior, not merely removing reset while freezing idle time|
|Foot controller socket/pedal|A/PC; E expression CC11 plus on-screen PEDAL wheel; EXP/EXP-WAH selector now present|No physical electrical socket/load model; optical pedal resistance, travel and taper unmeasured. Characterize pedal and its lamp/photosensor separately from the PRA stage|
|Footswitch socket|IB/A/PC; jack-plugged and contact-pressed parameters with the connected/disconnected/pressed truth table for both assigners|MIDI sustain (CC64) remains a separately documented extension; no MIDI-to-jack adapter exists because MIDI cannot report plug state|
|External input and level|IB/A; optional disabled mono bus, inverted software normalization, LEVEL and shared SUB routing|Alias-safe input capture and255-tap2x reconstruction; extra63.5host-sample control delay is software, not analog. Physicalvoltage/pot/headroom uncalibrated;10k schematic vs50k manual conflict; see EXTERNAL_MODULATION.md|
|GENERAL mono output|A; E digital mono fold-down|Trace actual summing and output impedance/level; digital average may not match hardware|
|LEFT and RIGHT outputs|A; stereo engine|Reconstruct exact routing, phase, gains and channel interaction with chorus/tremolo|
|HIGH/LOW output switch|A/MR; `output_level`, HIGH unity / LOW provisional -20 dB trim, smoothed|No documented level difference in the evidence set; digital stereo/mono output mode is a different control|Obtain the specification or measure level, loading and headroom for both positions|
|Headphone jack/amplifier|M/PC/MR|Output-stage model and routing absent; host headphones do not reproduce original amplifier/load behavior|
|Power supply, mains and fuse|M/MR|Rails/ripple/bias/thermal effects lack model. Represent audible behavior safely; do not imply software reproduces electrical safety or hazardous faults|
|Panel geometry, colors, lever directions and labels|A; UI/UT screenshots|Real-hardware-inspired layout exists. Verify every control against S2 and Yamaha color photos; measure dimensions/tapers and accessibility separately|
|Memory lid, miniature panel interaction and legends|IB/A/PC; UI magnified overlay|Functional opening/closing exists, magnification is deliberate software adaptation; original dimensions/hinge mechanics unmodeled|
|Ribbon surface, felt/cord/contact mechanics|M/PC/MR|Pointer/touch trajectory does not recreate tactile response; separate physical-controller requirements from voltage mapping|
|Walnut/leatherette case, legs, lid, vents and transport hardware|A/M/PC; visual chassis only|Track visible fidelity against original photos/dimensions. Software cannot physically reproduce weight, support, ventilation or transport hardware; do not mark these emulated by a texture|
|Mechanical control travel/detents/switch bounce|A/M/PC|GUI sliders lack original feel and contact behavior. Measure any audible transient before adding a circuit effect; optional tactile hardware is separate|

## Circuit-level closure checklist

None of the following is certified1:1. This section prevents completion based solely on visible controls.

|Circuit / component class|Baseline evidence and code|Required closure|
|---|---|---|
|IG00153 VCO|D analytical oscillator; nominal Hz/V and footage evidence in S42|Voltage-domain ramp/reset, waveform gain, pitch-input/vibrato interaction, idle/free-running state, measured tuning and temperature dependence|
|IG00158 waveform converter|D sine harmonic heuristic and pulse shaping|Actual nonlinear wave-shaping transfer, duty/polarity, source/load impedances, calibrated harmonic amplitudes|
|IG00156 HP/LP filters|D TPT SVF, E nominal fc law; T testFilterControlDomain/testFilterStability|Full board gain/polarity, Q law, nonlinear dynamics, input limits, bias/slew/bandwidth and loaded control nodes|
|IG00152 filter EG|D FilterEnv qualitative polarity sequence|Actual timing-control law, capacitor state, stage thresholds, restart/release behavior and loaded HP/LP injection|
|IG00159 VCA EG|D Adsr heuristic bend|Derive actual attack/decay/release curves and sustain transfer, continuity and retrigger state|
|IG00151 VCA|E scalar gain/clamp|Separate exponential and linear input laws, distortion, offsets, noise, bleed and loading|
|IG00150/modulation oscillators|E/D sinusoidal/modulation generators|Original oscillation law, waveform distortions, envelope control, pressure response and calibration|
|Preset, memory and panel resistor/diode networks|HC partial passive solver; source ledger|Complete node inventory, diode operating point, source/load impedances, pot tapers, switch states and all22 preset results|
|R51/R52 and other control summing|E shared approximations|Solve distinct HP/LP/VCA/VCO paths with actual resistor networks and trims; avoid treating knob fractions as volts|
|TKC/TSB initial/aftertouch circuitry|E velocity/pressure proxies|Separate filtered a, sampled/held b and AC-coupled C paths, force conditioning, key-specific response and global pressure aggregation; see INITIAL_TOUCH_CIRCUIT_EVIDENCE.md|
|Key voltage generation, scanner and assigner ICs|E event-based allocator|Full service-board/BOM inventory still required; exact scan/hold sequence, priority and analog settling|
|Ring multiplier and carrier/envelope circuit|E ideal multiplication and provisional sweep|Carrier feedthrough, control law, offsets, nonlinear intermodulation, envelope timing and original gain|
|Chorus/tremolo circuitry|D modulated delay/amplitude approximation|Trace actual phase/amplitude network and all active devices; validate stereo transfer rather than infer from generic BBD label|
|Output amplifiers and coupling networks|E10Hz DC block, gain and softClip|Derive real coupling poles, headroom, impedance, output switching, noise and load response|
|Supply distribution and bias references|Not reconstructed|Nominal rails and loading, ripple coupling, startup/settling and thermal dependence where relevant to observable behavior|
|Resistors/capacitors/diodes/transistors/op-amps/switches/trimmers|Not complete component-level inventory|BOM and netlist with value/tolerance/revision; identify measured vs nominal device models and calibration positions|
|Voice variation and aging|E random trims/slow sinusoidal drift/noise|Calibrated distributions, correlations and thermal time constants; arbitrary randomness cannot establish authenticity|

### Unsupported sonic assertions to remove or replace with evidence

At this baseline, D comments call its generic input soft clip part of the IG00156 sound, attribute a fixed third-harmonic coefficient to the original sine shaper, and describe a heuristic ADSR bend as hardware behavior. E calls the approximate chorus authentic. Those statements exceed their evidence. Treat these as **provisional coloration**, not measurements, until source-derived or measured models replace them. A future code cleanup should correct the assertions without silently changing sound under the guise of documentation.

## Verification matrix and next priority

1. **Functional omissions: closed in software, calibration open.** Keyboard contours, both footswitch assigners, EXP/EXP-WAH and the output switch now exist with stable host IDs, migration, UI controls and route tests (PERFORMANCE_CONTROLS.md). Every gain in them is provisional. Calibrated aftertouch-sub-VCF and original sub waveform/EXT circuitry remain open as before.
2. **Correct known state/behavior mismatches:** separate held initial-touch and pitch-transient circuit laws, free-running VCO/held filter state, scanner/overflow semantics and glissando topology. Obtain missing circuit evidence before assigning numerical constants.
3. **Complete electrical reconstruction:** board/BOM inventory, passive control networks, IC voltage domains and stage-level trace points. Use ADJ conditions and preserve source revisions.
4. **Characterize analog nonlinearities:** level-dependent transfer, transient response, modulation sidebands, noise/correlation and thermal/tolerance behavior for an identified calibrated reference instrument.
5. **Validate all presets and performance combinations:** both ranks, all memories, every selector, keyboard range, pressure, ribbon, pedal and sustain interactions. Include interrupted/repeated gestures and changes during sounding/releasing notes.
6. **Retain numerical/host quality gates:** core tests, HC tests, DSP probes, sanitizers, host validators, UI interaction/screenshot tests and parameter/state compatibility. These validate software quality independently of hardware fidelity.

Existing T coverage includes oscillator discontinuities, feet, filter domains/contours/stability, ring/ribbon, sustain, allocation, pressure, MIDI panic, sample rates and rendering. UT covers host/UI state and programmer gestures. Passing these does not supply missing circuit reference measurements.

For each row, promotion to verified fidelity requires a linked evidence artifact containing the original control/test-point setup, recorded stimulus/reference, residuals across the operating range and a stated uncertainty. Unobserved or unmeasured behavior stays open. The project must not be called complete merely because every table row has a parameter or every CI job is green.
