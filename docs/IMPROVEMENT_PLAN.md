# CS-80 Reference — improvement plan: original-CS-80 fidelity beyond Arturia CS-80 V4

## Context

**Goal.** Make every sound, feature, function and the UI/UX of this plugin match the
original Yamaha CS-80 as far as public evidence and sound engineering allow, and make
the result better than Arturia CS-80 V4.

**Where the project stands.** It is already an unusually rigorous reconstruction:
- 8 voices × 2 ranks.
- 22 source-model tone tabs compiled from the service-manual resistor matrix.
- Circuit-derived models for the PRA wah, the OE2 BBD chorus and the loaded output.
- 245 append-only host parameters, about 90 evidence docs and 4-platform CI with pluginval.

**What holds the sound back.** Exploration found that the parts that define the CS-80's
sound are still generic or placeholder:
- **Filter.** `Svf` in `src/core/Dsp.h:206-252` is a linear TPT SVF. Its only nonlinearity
  is `softClip(1.1x)` on the input, outside the resonance loop (`:240`). That clip is also
  the main source of aliasing: saw non-harmonics measure −34 to −42 dB (`docs/ENGINE_AUDIT.md:123`).
- **Oscillator.** PolyBLEP, with a placeholder 3.5 % third harmonic standing in for the sine
  shaper and a fixed 30 µs reset pulse (`Dsp.h:143-203`). The circuit-faithful
  `src/experimental/ReconstructedOscillator.h` is not active.
- **Envelopes.**
  - `exp(-6/…)` time law, which is −52 dB, not the −60 dB its comment claims.
  - A ×5 "bend" heuristic applied in fixed time windows.
  - The filter EG is reset unconditionally even in source mode (`Engine.cpp:796`).
- **VCA.** A linear multiply.
- **Smoothing and variation.**
  - No parameter smoothing, so block-rate zipper noise on every control.
  - Drift is a single 0.031 Hz sine of ±22 cents.
  - 3 of the 5 voice trims are generated but never read.
- **Voice continuity.** Voices freeze when idle and reset on reuse;
  `src/experimental/ContinuousVoiceCircuit.h` exists but is not wired in.
- **Engine default.** The circuit engine ("Source Performance") is **off by default**.
  All 222 library programs force the Legacy engine. Most users therefore never hear the
  circuit work.
- **Gain staging.** Source tabs reach +3.9 dBFS with no defined reference level.
- **CPU.** About 13 transcendental calls per rank per sample (`tan`/`exp` recomputed every
  sample) and a fixed 2× oversampling. Source mode costs about 0.27 RTF for 8 voices and has
  deadline misses; idle costs 6–7 %.

**Decisions taken with the owner:**
- **Evidence:** public sources only. No hardware capture phase; declared engineering
  inference, as the current policy allows.
- **Engine:** the circuit engine becomes the default. Legacy stays only so old sessions
  load unchanged.
- **UI:** original panel first. Modern extras move into drawers or a second view.
- **Modern layer:** match and exceed Arturia (multi-segment envelopes, mod matrix,
  microtuning, MIDI learn, undo, better FX), all inert in "Pure CS-80" mode.

**How we beat Arturia.** Arturia publishes no circuit-level evidence, so "better" has to be
demonstrable rather than claimed:
1. **Traceable engine.** Every law is traceable to a schematic page or a declared inference.
2. **Circuit behaviors modeled explicitly.** True free-running voices, the KAS
   scan/sample-hold, and the PRA, OE1/OE2 and output networks.
3. **In-app evidence view.** It shows which behavior is source-derived and which is
   inferred.
4. **Offline quality.** Offline renders run at a higher quality than realtime.
5. **"Pure CS-80" switch.** It strips every non-original feature.
6. **Modern layer at least equal to Arturia V4's.** That means multi-segment envelopes
   (MSEGs), a mod matrix, MTS-ESP microtuning, MPE and FX, plus ergonomics: undo, A/B,
   MIDI learn and typed values.

Arturia V4's feature list comes from search results quoting its product page, retailer
listings and the 4.0.1 manual. arturia.com itself was not reachable from this session.

---

## Guiding architecture: truth model vs realtime model

There is no hardware to measure. The reference for each stage is therefore an **offline
"truth model"**: double precision, high oversampling, implicit or Newton solves, built
directly from the schematic equations. The realtime model is then held to that truth model
by pre-registered tolerances.

The pattern already exists in `src/reference/*`; for example `IG00156Linear.h` provides a
`DynamicModel` with trapezoid plus Gaussian elimination. Extend it to every stage, and add
a `cs80_truth_render` tool beside `src/tools/parity_capture.cpp` that renders whole notes
through truth models. `scripts/parity_harness.py` and `tests/parity/tolerances-v1.json`
become real stage gates instead of synthetic-only checks.

Public recordings are used **only** for blind, level-matched listening reviews. They are
never treated as calibration data, as `docs/ANALOG_FIDELITY_REQUIREMENT.md` already requires.

---

## Phase 0 — Foundations (no intended sound change; legacy byte-identical gate stays green)

1. **Engine restructure for speed and smoothing** (`src/core/Engine.cpp`, `Engine.h`, `Dsp.h`)
   - Split the 420-line `processInternalSample` (`Engine.cpp:1141-1559`) into
     `tickGlobals` / `tickRank` / `mixBus` / `tickOutput`.
   - Cache filter and envelope coefficients and recompute them only when their inputs move.
   - Use fast `exp2`/`tan` approximations with bounded error, tested against `std::`.
   - Store the 16 rank chains structure-of-arrays so the compiler can vectorize across ranks.
   - Remove the dead `stillActive` (`:1366-1369`).
   - Targets: source mode ≤ 0.15 RTF for 8 voices at 48 kHz/2×, and idle ≤ 2 %.
     Measure with `src/tools/source_factory_benchmark.cpp`.
2. **Parameter smoothing** (new `src/core/Smoothing.h`)
   - Sample-rate-independent one-pole smoothers for every continuous panel parameter and
     controller: pressure, bend, CC1/11/74, cutoff, Q, levels, PW, pan and FX mix.
   - Smoothers start at their target, so static renders stay bit-identical.
   - Replace the sample-rate-dependent 0.003 and 0.0015 per-sample constants
     (`Engine.cpp:1195, 1521`).
3. **State schema version** (`src/plugin/PluginProcessor.cpp:699-807`)
   - Write `stateSchemaVersion` into saved state.
   - A missing version means pre-plan semantics, so old sessions keep Legacy.
   - This unlocks the engine default flip in Phase 1.
4. **Test framework**
   - Keep the existing `check()` executables.
   - Add a shared `tests/TestSupport.h` with spectral, alias and envelope-fit helpers so the
     new stage gates are short.
   - Register the new stage gates in `CMakeLists.txt` the way the existing `add_test`
     entries are registered.
5. **Single status source of truth**
   - Add `docs/STATUS.md`: the current spec, a model register and an open-issue list.
   - Move historical evidence into `docs/evidence/` and CI incident logs into
     `docs/history/`, leaving link stubs.
   - The README links only STATUS, the roadmap and the coverage ledger. This removes the
     README vs `PARITY_ROADMAP.md:7-13` status drift.

## Phase 1 — The voice: filter, oscillator, envelopes, VCA, gain staging (largest audible gain)

1. **IG00156 realtime nonlinear filter** (new `src/core/Ig00156Filter.h`, replacing `Svf`
   in the circuit engine only)
   - **Topology:** promote the recovered `IG00156Linear::DynamicModel` graph
     (`src/reference/IG00156Linear.h:13-27, 163-186`): 1.2 MΩ/180 pF feedback, 4.7 MΩ
     feedforward and 1.5 nF integrators. HP→LP run in series through the 1 µF coupling.
   - **Nonlinearity (declared inference):** put the transconductor nonlinearity *inside* the
     loop, as a tanh of the differential input per gm block. This gives amplitude-dependent
     resonance and self-oscillation limiting.
   - **Q:** declare a hypothesis for the Q-block (VQ→gq) law that meets the documented
     endpoints (Q 5 at 0 V, 0.5 at 10 V).
   - **Solver:** semi-implicit, using one Newton iteration with a cached Jacobian.
     Antiderivative antialiasing is applied to the nonlinearity.
   - **Gates:**
     - Small-signal response within tolerance of `IG00156Linear` over the cutoff × Q grid.
     - The W1 anchors (`docs/PARITY_ROADMAP.md:99`).
     - Non-harmonic energy ≤ −70 dB on the `cs80_engine_audit --suite alias` saw rows at
       48 kHz.
     - Stable at every rate.
2. **Oscillator promotion** (`src/experimental/ReconstructedOscillator.h` → `src/core/`)
   - It already passes the linear 2× path at −83 dB. It failed only against the clipping
     `Svf` (`docs/OSCILLATOR_EXPERIMENTAL_CHECKPOINT.md:36-40`).
   - Re-qualify it against the new filter.
   - Cut its 22 KB per-instance residual bank to one shared table.
   - Replace the 3.5 % third-harmonic sine with its piecewise triangle→sine shaper.
   - Gate: alias and CPU budget, and the waveform pin-level anchors (saw 3.5 Vpp, sine 3 Vpp).
3. **Envelopes (IG00152/IG00159) as capacitor circuits** (`Dsp.h` `Adsr`/`FilterEnv`
   source path)
   - Model RC charge toward an overshoot target with a comparator threshold.
   - Replace the time-window ×5 bends with level-dependent curvature.
   - Fix the time constant (ln 1000).
   - Stop resetting the filter EG on tail reuse in the circuit engine (`Engine.cpp:796`).
   - Gate: envelope truth model, plus interrupted, retriggered and stolen-tail regressions
     (extend `tests/LiveEnvelopeTimingTests.cpp`).
4. **VCA (IG00151)**
   - Separate the exponential and linear control inputs.
   - Add a small CV feedthrough and offset as declared inference.
   - Use constant-power rank pan.
5. **Gain staging and output reference**
   - Define a documented digital reference: nominal HIGH output volts map to −18 dBFS RMS.
   - Model the output op-amp rail soft limit at its inferred ±15 V supply instead of
     running unbounded.
   - This fixes the +3.9 dBFS source tabs without an arbitrary limiter
     (`OutputTapCircuit.h`).
6. **Oversampling quality setting**
   - A new appended parameter selects Eco (2×), Standard or HQ (8×).
   - Standard is the lowest factor that meets the alias target within the CPU target, chosen
     by measurement, not assumed.
   - Offline bounce always uses HQ, detected with `isNonRealtime()`.
   - Decimator taps and reported latency are computed per mode (`Engine.cpp:23`,
     `Dsp.h:943-1015`).
   - The new parameter gets the next AU version hint, following the append-only rule.
7. **Engine default flip**
   - Fresh instances and new presets use the circuit engine.
   - Sessions with no schema version stay Legacy.
   - Collapse the two cryptic toggles (`parity_mode`, `source_performance`) into the UI
     concepts **ENGINE: Circuit | Legacy (compat)** and **PURE CS-80** (today's isolation
     contract). Host IDs stay unchanged.

## Phase 2 — Voice behavior, keyboard and variation

1. **Free-running voices**
   - Wire `ContinuousVoiceCircuit` (`src/experimental/`) into the engine: held key voltage,
     oscillators that run without note-on reset, and filter state that keeps evolving.
   - Add a cheap idle path, such as block-rate decimated idle evolution with an exact catch-up
     when the voice wakes, to keep CPU in budget.
   - Gate: `docs/VOICE_CIRCUIT_CONTINUITY.md` criteria plus CPU.
2. **KAS scan and sample/hold**
   - Model scan timing and priority.
   - Add the per-voice held-key-voltage droop (0.033 µF node) and the NAND(TR, SUS) hold.
   - Decide, as a declared model, whether a ninth held key is reconsidered.
3. **Keyboard contour key map**
   - Choose a declared mapping with listed alternatives (`docs/KEYBOARD_CONTOUR_EVIDENCE.md`).
   - Drive audio from `src/reference/KeyboardContour.h`.
   - Delete the provisional weights in `PerformanceControls.h:33-34`.
4. **Touch**
   - Model the TWS b/C acquisition: the sampled-and-held initial touch and the AC-coupled
     pitch transient.
   - Calibrate the TRG5 pressure bus.
   - Add velocity and pressure response curves for different MIDI controllers (an
     interface-layer adapter, labeled as such).
5. **Tolerance and drift model** (replacing the sine drift and unused trims at
   `Engine.cpp:41-58, 1245`)
   - Per-card component tolerances, using declared distributions for 5 % resistors and
     10 % capacitors, mapped into tune, cutoff, Q, level and envelope trims.
   - Thermal drift as a slow 1/f random walk, with a warm-up curve.
   - Default amounts set from those physical ranges. Today's ±22 cents becomes a "worn unit"
     preset.

## Phase 3 — Modulation, effects, output

- **SUB (IG00150).** An exponential speed converter, including touch currents, anchored at
  the documented 200 Hz point. Waveforms band-limited where audible.
- **Ring modulator.**
  - An envelope→frequency law anchored to the ADJ p19 points (150 ms and 5 ms periods),
    replacing `kProvisionalRingFrequencyExcursion`.
  - Multiplier DC offset and carrier feedthrough.
  - The 500 Hz extension is masked in PURE mode.
- **OE1/OE2 chorus and tremolo.**
  - Model tremolo from OE1 instead of the generic law.
  - BBD: per-stage charge-transfer loss, clock feedthrough, noise, and true left/right wet
    paths (`src/core/BbdChorus.h`). Reuse `src/reference/Mn3001Packet.h` and
    `Oe2Filters.h` as truth models.
- **PRA wah and EXP pedal.** An optical cell with asymmetric attack/decay (a declared LDR
  model) in `src/core/PraWahCircuit.h`.
- **Output.**
  - Make the GENERAL mono jack audible as an output-mode choice.
  - Add a headphone amplifier as an optional listening model.
- **Noise.** Shaped noise PSD, with a decision on which sources are shared and which are per
  voice.

## Phase 4 — Tones, memories and library

1. **The 22 tone tabs**
   - Re-examine the 25 unknown matrix cells (`HardwareControl.h:102`) with native crops
     through `circuit-inspection.yml`.
   - Keep any cell that stays unresolved as a labeled hypothesis
     (`FactoryToneHypotheses.h`).
   - Refine the BA driver impedance and diode I-V boundaries (`SourceFactoryTones.h`).
2. **Memory panels.** Add the electrical loading and switch-state model for the four
   memories and PANEL.
3. **New curated factory library on the circuit engine**
   - About 300 hand-voiced programs, categorized and tagged: classic CS-80 sounds by name of
     technique, plus modern performance patches.
   - Replaces the 178 random-generated variants (`FactoryPresets.cpp:552-610`).
   - The old 222-program bank moves to a read-only "Legacy Library", so old sessions still
     recall it.
4. **Program Change, and MIDI CC for the ribbon.**

## Phase 5 — UI/UX: original panel first, then better ergonomics than Arturia

**Findings from the editor audit:**
- The look is strong.
- Several placements differ from the original. The code itself notes that the original
  puts the foot-controller group on the left-hand panel (`PluginEditor.cpp:3091-3093`), but
  the editor puts it on the right. Chorus and tremolo are a 3-way selector standing in for
  two switches (`:3011`).
- The 360×378 px signal-flow plate is software-only.
- The CS-80 has no pitch or mod wheels. Here they are hidden under VOICE SETTINGS, and so
  are a duplicate LEVEL control and the pan control.
- Ergonomic basics are missing: undo, MIDI learn, A/B, typed values, context menus,
  fine-drag, QWERTY play, preset audition, and memory "store panel".
- The engine-mode controls are buried in Advanced → page 4, and their help text points to
  an "About" page that does not exist (`PluginEditor.cpp:3473`).

1. **Panel conformance audit (do first).**
   - Build `docs/PANEL_CONFORMANCE.md`: a control-by-control table covering the owner-manual
     index 1–46 and the service S2 panel drawing.
   - Fetch the drawings with `.github/workflows/circuit-inspection.yml` /
     `scripts/inspect_circuit_sources.py`.
   - Columns: position, group, legend text, cap colour, lever direction, switch type.
   - Record each source conflict in the table rather than resolving it silently.
2. **Re-lay the main view to the original arrangement.**
   - Remove the signal-flow plate, so the channel rows span the panel as on the hardware.
   - Move the foot and contour groups to the side the evidence shows.
   - Replace the 3-way selector with separate chorus/tremolo switches.
   - Print full tone-tab names in the hardware wording instead of STR/BRS codes
     (`UiComponents.cpp:2131-2153`).
   - Match the memory-lid placement and proportions.
   - Replace the `midShift` ±24 px hack (`PluginEditor.cpp:183-189`) with one layout table
     taken from the conformance doc.
3. **Tuck the modern items away.**
   - **Toolbar:** a slim strip holding the preset display, prev/next, library, A/B, undo/redo,
     ENGINE / PURE CS-80 switches and a menu.
   - **"Studio" view (replaces Advanced):** tabs for Modulation (MSEG, LFO, matrix),
     Keyboard/MPE/Tuning, FX, Arp and Settings.
   - **Optional drawer:** the signal-flow/scope plate as a live voice monitor.
   - The pitch and mod wheels move to a modern controllers strip that is hidden in PURE mode.
4. **Interaction polish.**
   - Faders drag relative to the cap, with no jump: `setSliderSnapsToMousePosition(false)`.
   - Shift-drag gives fine adjustment.
   - Ctrl/Cmd-click types a value; double-click resets.
   - The right-click menu offers MIDI learn, reset, type value and copy/paste.
   - Ribbon range uses the full strip from any touch point (fix `UiComponents.cpp:3885`).
   - QWERTY note input with octave shift.
   - Library audition of rows.
   - Prev/next through the current library filter, including user presets.
   - Memory lid: store panel → memory, copy memory ↔ memory, and copy rank I ↔ II.
   - Plain-language tooltips (owner-manual style), with the engineering text in an
     expandable "Circuit notes" section that shows the evidence class.
5. **Readability and rendering.**
   - Embed one OFL-licensed condensed typeface through BinaryData, so the panel looks the
     same on every OS (today it uses the system default, `PluginEditor.cpp:81`).
   - Legend floor of 10.5 px at the minimum size, and a WCAG-contrast pass on readouts.
   - Zoom presets at 75/100/125/150/200 %.
   - Draw the crinkle tile at physical scale for HiDPI.
6. **Editor code health** (behavior-preserving, before the re-layout).
   - One `src/plugin/ArtCommon.h` replacing the three copies of the wrinkle tile,
     `fillRelief` and `displayFont`.
   - Delete the dead `makeRelief`, `louvre`, `stamped`, `matteSheet` and
     `RovingFocusGroup`.
   - Split the editor `Impl` (about 1,830 lines) into Toolbar, ChannelRow, CentreRow,
     PerformanceDeck and MemoryLid components.
   - Cache the memory-lid art.
   - Make the fader-cap variant deterministic instead of keyed by object address
     (`UiComponents.cpp:2375`).
   - Turn `PaintCache` into an LRU instead of wiping it at 700 entries.
   - Replace the 8 Hz full-parameter polling in `syncPresetDisplay`
     (`PluginEditor.cpp:3213-3281`) with parameter listeners.
7. **UI tests.**
   - Add tolerance-based golden-image diffs at 1600×1000 and 1120×700.
   - Add tab-order, minimum hit-target (≥ 24 px), library-behavior and context-menu tests to
     `src/tools/ui_snapshot.cpp`.
   - Run the `design:accessibility-review` skill on the captures.

## Phase 6 — Modern layer that beats Arturia V4 (all inert in PURE CS-80)

- **Modulation sources**
  - 3 drawable multi-segment envelopes (MSEGs), loopable and tempo-syncable.
  - LFO waveforms beyond sine; today the waveform is hard-coded to sine at `Engine.cpp:1150`.
- **Modulation routing**
  - A 16-slot modulation matrix whose sources include the original controls: ribbon,
    pressure, contour, SUB and ring envelope.
  - A keyboard modulation page covering velocity, aftertouch, mod wheel and key-follow
    curves.
- **Tuning and MIDI**
  - MTS-ESP and Scala/KBM microtuning.
  - MPE: RPN zone negotiation, an upper zone and high-resolution velocity (MIDI 2.0 when
    JUCE allows).
  - MIDI learn with a per-session map.
  - Program Change.
- **Editing**
  - Undo/redo through the APVTS `UndoManager`.
  - A/B compare.
  - Copy rank I↔II and copy memory↔panel.
  - "Randomize within original ranges".
- **Host and engine**
  - Host parameter groups (`ParamDef.group` already exists; `PluginProcessor.cpp:153-240`
    ignores it).
  - Voice modes: 8 voices (authentic default), plus optional 16-voice and unison modes as
    labeled extensions.
  - Optionally sample-accurate automation for key parameters.
- **FX**
  - Gain-compensated FX.
  - Better plate and tape models.
  - A BBD ensemble and a spring reverb.
  - Per-slot oversampling for the drive.
- **Learning tools**
  - A tutorial and tour overlay.
  - Per-control help that cites the owner's manual page and shows the evidence class
    (source-derived or inferred). Arturia has no equivalent.

## Phase 7 — Release qualification

- Root-cause the intermittent Linux pluginval exit 139 instead of relying on reruns.
- Run real-DAW session checks: Live, Logic, Reaper, Bitwig, Cubase and FL.
- Choose a LICENSE and settle JUCE licensing (AGPL or commercial).
- Get a legal review of the "CS-80" and "Yamaha" names: product name, bundle ID, and the
  nameplate art in the editor.
- Sign and notarize. Build installers and a universal macOS binary.
- AAX only if the owner obtains the Avid SDK and a PACE agreement.

---

## Execution order

Each step is one or more reviewable commits on `claude/clever-babbage-sm9ifa`.

1. **Phase 0** in full, plus the **Phase 5.6** editor code-health refactor. Neither changes
   the sound or the look, so it is safe to land first.
2. **Phase 1, in this order:**
   1. Gain staging (1.5).
   2. Envelope fixes (1.3).
   3. IG00156 filter (1.1).
   4. Oscillator promotion (1.2).
   5. Oversampling setting (1.6).
   6. Engine default flip (1.7), last, so users first meet the circuit engine at its best.
3. **Phase 5.1–5.4:** the panel conformance audit, then the re-layout and interaction
   polish. This can run in parallel with Phase 2.
4. **Phases 2, 3 and 4** (voice behavior, modulation and FX circuits, and the curated
   library), then **Phase 6** (modern layer), then **Phase 7** (release).

Each phase ends with:
- A `docs/STATUS.md` update.
- A dated entry in `docs/HARDWARE_COVERAGE.md` for every row it changes.
- Listed intentional audible changes, each with the prior commit to use for historical
  renders.

---

## Verification (applies to every phase)

- **Builds and tests**
  - Core: `cmake -S . -B build-core -DCS80_BUILD_PLUGIN=OFF -DCS80_BUILD_TESTS=ON && cmake --build build-core && ctest --test-dir build-core --output-on-failure`.
  - Plugin: `cmake --preset linux-release && cmake --build --preset linux-release && ctest --preset linux-release`.
  - UI smoke runs through `scripts/run-ui-smoke-linux.sh`.
- **Compatibility gates**
  - The `dsp-measurements.yml` legacy byte-identical job must stay green: old sessions sound
    identical.
  - Every intended circuit-engine sound change is documented with the prior commit for
    historical renders.
- **New stage gates.** Truth-model vs realtime residuals use tolerances committed *before*
  evaluation in `tests/parity/tolerances-v2.json`, and every new gate is mutation-checked.
- **Engine audit**
  - Run `cs80_engine_audit --suite all --rates 44100,48000,96000`, which reports alias, click,
    headroom and loudness spread.
  - Targets: alias ≤ −70 dB, no source tab above 0 dBFS at the default volume, and a
    `click_steal` score ≤ 1.
- **CPU.** `source_factory_benchmark` at 44.1/48/96 kHz with the p99 and deadline-miss
  counts recorded per phase.
- **Host.** Pluginval strictness 10 on all four platforms, run in CI.
- **Screenshots.** Refreshed at 1600×1000 and 1120×700.
- **Listening.** A blind, level-matched listening panel per phase against the previous build
  and against public CS-80 recordings, labeled as listening evidence, not calibration.
