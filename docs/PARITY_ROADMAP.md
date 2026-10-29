# Roadmap to 1:1 software parity with the original Yamaha CS-80

Written 2026-10-29 for the next developer. It states where the engineering actually stands, what the public-knowledge 1:1 target means, and the ordered work that remains. It is deliberately blunt: the instrument today is a **circuit-informed behavioral model**. The original software-meaningful panel controls have implemented routes, but almost no numerical law is calibrated against the original, so **verified hardware parity is not established**. The updated 2026-10-29 requirement permits explicitly labeled engineering inference and does not make hardware access a development gate; substantial modeling and integration work still remains.

Read [ANALOG_FIDELITY_REQUIREMENT.md](ANALOG_FIDELITY_REQUIREMENT.md) (the target), [FIDELITY_AUDIT.md](FIDELITY_AUDIT.md) (the dated gap register) and [HARDWARE_COVERAGE.md](HARDWARE_COVERAGE.md) (every control, row by row) alongside this file. This file is the plan; those are the evidence.

---

## 1. Definition of done

The standing requirement (updated by the user 2026-10-29) is the **closest 1:1 CS-80 reconstruction achievable from publicly available knowledge and sound engineering**, covering analog behavior and every software-reproducible feature, control, tone and preset. Missing hardware must not block implementation, integration, milestones or release. For each subsystem and then end to end:

1. **An evidence/model row**: primary document and page where available, circuit derivation, implemented equations, unknowns and explicitly labeled engineering assumptions. Prefer stronger public evidence; preserve contradictions.
2. **A declared reference and uncertainty**: source-derived nominal predictions, an independently evaluated engineering model, or identified calibrated hardware captures if available. An inferred model is an implementation hypothesis, not hardware measurement. Record sensitivity to uncertain parameters and credible alternatives.
3. **Tolerances fixed before evaluation**, using explicit metrics (magnitude/phase, spectra, envelope curves, noise statistics, CV voltages). Separate solver error, agreement with public anchors and optional measured hardware residuals. No percentage fidelity score without a metric and corpus.
4. **Results published** across control, pitch, amplitude and polyphony ranges, with fitted and held-out settings separated. Do not manufacture a reference corpus or hide disagreement.
5. **Stage-level validation**, then complete-path tests and level-matched, latency-aligned perceptual regressions. Public recordings may guide listening but are not calibrated electrical evidence unless their provenance supports it.
6. **Software quality gates stay green** (core tests, sanitizers, host validators, UI tests), with compatibility and numerical convergence checks. They prove the software obeys its spec, not that every inferred law equals the instrument.

A public-knowledge reconstruction milestone can be accepted with disclosed inferences and bounded limitations; absent hardware measurements alone do not prevent it. An unqualified verified 1:1, measured-hardware parity or bit-exact analog claim remains prohibited without corresponding evidence. Every control having a parameter and every CI job being green do not by themselves establish completion.

---

## 2. Evidence classes, not hardware blockers

Know which evidence class a model belongs to and keep that label through integration.

| Class | What it is | Work without a CS-80 | State |
|---|---|---|---|
| **A. Document-derived** | Topology, wiring, nominal laws, resistor networks, calibration anchors, switch truth tables, control routing | Transcribe, derive, solve and test from public sources | Substantial work done; more remains in W1, W2, W9 |
| **B. Engineering-inferred** | Quantitative laws or device behavior not fixed by available sources: nonlinearities, envelope bends, noise, drift, waveform shapes, touch dynamics | Implement a justified physical/behavioral model, declare assumptions and ranges, test sensitivity and regressions; optionally refine later with measurements | Many current laws are provisional; no calibrated hardware corpus exists |
| **C. Physical interface limits** | Key weight/travel, ribbon felt, chassis and mains safety | Model audible/control consequences; document physical-controller needs, never claim physical replication | Documented as PC/M in the coverage ledger |

**Missing hardware is not a development blocker.** A known feature whose quantitative behavior cannot be recovered publicly must receive a defensible, explicitly inferred implementation rather than remain indefinitely unimplemented. Do not invent a source value or resolve contradictory documents by silently picking one; record the conflict and justify the chosen model or configurable alternatives. Model acceptance requires the evidence and testing in sections 1 and 7, not instrument access.

The [capture request](REFERENCE_CAPTURE_REQUEST.md) remains an optional future validation plan, separating safe external intake from qualified-technician internal work. Its unfilled checklist is not an accepted comparison manifest. If hardware or authorized calibrated captures become available, record provenance and redistribution rights and refine the model. Never probe a powered vintage unit casually. Such captures can establish measured agreement that software-only work cannot, but they are not a gate for later milestones.

Practical note on sources: earlier container requests encountered 403/unreachable mirrors, so source inspection used GitHub Actions (section 10). On 2026-10-29, native `therogoffs.com` scans downloaded successfully with curl in the cloud checkout; Python urllib still returned 406 for some of the same URLs. Verify current access rather than treating historical failures as permanent. Preserve source-byte hashes and native crop provenance with either route.

---

## 3. Where the engineering stands

Status vocabulary, used here and in the coverage ledger: **IB** behavior implemented; **A** approximate (route exists, law is provisional); **M** missing; **MR** hardware measurement outstanding (optional refinement, not an implementation blocker); **PC** physical controller required.

Of the 85 control and connector rows in HARDWARE_COVERAGE.md, counting each row by its first status tag, **29 are IB, 49 are A and 7 are M**. The 7 M rows are all physical or electrical-state items: AC power switch, key travel/force, scanner timing, per-voice sample/hold droop, headphone amplifier, power supply, ribbon surface mechanics. Treat "A" as "present but not equal".

| Subsystem | Code | Implemented | Not established | Evidence docs |
|---|---|---|---|---|
| Voice structure | `Engine.cpp` | 8 voices x 2 ranks (16 chains), cyclic assignment, ninth held key ignored, MIDI note-off channel-exact | Real KAS/TKC scan, priority and re-reconsideration of an ignored key; free-running vs reset oscillator and filter state | FIDELITY_AUDIT.md, KEYBOARD_CONTOUR_EVIDENCE.md |
| VCO (IG00153/IG00158) | `Dsp.h` `Oscillator` | PolyBLEP saw (corrected polarity), comparator-style pulse, sine, noise, nominal harmonic footage `[0.5,1,1.5,2,3,4]` | Ramp/reset trajectory, ~30 us reset assumption and amplitude, sine shaper (fixed 3.5% third harmonic is a placeholder), per-wave levels, Hz/V calibration, temperature | OSCILLATOR_CIRCUIT_EVIDENCE.md, DSP_MEASUREMENTS.md |
| VCF (IG00156) | `Dsp.h` `Svf`, `provisionalFilterQ` | Two serial TPT state-variable stages, documented cutoff law `fc = 125 * Kv * 2^V`, Q endpoints 0.5 and 5, HP and LP separate | Intermediate Q law and the internal Q-block law (the functional graph is recovered but only as an offline conditional model), the 4.7 MOhm feedforward and 1.2 MOhm/180 pF network in the audio path, nonlinearity (current `softClip(1.1x)` is a placeholder), grade A/B/C per voice | IG00156_MODEL_EVIDENCE.md, IG00156_REFERENCE_EXPERIMENT.md, CIRCUIT_MODEL_SPEC.md |
| Envelopes (IG00152, IG00159) | `Dsp.h` `FilterEnv`, `Adsr` | IL-AL-steady-release-to-IL ordering, global SUSTAIN as release floor, sustain I/II | True time laws, "bent" start (a x5 factor over the first ~35-45 ms is a heuristic), voltage-to-octave scale (`kProvisionalFilterEnvelopeOctaves = 3`), HP/LP gain split | FIDELITY_AUDIT.md, CIRCUIT_MODEL_SPEC.md |
| VCA (IG00151) | `Engine.cpp` | Level CV sum, sine bypass after the filters | Separate exponential and linear inputs, distortion, offsets, loading | HARDWARE_COVERAGE.md |
| Touch | `Engine.cpp` | Velocity as held "b" path, separate AC-coupled "C" pitch transient (nominal 50 ms pole), shared max-pressure bus, per-voice pressure, AT to VCO/VCF/speed | Sample/hold acquisition and droop, TSB detector and FET states, force-sensor law, key contact timing | INITIAL_TOUCH_CIRCUIT_EVIDENCE.md, INITIAL_PITCH_TRANSIENT.md, SHARED_PRESSURE_SPEED.md |
| SUB oscillator and EXT | `OriginalSubOscillator.h`, `ExternalModulationInterpolator.h` | Six original functions, 0.7-60 Hz, optional mono EXT bus (modulation only) | IG00150 exponential converter, true waveform shapes/noise spectrum, EXT physical voltage and headroom | ORIGINAL_SUB_OSCILLATOR.md, EXTERNAL_MODULATION.md, IG00150_CONTROL_EVIDENCE.md |
| Ring modulator, ribbon | `Engine.cpp` | Frequency-envelope routing, calibration anchors 150 ms / 5 ms, relative ribbon (~1 octave up) | Excursion law (`kProvisionalRingFrequencyExcursion`), multiplier non-idealities, ribbon voltage curve | FIDELITY_AUDIT.md |
| Keyboard contours, footswitch, EXP/WAH, output switch | `PerformanceControls.h` | Four contour levers, assigner truth table with tail/glide edges, EXP vs EXP-WAH stand-in, HIGH/LOW | Everything numerical (see register, section 5); KAS/KBC key map; PRA transfer; LOW level | PERFORMANCE_CONTROLS.md, FOOT_CONTROL_EVIDENCE.md, KEYBOARD_CONTOUR_EVIDENCE.md |
| Tone selectors and memories | `HardwareControl.h`, `FactoryPresets.cpp`, plugin layer | 14 positions per rank, 4 memory banks (104 host parameters), PANEL, persistence and automation; 547 transcribed resistor-matrix cells in an offline solver | 25 cells unreadable/revision-dependent, loaded solve (BA driver, receivers), mapping into IC laws; **all 22 factory tone voicings are placeholders** | PRESET_*.md, MEMORY_AUTOMATION.md |
| Chorus/tremolo, output stage | `Dsp.h` `Chorus`, `Engine.cpp` | Modulated delay chorus, amplitude tremolo, 10 Hz DC block, soft clip, 255-tap decimator (63 samples latency) | The original BBD/phase network, coupling poles, headroom, noise | FIDELITY_AUDIT.md, DSP_MEASUREMENTS.md |
| Voice variation | `Engine.cpp` | Per-voice tuning and pan scatter, slow drift, noise floor | Calibrated distributions; cutoff/level/envelope trims are generated but never read | ENGINE.md, section 6 |
| Host layer | `src/plugin/` | VST3, AU, Standalone; 244 append-only host parameters; state migration; UI; pluginval 10 | n/a (software quality) | MEMORY_AUTOMATION.md, VALIDATOR_DIAGNOSTICS.md |

**Examples of offline reference models that do not yet drive audio** (integration requires explicit assumptions and tests, not hardware access): `src/core/HardwareControl.h` (passive preset matrix), `src/reference/IG00156Linear.h` (conditional filter graph), `src/reference/KeyboardContour.h` (settled-DC KBC network). Their tests establish arithmetic consistency with transcribed or explicitly supplied model boundaries, not measured hardware agreement. Additional offline loaded-control, resonance, level, PRA, OE2, MN3001 and output references are described below; their existence does not mean they already drive production audio.

---

## 4. Workstreams

Each lists what to do, where to start, and the acceptance gate. Order is in section 8. "Gate" means the evidence and tests required for the stated model milestone; hardware-calibrated claims require actual calibration, but engineering-inferred implementations do not.

### W0. Parity mode and the comparison harness (start here)

Implementation checkpoint: [PARITY_ISOLATION.md](PARITY_ISOLATION.md) describes the opt-in isolation contract and explicit numerical/controller-transport exceptions. [PARITY_HARNESS.md](PARITY_HARNESS.md) describes the synthetic comparison and mutation tests. The optional `live-adr-v1` candidate additionally records applied A/D/R transactions and state probes; its tests validate the current software law only. Hardware-comparison claims await calibrated references and separately preregistered analog tolerances; implementation and model-based validation continue without them. Consult exact-commit CI for verification status.

Isolate original features and compare against declared source-derived or engineering-model references; add hardware references if available.

- Add a **parity mode** that holds the instrument to the original feature set: no modern FX slots, macros, LFOs 1-3, MPE, arpeggiator, filter bypass switches, rank pan, tuning reference, output mono fold, legacy SUB mode, 500 Hz ring extension, MIDI CC64 sustain, KEY RELEASE; voice dispersion, drift and noise set from source-informed, explicitly inferred physical ranges (W11), or calibrated data if available, rather than arbitrary defaults. List of divergences: section 6.
- Build the harness: stimulus generator with fixed control settings, a reference-corpus manifest (instrument identity, board/IC revisions, rails, temperature, trims, probe, scaling, sample rate, settings, SHA-256, redistribution status), alignment by latency, and metric reports per stage. `src/tools/dsp_probe.cpp` and `scripts/analyze_dsp.py` are the existing pattern (deterministic CSV/JSON/plots from an exact-commit CI build).
- **Gate:** the harness runs end to end against a synthetic stand-in reference in CI, and tolerances live in a versioned file committed *before* any real reference is compared.

### W1. Voltage-domain calibration layer (document-derivable)

Offline checkpoint: [LOADED_CONTROL_EVIDENCE.md](LOADED_CONTROL_EVIDENCE.md) now records reviewed R39/M41/M44–47 wiring and conditional single-pair/eight-voice cutoff-node models. They retain finite-source loading and inter-voice common-bus coupling; a conditional transient model adds the documented node capacitors with explicit initial states and fixed topology. They require explicit source impedance and receiver-current boundaries; they do not drive production audio or establish the IC voltage convention. The [resonance reference](RESONANCE_CONTROL_REFERENCE_DESIGN.md) separately solves 32 individual VQ nodes across both ranks with a shared loaded RES source. It retains explicit affine chip-current boundaries and does not supply a VQ-to-Q law. The [level reference](LEVEL_CONTROL_REFERENCE_DESIGN.md) separately solves the 16 LC nodes with 43 supplied source ports, preserving shared AM/KB loading. Its boundary ends at IC5 LI2, not calibrated loudness; upstream multiport and IC gain laws remain open.

The model still maps normalized panel positions to behavior. The original is a network of resistor-summed control voltages.

- Keep key voltage `Kv`, effective cutoff CV and Q CV as explicit quantities separate from slider position (the first step exists: `filterKeyVolts`, `nominalFilterCutoff`, `filterEffectiveCv`).
- Solve the loaded summing networks: R51/R52 (service p39) into FH (HP) and FL (LP), the LC level bus, and the M41 control nodes with rank-selected shunts (R6/R7: 2.7/2.2/1.8 kOhm HP, 3.3/2.7/2.2 kOhm LP), 0.01 uF node capacitors and trims. HP and LP differ (120 kOhm vs 39 kOhm branches); equal knob values do not give equal octave excursions. Replace the equal-HP/LP assumption in the brilliance paths.
- Replace provisional "octave per unit" scales with solved volts at the IC pin, then the documented IC law.
- **Gate:** documented anchors reproduced (0.25 V/5 V gives 1 kHz; 0.5 V/5 V gives 2 kHz; 0.25 V/6 V gives 2 kHz; Q 5 at 0 V and 0.5 at 10 V); service setup points reproduced (ADJ p11: FEET 2', C1, resonance max, FH = 4.0 V gives 4.3 +/- 1 Vpp at CP3; LP FL = 3.3 V gives 2.2 +/- 1 Vpp).

### W2. Key assigner, contour key map, sample/hold (source-derived with explicit inference where unresolved)

A [constraint review](KEYBOARD_MAPPING_CONSTRAINT_REVIEW.md) rules out fixed bit permutation/XOR and pure uniform note-label shifts under the current detailed-network premises. It also identifies OC0/OC1 settled-DC non-identifiability. These exclusions do not supply a 61-key mapping or justify a production remap.

- **Resolve the KAS/KBC physical-key map**: B1/B2/B3 codes through the latched Q states to the YM26700 octave-tap selection, for all 61 keys. KEYBOARD_CONTOUR_EVIDENCE.md lists exactly which pages and which contradiction remains (the naive mapping disagrees with the C2/C5# calibration conditions). Preserve the contradiction; if public evidence cannot resolve it, document a constrained engineering mapping and its alternatives instead of waiting for hardware. Do not present that choice as a verified decoder. Then drive audio from `src/reference/KeyboardContour.h` and delete the provisional weights in `PerformanceControls.h`.
- Model the per-voice sample/hold (the page-7 hold buffer switched by NAND(voice TR, SUS) with a 22 kOhm SUS pull-up, see INITIAL_TOUCH_CIRCUIT_EVIDENCE.md; and the 0.033 uF held node on each KBC channel), including droop and settling, plus scan timing and allocation priority and whether a still-held ninth key is reconsidered when a voice frees.
- **Gate:** all 61 keys x lever grid agree with the declared network/mapping; evaluate the printed O1 -2.76 +/- 0.14 V and +2.76 +/- 0.14 V calibration anchors at C2 and C5# on both KBC boards. Require agreement where the selected source interpretation permits it; otherwise publish the conflict, model choice and residual rather than inventing a match. Scan and hold behavior implemented with stated timing/droop assumptions, sensitivity tests and allocation regressions; measurements may refine it later. Conflicting key-map anchors must remain visible rather than being falsely claimed as simultaneous matches.

### W3. VCO and wave shaper (IG00153, IG00158)

- Replace the analytic PolyBLEP source with a model of the verified M41/IC42 graph: current-source ramp, reset one-shot, comparator with clamp references, triangle-to-sine diode shaper, per-wave voltage gains (saw 3.5 Vpp, sine 3 Vpp, triangle 3.2 Vpp are *pin-level* targets, not final output levels).
- Decide, from the circuit, whether oscillators free-run between notes and what the filters hold; [active-tail continuity](ACTIVE_TAIL_CONTINUITY.md) now preserves phase/filter state on still-active unheld reassignment in opt-in isolation mode. Default-off and inactive assignment retain their prior reset policy. Full idle evolution and held key-voltage behavior remain unresolved.
- Model reset shape, thresholds, spread and drift from the documented graph plus declared device assumptions. Optional hardware refinement: capture DC-coupled waveforms at calibrated pitch and pulse width to fit reset shape, thresholds, device spread; measure Hz/V tuning curvature and temperature drift.
- **Gate:** agreement with public pin-level anchors and independent model predictions per pitch and width, state-continuity and sensitivity regressions; report capture residuals if available; alias level reported separately from analog response error (see DSP_MEASUREMENTS.md for the method and its limits; the 2x internal rate does not remove aliases folded in the oscillator).

### W4. VCF (IG00156): the highest-impact sonic item

- The **functional graph is recovered** (three amplifiers, two variable-gm blocks with the cutoff control, capacitor feedback, and a Q-controlled block labelled only "Variable"; the original guidebook page identity is confirmed) and is implemented as the offline conditional model in `src/reference/IG00156Linear.h` (`DynamicModel`, `AlgebraicModel`). It is *not* a silicon model: the Q port is a supplied conductance `Iq = -gq * BP` with **no sourced VQ-to-gq curve**, gm1/gm2 are supplied, there is no nonlinearity, saturation, noise or temperature law, and the fixture's `Rs`/`Rg` are placeholders for the real CS-80 input network (HP and LP differ). These missing quantities require explicit engineering models before audio integration: the Q block's law, gm versus control current, amplifier finite effects, and the actual input and ground networks on the M card. Hardware access is not required to implement and test those models.
- Promote the reference model into production only after it is verified against the discrete TPT solver in small signal, then add nonlinear terms justified by public device/circuit knowledge or explicitly declared engineering inference, with sensitivity and amplitude-dependent regressions. Keep the 4.7 MOhm paths as feedforward (not leakage).
- Validate modeled complex HP/BP/LP responses over cutoff and Q; the same at several amplitudes (amplitude-dependent Q indicates feedback dynamics rather than input clipping); harmonic and intermodulation tests; slew and saturation recovery; noise; the serial HP-to-LP pair, not isolated filters; grades A/B/C. The plan is in IG00156_MODEL_EVIDENCE.md "Measurement and discrimination plan"; use that plan for optional hardware refinement. Until then, implement constrained Q/nonlinearity hypotheses and label them as inferred. Keep any safety limiter external and labelled; justify any nonlinear stage model rather than inserting unexplained `tanh` blocks.
- **Gate:** complex response error across the control grid within pre-registered tolerance; nonlinear behavior checked against documented anchors and declared physical hypotheses across held-out settings; hardware residuals only when captures exist.

### W5. Envelopes (IG00152 filter EG, IG00159 VCA EG) and VCA (IG00151)

The [target-transition source review](ENVELOPE_TARGET_TRANSITION_REVIEW.md) distinguishes LC target drive to +IN3 from buffered feedback to −IN4 and the external 0.22µF integrating node. It does not resolve the opaque sequence thresholds or charging-current law, so live sustain/IL/AL trajectories are currently uncalibrated and unimplemented. Next implement a documented, state-continuous engineering transition model consistent with the known topology; unavailable hardware must not postpone that work.

- Derive source-supported timing laws and explicitly model the unknown portions and sustain/release routing from the IC pages (higher timing control voltage shortens a stage; global SUS selects the lower control voltage) and the verified 1 ms-1 s / 10 ms-10 s ranges, instead of the `coefficientForTime` -60 dB heuristic and x5 "bend".
- VCA: separate exponential and linear control inputs, offsets, bleed.
- Test modeled voltage and acoustic envelopes at several settings, interrupted attacks, retriggers, releases from each stage.
- **Gate:** envelope curves agree with declared source/model references within pre-registered tolerance for all stages, including stolen and retriggered tails.

### W6. Touch: initial touch, aftertouch, pitch transient (TKC/TSB/TWS/TRG)

- Keep the established separation: **b** (sampled/held, feeds the brilliance/level banks) versus **C** (AC-coupled, feeds pitch bend). Derive the switched FET/diode sequence, trigger polarity and acquisition window; replace "velocity as a number" with the electrical acquisition. Calibrate the TRG5 diode-OR pressure bus and the pressure-to-speed/VCO/VCF paths.
- Model per-key sensor response and key-contact timing from public information and declared engineering assumptions, refining from measurements if available (MIDI velocity is only a proxy).
- The implementation gate list is in INITIAL_TOUCH_CIRCUIT_EVIDENCE.md ("Implementation gate"); retain its source checks and retrigger/staggered-polyphony regressions. Any older hardware-only promotion restriction is superseded by this requirement; document inferred acquisition/response laws instead.

### W7. Modulation: SUB (IG00150), ring, PRA, EXP/WAH, chorus/tremolo

- IG00150 exponential speed converter (including the touch-to-speed currents), calibrated against the documented 200 Hz point.
- Ring: carrier law, envelope-to-frequency transfer (replace `kProvisionalRingFrequencyExcursion`), multiplier DC and feedthrough, calibrated to the ADJ p19 anchors and extended across speed x depth.
- **PRA wah**: the [conditional small-signal reference](PRA_WAH_REFERENCE_DESIGN.md) derives the ideal-op-amp network up to IC2 pin13 before FET2. Optical resistance, an unresolved parallel resistor and omitted source/load boundaries remain explicit caller inputs; it does not drive audio. Next, solve the complete IC1/IC2 active RC network with source-informed optical resistance/dynamics models (measured resistances if available) (pedal travel to EI, EI to resistance with dynamics, WI to WO response, dry/processed insertion level) and retain the verified ring-blend → wah → OE1/OE2 → optical-expression → volume order ([routing evidence](OE1_SIGNAL_ROUTING_EVIDENCE.md)). Replaces the swept band-pass stand-in.
- Chorus/tremolo: [OE2 evidence](OE2_CIRCUIT_EVIDENCE.md) now identifies the device and branch graph; [MN3001 clock-event reference](MN3001_CLOCK_EVENT_DESIGN.md) implements only an explicitly normalized packet-timing abstraction, with no pin/charge calibration or production integration. A separately reviewed [OE2 filter reference](OE2_FILTER_REFERENCE_DESIGN.md) solves only the ideal-amplifier input/reconstruction RC networks and their AC-coupled unequal gains, with explicit source/load boundaries. Actual phase/amplitude control, bias, charge transfer, clock law and complete OE1/OE2 response remain open; the current audible chorus is still a generic modulated delay.
- Footswitch edge transients and the real release-cancellation behavior.

### W8. Output stage, supply, headphone

The [output source review and conditional AC reference](OUTPUT_LEVEL_SOURCE_REVIEW.md) now separates the three HIGH/LOW tap selectors from the upstream headphone path. It solves only an explicit passive fixture with supplied disputed resistor values and loads. Source discrepancies and active-stage/load calibration remain unresolved; the production 0.1 LOW scalar is not replaced.

HIGH/LOW levels (currently an arbitrary -20 dB), output coupling poles and headroom (currently a 10 Hz DC block and `softClip`), left/right/general routing, headphone amplifier, noise. Rail/ripple/bias effects only as far as they are audible and with source-derived or explicitly inferred physical models and bounded sensitivity; never imply software reproduces mains safety.

### W9. Presets, programmer and memories (document-derivable until the final comparison)

Source checkpoint: [PRESET_RECEIVER_PORTS.md](PRESET_RECEIVER_PORTS.md) records all 26 rank-I first-hop destinations with reviewed native evidence. The [rank-II receiver ledger](RANK_II_RECEIVER_PORTS.md) now records all 26 first hops, individually traced M21–M28 harness connections and R52 destinations. Full intermediary BA/SUB/TRG behavior, conflicting memory endpoints, passive Q/level solves and quantitative receiver-current laws remain open.

The 22 factory tone voicings are placeholders; the preset circuits are the source.

1. Close the transcription: 547 of 572 cells are classified; seek resolution of the 25 revision/readability exclusions (PRESET_FULL_MATRIX_AUDIT.md, PRESET_PCB_UPDATE_REVIEW.md) from native-resolution crops. If still unreadable, retain them as unknown source cells and separately declare justified model estimates/ranges; never relabel estimates as transcriptions or let missing hardware block modeled presets.
2. Add the missing layers the offline solver deliberately omits: the actual BA column driver and its source impedance, nonlinear diode I-V, alternate memory/PANEL sources, and the 26-output **receiver table** (connector destination, fan-out to eight voices, reference potentials, gain, capacitors). Formulation: nodal KCL with signed receiver currents (PRESET_COMPLETENESS_AUDIT.md).
3. Carry volts through the real networks into the IC laws from W1/W5, derive all 22 electrical states, and **only then** let `HardwareControl.h` drive `FactoryPresets.cpp`. Memory banks and PANEL must be traced for loading and switch-state interaction.
4. **Gate:** source anchors, loading/switch-state tests, uncertainty sensitivity and perceptual regressions support the modeled replacement. Label unresolved cells and inferred IC laws. Authorized recordings can inform comparison, and calibrated captures can refine it later; neither is a prerequisite to replacing placeholder voicings. Keep the 200 modern performance patches in a clearly separate bank.

### W10. Interaction, state and edge cases

The [live timing correction](LIVE_ENVELOPE_TIMING.md) refreshes active A/D/R controls in isolation mode without resetting voice or envelope state. It retains existing provisional timing laws and deliberately leaves sustain-level/IL/AL target transitions unresolved. This is a bounded routing fix, not W5/W10 completion.

Every combination the real instrument can be put in: both ranks, all memories, every selector, full key range, pressure, ribbon, pedal and sustain interactions, interrupted and repeated gestures, controls changed while notes sound or release. Add these as regression traces alongside W1-W6 implementation and integration (many currently pass only because both sides share one provisional law).

### W11. Voice variation, tolerance and aging

Replace arbitrary random trims with source-informed distributions (and correlations) of component/card tolerances and thermal time constants; label engineering-inferred ranges and test sensitivity. Use measured distributions for an identified unit if available, not as a prerequisite. Wire the unused cutoff, level and envelope trims through justified circuit-related parameters with neutral-default compatibility and regression tests.

### W12. Numerical and platform quality (keep green throughout)

The [keyboard handoff](KEYBOARD_AUDIO_HANDOFF.md) removes the identified shared MidiKeyboardState lock and host-buffer injection from the callback using a bounded queue and explicit lifecycle/overflow policy. This is a scoped boundary correction, not proof of whole-plugin allocation freedom, hard realtime or race freedom. The [schema correction](STATE_SCHEMA_VALIDATION.md) removes direct live-state type reads during competing restores.

Oversampling and aliasing at higher rates, CPU cost of the nodal models, denormals and determinism, sanitizer and ThreadSanitizer coverage of host and program metadata paths, AAX (SDK not redistributable), and DAW exercise before release.

---

## 5. Register of provisional values

Every numerical law needs a source or an explicit engineering-assumption row. Current undocumented values are placeholders; a reviewed inferred model is not a measured hardware law. These are the ones that shape the sound; search for `kProvisional`, "provisional" and bare literals in `Engine.cpp`/`Dsp.h` for the rest. **Do not use unstructured tuning by ear as proof of fidelity.** Controlled perceptual comparisons may inform a documented model choice alongside circuit and numerical evidence.

| Value | Where | Now | Resolved by |
|---|---|---|---|
| Sine shaper third harmonic | `Oscillator::tick` | 3.5% | W3 model / optional measurement |
| Reset pulse | `Oscillator::tick` | 30 us, weight 0.55 x 0.70 | W3 |
| Filter pre-saturation | `Svf::process` | `softClip(1.1x)` | W4 model / optional measurement |
| Q law and rank/global mix | `provisionalFilterQ`, `Engine.cpp` | `1/Q = 2 - 1.8r`; `r = 0.8 rank + 0.6 global` | W1/W4 |
| Filter envelope scale | `kProvisionalFilterEnvelopeOctaves` | 3 octaves | W1/W5 |
| HP envelope gain | `kProvisionalHpfEnvelopeGain` | 0.5 | W1 (100 kOhm vs 47 kOhm branches) |
| Envelope shape | `coefficientForTime`, `Adsr`, `FilterEnv` | -60 dB time-to-rest, x5 early "bend" | W5 |
| Global SUSTAIN law | `Engine::releaseVoice` | linear, 10 s maximum | W5 |
| Brilliance paths | `Engine.cpp` (`brillOct`) | initial/after-touch 2 oct, timbre 0.8, wheel 1.5, SUB 1.5 oct | W1/W6 |
| Level path | `Engine.cpp` (`levelCv`) | SUB 0.45, clamp 2.6, overall `* 0.17` | W1/W5 |
| Pitch modulation scales | `Engine.cpp` | SUB-VCO 1.5 st, wheel-SUB 2 st | W7 |
| Initial pitch transient | `kNominalInitialPitchDecaySeconds` | 50 ms nominal pole | W6 |
| Ring excursion | `kProvisionalRingFrequencyExcursion` | 1 octave at full depth | W7 |
| Footage ratios | `kFeetRatios` | nominal harmonic | trims/tolerances in W3 |
| Keyboard contour | `PerformanceControls.h` | 2.0 oct, 0.4 level, smooth weights, 61-key span MIDI 24..84 | W2 |
| EXP-WAH | `PerformanceControls.h` | 350 Hz + 2.9 oct, Q 4.5, x1.6 | W7 |
| LOW output | `kProvisionalLowOutputGain` | 0.1 (-20 dB) | W8 |
| Voice tuning scatter | `kProvisionalVoiceTuneSpreadCents` | 7 cents x dispersion | W11 |
| Drift | `Engine.cpp` | sine, 0.9 semitone x amount (about +/-22 cents at the 0.25 default), 0.031 Hz | W11 |
| Chorus, delays, FX | `Chorus`, `Phaser`, `Reverb` | generic | W7 / out of scope (modern FX) |
| Output | `Engine.cpp` | 10 Hz DC block, `softClip(x * volume * 4 * expression)` | W8 |
| Smoothing | `Engine.cpp` | 10 ms SUB rate, 12 ms wah, 5 ms output | engineering choice; label as such |
| Factory voicings | `FactoryPresets.cpp` | 22 tone studies | W9 |

---

## 6. Things in the product that the original does not have

These are intentional software extensions. They are fine for a modern instrument and must be inert in parity mode (W0). Each is documented where it was added.

MIDI CC64 sustain (per channel, MPE master rule) and CC11/CC74 mapping; MPE with separate bend ranges; arpeggiator; three LFOs; four macros; three modern FX slots (chorus, phaser, tape delay, plate reverb, drive); 200 original performance patches; legacy SUB waveform/rate mode kept for old sessions; ring speed above the documented 200 Hz endpoint (500 Hz); KEY RELEASE; per-rank pan and filter bypass switches (VOICE SETTINGS); master tuning reference and pitch-bend range; mod wheel and pitch wheel; stereo/mono output mode; the 2x oversampling and 255-tap decimator (63 samples of reported latency); output DC blocker and soft clip; latching virtual footswitch; on-screen key-pressure drag gesture; the optional mono EXT bus (modulation only, no audio-through).

---

## 7. Verification method

1. **Layered references.** Each stage has its own comparison against its own reference (source-derived or explicitly inferred independent model, with optional measured refinement), before any complete-path comparison. This localizes error to oscillator, filter, envelope, modulation or output stage.
2. **Pre-registered tolerances**, stated against reference uncertainty, in a committed file.
3. **Hold-out.** Fit on some control settings, validate on others. Do not tune and evaluate on the same small preset sample.
4. **Numerical convergence independent of hardware.** Compare the discrete solver with an offline high-accuracy integration of the *same* equations; check smaller steps and several sample rates; report aliasing separately from analog error.
5. **Provenance.** Exact-commit CI builds produce the artifacts; local runs are not authoritative. Record SHA-256 of every source and capture.
6. **Blind listening last**, level-matched and latency-aligned.

---

## 8. Suggested order and milestones

| Milestone | Content and acceptance | Needs hardware? |
|---|---|---|
| **M0** Harness | W0 isolation, model/capture provenance manifests, stand-in references, pre-registered model tolerances and mutation tests | No |
| **M1** Public-evidence and assumption baseline | Inventory public sources/anchors, conflicts, unknowns, engineering hypotheses and uncertainty ranges; define model validation stimuli. Keep a separate optional hardware capture plan | No |
| **M2** Control domain | W1, W2, W9 steps 1-2: loaded networks, declared key map and receiver models; test and integrate with explicit uncertainty | No |
| **M3** Core signal path | W3-W6 implemented and validated stage by stage against public anchors and independently evaluated models; sensitivity, convergence and perceptual regressions | No |
| **M4** Modulation and output | W7, W8 and circuit/model-driven presets (W9 step 3); replace stand-ins through reviewed integration, not a hardware gate | No |
| **M5** Variation and interaction | W10, W11: inferred physical variation ranges, complete gesture/state regressions and optional measured refinement | No |
| **M6** Public-knowledge reconstruction sign-off | Complete-path tests, declared pass criteria, listening regressions, feature coverage and published limitations. Report exactly which behavior is source-derived or inferred, not unqualified verified hardware parity | No |
| **Optional hardware validation** | If a suitable unit/corpus becomes available, document calibration, measure residuals, refine hypotheses and state agreement only for that unit, settings and tolerances | Only for this optional measured claim |

M1 is a living evidence/assumption baseline, not a wait for a reference instrument. Workstreams can progress in parallel as their model interfaces and test fixtures are defined. Missing hardware does not block any development milestone; missing implementations and failing software/model tests still require work.

---

## 9. Working rules (established by earlier sessions; keep them)

- **Append-only host parameters.** Never reuse or reorder an ID; give new parameters a later AU version hint; old sessions must restore neutral values. The smoke test pins literal snapshots of the shipped IDs and hints.
- **Neutral defaults are bit-identical.** New behavior must leave existing patches sample-identical until deliberately engaged (compare raw float streams against the previous commit).
- **Name unmeasured numbers.** `kProvisional...` constants and comments that say "provisional"; never describe a heuristic as measured hardware behavior. Promotion requires a source row or explicit reviewed engineering-assumption row, uncertainty and tests—not a hardware measurement.
- **Intentional audible changes are documented**, with the exact commit to use for historical renders.
- **Record evidence**: source URL, source-byte SHA-256, crop box, crop SHA-256, units, independent-review status. Mark unreadable marks unknown and request a native crop; do not substitute a neighbor's value or an OCR guess as a transcription. If unavailable, keep the source value unknown and declare a separate model assumption/range so implementation can continue.
- **Tests tiers**: unit and numerical suites (core, performance controls, reference experiments), plugin/UI smoke test, pluginval strictness 10, sanitizers. Mutation-check new tests (break the behavior, confirm failure).
- **Do not silence a conflict.** Where two sources disagree (for example KAS04 vs KAS05 connector 59/57 labels, 10 kOhm schematic vs 50 kOhm manual for EXT), preserve both and say so.
- **No completion claims** without section 1's evidence.

---

## 10. Tooling and environment

Build and test (Linux):

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release   # fetches JUCE 8.0.6 unless deps/juce exists
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Core-only (no JUCE) for fast engine work: add `-DCS80_BUILD_PLUGIN=OFF`. The UI smoke test needs `JUCE_FONT_PATH` pointing at a directory containing DejaVu Sans on a bare container (CI sets it). Sanitizers: configure with `-fsanitize=address,undefined` (leak detection was off in this container; no leak-clean claim exists).

Reading the original service scans: `.github/workflows/circuit-inspection.yml` and `scripts/inspect_circuit_sources.py` download a named page (the `SOURCES` map names 62 pages: the M card and wiring, KAS/KBC, T51-T54 and their wiring, TKC/TSB/TWS/TRG/SH, SUB, PRA, OE1, BA, panels 1/2/3/5, MKB and tone selector, R51-52, the M-circuit IC sheets, the IC guidebook, the adjustment procedures, and several servicing-guide pages) inside GitHub Actions, render native-resolution crops and print them as numbered base64 chunks in the job log (concatenate each marker's chunks in order, no resizing). Nothing downloaded is committed. To add a page, add it to `SOURCES`, push, and read the job log. If you have direct access to the sources you do not need this, but keep the hash-and-crop provenance convention.

Other workflows: `build.yml` (3-platform build, tests, pluginval 10 including GUI under Xvfb and AU GUI on macOS), `screenshots.yml` (refreshes `docs/screenshots` on `main` only when the source commit is current), `dsp-measurements.yml`, `ci-evidence.yml` (publishes run outcomes as commit statuses).

---

## 11. Known open issues and questions

- Default ANALOG DRIFT is large (see section 5) and arbitrary; decide whether it stays a creative control outside parity mode.
- Three of five voice trims are generated but unused.
- In isolation mode, active unheld tail reassignment preserves oscillator phase and filter state. Full idle free-running and held key-voltage evolution remain unresolved (W3, W2); default-off and inactive assignment retain the previous reset policy.
- Whether the original continuously rescans and reconsiders a still-held ninth key is unresolved (W2).
- The KAS-to-KBC code mapping contradicts the CS-80 calibration keys under the naive reading (KEYBOARD_CONTOUR_EVIDENCE.md).
- PRA placement is now source-verified in [OE1_SIGNAL_ROUTING_EVIDENCE.md](OE1_SIGNAL_ROUTING_EVIDENCE.md); detailed OE2 transfer and optical laws remain open. [OE2_CIRCUIT_EVIDENCE.md](OE2_CIRCUIT_EVIDENCE.md) resolves I(53), records the four parallel VCA returns and verifies the manufacturer dual 512-stage identity; installed clock/modulation calibration is still missing.
- The quantitative Q-block law inside IG00156 is not in any inspected source (see W4).
- Filter-IC component grades A/B/C are unrelated to synthesis rank I/II; PRESET_COMPLETENESS_AUDIT.md warns about the shared "rank" wording.
- Historical docs record audit baselines at older commits (for example "233 host parameters"); they are dated evidence, not current counts.
