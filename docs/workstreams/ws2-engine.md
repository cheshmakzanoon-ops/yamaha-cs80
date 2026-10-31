# ws2-engine: engine I/O, smoothing, oversampling, default flip, output and noise

Workstream report, 2026-10-31. Branch base: integration commit `6719ce4`.
Plan steps: parameter smoothing (former 0.2), 1.6 oversampling quality, 1.7 engine
default flip, Phase 3 output and noise, Phase 6 host parameter groups.

Everything here is a property of this software. None of it is a measurement of,
or a claim about, a Yamaha CS-80. Evidence classes follow
[ANALOG_FIDELITY_REQUIREMENT.md](../ANALOG_FIDELITY_REQUIREMENT.md): *source-derived*
(schematic/manual page), *declared inference* (with alternatives), *numerically
verified* (a gate in this tree).

Pre-registered tolerances for every new gate live in
[`tests/Ws2EngineTolerances.h`](../../tests/Ws2EngineTolerances.h), committed before
the realtime models were evaluated (commits listed per gate below).

## Summary

| Step | State | Main commit |
| --- | --- | --- |
| Parameter smoothing (circuit engine) | done | `d29f6eb` |
| 1.6 Oversampling quality | done; the −70 dB target is not reachable with the current oscillator at any factor, Standard = 4x by the documented rule | `ed877d4` |
| 1.7 Engine default flip | done at the plugin layer; the INIT button hook is left to ws8 | `83f8a0e` |
| Phase 3 output and noise | done: GENERAL and PHONES listening, declared noise PSDs, shared/per-voice model; EXP volume optics (ws4 hand-off) after the merge | `62e18c1`, `3fcff91` |
| Phase 6 host parameter groups | done; sample-accurate automation skipped (documented) | `b533184` |

Commits on the branch (oldest first): `fa9b8ec`, `f417d32`, `a158dc8`, `44145b9`
(pre-registered tolerances and their documented re-registrations), `ed877d4`
oversampling, `d29f6eb` smoothing, `83f8a0e` engine default flip, `62e18c1`
output and noise, `b533184` parameter groups, `1327147` merge of the integration
branch (ws4, ws7), `3fcff91` EXP optics hand-off and merge follow-ups, then
this report.

## 1.6 Oversampling quality

**State: done.** The −70 dB alias target is not met by any factor with today's
PolyBLEP oscillator; Standard is therefore chosen by the documented fallback rule
(below) and is one constant for the integrator to change after ws1's oscillator
promotion.

### Implementation

- [`src/core/Oversampling.h`](../../src/core/Oversampling.h): the mode and the
  factor rule `oversamplingFactor(mode, circuitEngine, offline, hostRate)`.
  - Eco = 2x, Standard = `kStandardOversampling` (**4x**, the single integrator
    constant), HQ = 8x, at 44.1/48 kHz.
  - Higher host rates divide the factor by `hostRateMultiple` (1 at 44.1/48 kHz,
    2 at 88.2/96, 4 at 176.4/192), never below 2x: aliases fold at the INTERNAL
    rate, so a 96 kHz host at 2x already equals 48 kHz at 4x.
  - Legacy (compat) maps Eco and Standard to its historical 2x and ignores the
    offline override, so an old session renders and bounces exactly as before;
    only an explicit HQ selection changes it.
  - The circuit engine renders every offline bounce at HQ.
- [`src/core/Dsp.h`](../../src/core/Dsp.h) `OversamplingDecimator`: the existing
  255-tap Kaiser (beta 8.6) design generalized to 128N−1 taps with the cutoff
  scaled by 2/N. Factor 2 is `Decimator` bit for bit (same coefficients and
  summation order). The last internal sample of each host frame is retained, so
  the reported latency is `(centre − (N−1)) / N` = **63 host samples at 2x, 4x and
  8x**. The transition band is fixed in Hz by the host rate, so the filter's
  duration, and its delay, cannot shrink with N at the same specification.
- [`src/core/ExternalModulationInterpolator.h`](../../src/core/ExternalModulationInterpolator.h):
  N samples per host frame, the same design per factor (EXT control delay 63.5,
  63.75 and 63.875 host samples; audio latency unchanged).
- `Engine` (`prepare`, `configureInternalRate`, `updateOversampling`,
  `setNonRealtime`, `getOversamplingFactor`, `getLatencySamples`): `prepare()`
  first prepares every rate-sized buffer (chorus, BBD, FX slots) at the largest
  factor the host rate allows, then at the real rate. A later factor change from
  the quality mode, the offline flag or a policy change re-prepares on the render
  thread **without allocating** and resets like a policy change (notes stop).
  Memory cost: the reserved delay lines grow from about 3 MB to about 12 MB per
  instance at 48 kHz (internal rate is capped at about 384 kHz by the rule).
- Plugin ([`PluginProcessor.cpp`](../../src/plugin/PluginProcessor.cpp)):
  `prepareToPlay` and every `processBlock` pass `isNonRealtime()` to the engine
  before the control frame; a factor change resets the keyboard hand-off like a
  policy change; `setLatencySamples` is called whenever the engine's latency
  differs from the reported one. The quality is a session setting: factory and
  user presets neither set nor clear it (`isPhysicalSetupParameter`,
  `loadUserPreset`).

### Measurements (this container, 2026-10-31)

Tool: `cs80_engine_audit --suite alias --rates R --oversampling MODE` (worst row of
the alias grid: saw/pulse, ring on/off, notes 72–108, 8' and 2', 65 536-point
Blackman-Harris, non-harmonic energy 20 Hz–20 kHz versus partials) and
`--suite cpu` (8 held voices, PANEL defaults with chorus, single thread, best of 3
four-second passes). The machine is shared by eight builds, so CPU numbers are
noisy (±30 % between runs); compare within one column.

Worst non-harmonic level, dB (wave, note x footage ratio); circuit engine:

| Host | Eco | Standard | HQ |
| --- | --- | --- | --- |
| 44.1 kHz | −50.0 (pulse 96x4) | −63.5 (pulse 96x4) | −67.6 (saw 72x4) |
| 48 kHz | −51.1 (saw 84x4) | −66.5 (pulse 96x4) | −67.6 (saw 72x4) |
| 96 kHz | −66.5 (2x) | −66.5 (2x) | −67.6 (4x) |

Legacy at its default stays at 2x: −32.5 / −33.8 / −39.0 dB (44.1/48/96 kHz),
unchanged; legacy at HQ: −44.3 / −45.2 / −45.2 dB.

CPU, real-time factor for 8 voices, circuit engine at 48 kHz: 2x 0.35, 4x 0.55,
8x 0.99 (first run; a second, busier run read 0.24 / 0.56 / 1.20). Legacy at 2x:
0.30–0.34 in the same runs.

### Rule for Standard (single constant `kStandardOversampling`)

Standard is the lowest factor whose worst non-harmonic level meets the −70 dB
target **within the realtime CPU budget**; if no factor does both, it is the
largest factor within the budget. The plan states the CPU target only as
0.15 RTF at 2x after the later CPU pass, so the budget used here is a declared
engineering choice: **at most twice the CPU of the same engine at 2x**, measured
in one session (machine-independent).

- No factor reaches −70 dB. HQ improves on 4x by only 1 dB and its worst row
  moves to a LOW note (72 x 4 = 2.1 kHz fundamental), so the remaining floor is
  not fold-over aliasing. Declared inference: it is the oscillator's own
  non-harmonic floor (float phase accumulation, see the cross-factor gate below,
  and the 30 µs reset pulse); alternatives: window leakage of the audit, PolyBLEP
  residual. ws1's oscillator promotion is expected to move it.
- 4x costs about 1.6x the 2x CPU (inside the budget); 8x about 2.8x (outside).
- So **Standard = 4x**. When ws1 reports the factor its oscillator needs, change
  `kStandardOversampling` only; the decimator, latency and tests follow.

### Tests (`cs80_oversampling_tests`, 86 checks; host checks in the UI self-test)

- Factor rule table (legacy, circuit, offline, 44.1/48/96/192/768 kHz).
- Decimator per factor: factor 2 coefficients and output identical to `Decimator`;
  dense DFT of the float coefficients: passband −0.0004/+0.0004 dB through 20 kHz
  at 44.1 kHz, stopband −87.1 / −87.6 / −87.8 dB (2x/4x/8x); measured render-path
  delay 63 ± 0.001 host samples.
- Engine at every mode/rate/policy: factor and internal rate follow the rule,
  latency 63; a render-thread switch equals a full re-prepare at the new mode;
  the offline flag gives HQ for the circuit engine and nothing for legacy; no
  allocation across quality, offline and policy switches (counting
  `operator new`).
- Cross-factor render: a pure SINE voice through the linear path renders with the
  same delay (lag Standard +0.32, HQ +0.44 host samples; the oscillator advances
  its phase before output, a one-internal-sample lead of 1/2 − 1/N) and level
  (ratio 0.99993 / 0.99990) at every factor.
- Host (`tests/EngineHostTests.h`, run by `cs80_ui_smoke`): choice parameter
  host245, AU hint 9, default Standard; reported latency equals the engine's for
  every mode/rate/engine; offline prepare and an offline switch inside
  `processBlock`; state round trip; presets keep the session value; a state
  without the parameter restores Standard.

Tolerances: `fa9b8ec` (decimator, latency), then `f417d32`, `a158dc8`, `44145b9`
(cross-factor gate). That gate was re-registered twice, each time BEFORE
re-evaluation and with the failure recorded in the header:
1. A 0.25–0.5 s waveform comparison failed at HQ (1.03e-2 > 1e-2): float phase
   accumulation wanders at every rate.
2. An onset-window comparison failed (1.08e-2): the one-internal-sample lead
   (0.375 host samples between 2x and 8x).
3. The final metric is the cross-correlation lag (< 0.5 host sample; any decimator
   delay fault is at least 1) plus level (1 %), evaluated at 421.875 Hz, where
   the float phase increment is exact at every factor.

Mutation checks (each run, then restored):

| Mutation | Result |
| --- | --- |
| No 2/N cutoff scaling for 4x/8x | caught (stopband) |
| Reserve buffers at 2x only | caught (allocation check) |
| Decimator not re-prepared on a factor change | caught (cross-factor lag and level) |
| Retained phase fixed at 1 | caught (cross-factor lag) |
| `hostLatencyFor` != 63 or non-integer | caught at compile time (`static_assert`) |

### Intentional circuit-engine sound change

Standard renders the circuit engine at 4x at 44.1/48 kHz (2x at 88.2/96 kHz,
unchanged there). With Standard set to 2x the v2 probe was byte-identical to
`f6b126d`, so the whole v2 difference of this commit is the factor. Fixtures
whose independent oracles tick at 2x pin Eco (OutputTap, PraWah, ActiveTail,
ParityMode, SourcePerformance, SourceFactoryIntegration, parity capture, two UI
self-test fixtures), each with a comment.

## Parameter smoothing (circuit engine only)

**State: done.** Legacy is untouched (byte-identical gates; a legacy LEVEL step
still measures −39 dB HF, the historical unsmoothed behavior).

### Implementation

- [`src/core/Smoothing.h`](../../src/core/Smoothing.h): `smoothingCoefficient(seconds,
  rate)` (k = 1 − e^(−1/(τ·fs)), any internal rate), `SmootherBank` (double state,
  128 slots, one coefficient, settles exactly on its target and then costs
  nothing), `ControlSmoother` (single controller) and `legacyPerSampleSeconds`.
- **One place per sample** (`Engine::beginSmoothedControls`, called once at the
  top of `processInternalSample` in the circuit policies, before the voice loop):
  - Settings: `visitSmoothedSettings` in `Engine.cpp` is the ONE field list.
    After every `buildSettings` the new values become targets
    (`retargetSmoothedSettings`) and the fields keep running values; each sample
    advances the bank and writes the fields back. The rank chain reads
    `global`/`rankSettings` exactly as before: **no rank-chain use site changed**.
  - Global fields: master tune, detune, mix, brilliance, resonance, volume, initial
    bend, EXT level, SUB VCO/VCF/VCA and AT VCO/VCF depths, ring speed/depth/
    amount, chorus speed/depth, dispersion, drift, analog noise, tuning reference,
    mod-wheel depths, the four keyboard contours, LFO rates and depths, FX mixes.
    Macros act through these fields.
  - Rank fields: sine, noise, PW, PWM speed/depth, HPF/LPF effective CV (octave
    domain), HPF/LPF resonance, VCF level, LEVEL, initial/after brilliance and
    level, pan.
  - Not smoothed, by design: switches and choices; envelope times and IL/AL/S
    levels (the RC envelopes already move continuously toward a new level);
    portamento/release/envelope-scale times; the SUB speeds (their own smoothing).
  - Controllers: master bend, CC1, ribbon (+ RIBBON parameter), and per voice
    poly/channel pressure, MPE bend and CC74 use smoothed values for one internal
    sample; `endSmoothedControls` restores the event-set targets, so event handlers
    (ws3's touch work) are not edited and `getVoiceInfo` still reports targets. A
    voice whose `age` changed (new note) snaps to its own fresh touch, bend and
    timbre. CC11/expression acts **at its gain stage only, through the EXP optical
    cell** (see "EXP volume optics" below); the PRA wah cell reads the target.
- Smoothers start at their targets: `reset()` (prepare, policy or factor change)
  requests a snap that lands on the first rendered sample, so any frame applied
  before audio starts defines the initial state.
- The two per-sample constants on the circuit path are replaced:
  - `0.003f` volume: the circuit engine's volume is a bank field (5 ms).
  - `0.0015f` legacy-SUB speed: `legacyPerSampleSeconds(0.0015)` = 6.94 ms, the
    time constant it had at the 48 kHz/2x reference, computed at the internal rate.
  Legacy keeps both constants exactly.

### Evidence class

Declared engineering choice: τ = 5 ms one-pole for every continuous control. The
CS-80's panel CV lines are RC-filtered on the boards, but their values are not in
the evidence set. Alternatives: 2 ms (residual zipper on large cutoff jumps),
10–20 ms (audibly lagging bends). It matches the existing 5 ms HIGH/LOW readout
dezipper.

### Measurements and tests (`cs80_smoothing_tests`, 84 checks)

- Smoother step response equals 1 − e^(−t/τ) within 1e-6 at 88.2–768 kHz; the
  63.2 % point at τ.
- Engine time constant, from per-sample diagnostics after a step: LPF cutoff and
  pitch bend reach 63.2 % at 5.0113 ms (44.1 kHz Eco, one-sample read-out
  resolution) and 5.0000 ms (48 kHz Standard, 48 kHz HQ, 96 kHz Standard).
- Zipper gate: a control stepped mid-note on a sine voice; HF energy (4–20 kHz)
  of (stepped − unstepped render) as a fraction of the difference energy. Limit
  −60 dB, registered in `fa9b8ec`. Unsmoothed legacy step: −39.3 dB.

  | Control | 48 kHz Eco / Std | 96 kHz Eco / Std |
  | --- | --- | --- |
  | LEVEL, SINE, PAN, after-touch LEVEL | −83.3 / −83.4 dB | −70.6 / −70.6 dB |
  | VOLUME, CC11 expression | −83.0 / −83.1 dB | −70.6 / −70.6 dB |
  | MIX | −78.4 / −78.5 dB | −90.3 / −90.3 dB |

- Static renders unchanged: re-applying an unchanged frame every block is
  bit-identical to applying it once, and a frame applied before the first sample
  defines the start. Against the pinned v2 baseline (`f6b126d`), a static
  circuit-engine probe (3 rates, source and isolation, PANEL and two tone tabs,
  note events, frames re-applied each block, Eco) is **byte-identical**.
- Mutation checks: τ = 0 caught (47 failures); new-note snap removed caught;
  first-sample snap removed caught; CC11 gain smoothing removed caught (first
  as the 5 ms one-pole, again after it became the optical cell). A
  retarget-time snap that duplicated the first-sample snap survived (fully
  redundant, since nothing reads the fields in between) and was removed, so the
  remaining mechanism is the gated one.

### Intentional circuit-engine sound change

Every listed control now glides with τ = 5 ms. The v2 probe moves brilliance,
resonance, pressure, expression and ribbon every block, so its bytes change.
Existing tests that read a controller's full effect one frame after the event
now read it after 0.2 s of settling (SourcePerformance controller transports),
and the OutputTap and PRA-wah oracles model the 5 ms volume law and the
optical expression gain (`tests/ExpressionOpticsOracle.h`).

## 1.7 Engine default flip

**State: done at the plugin/host layer.** One hook is left to ws8 (INIT button).

### Implementation

- The core `ParamDef` defaults of `source_performance` and `parity_mode` are
  **unchanged** (Legacy, off). The CI legacy gate, the legacy factory bank (built
  from `ParamValues()`) and every default-ParamValues render keep their sound.
- Host layer ([`PluginProcessor.cpp`](../../src/plugin/PluginProcessor.cpp)):
  - The APVTS default of `source_performance` is now **on** (Circuit), so host
    "reset to default" selects the circuit engine.
  - Fresh instances run `applyFreshInstancePatch()`: factory program 0 ("Tone I:
    String 1", as before), the original six-function SUB panel and now the
    circuit engine.
  - New-preset creation: `loadInitPatch()` loads the factory "Init Patch" program
    (as the INIT button does today) and, like a fresh instance, selects the
    original SUB panel and the circuit engine ("Init Patch / Original SUB");
    PURE CS-80 (a session policy) is left alone. The legacy bank programs themselves still
    recall Legacy (they are the future Legacy Library, ws6), so loading one from
    a circuit-engine instance is an effective policy change and stops sounding
    notes, as any policy change does.
- Two user-facing concepts over the unchanged host IDs, indexes and AU hints:
  - **ENGINE** (`source_performance`, host244, hint 7): host name "Engine",
    value text "Circuit" / "Legacy (compat)".
  - **PURE CS-80** (`parity_mode`, host243, hint 6): host name "PURE CS-80"; the
    isolation contract, rendered by the circuit engine whatever ENGINE says.
  - Core ParamDef display names: "ENGINE: CIRCUIT", "PURE CS-80", group "ENGINE".
  - Processor API: `EngineMode { circuit, legacy }`, `setEngineMode`,
    `getEngineMode`, `getEffectiveEngineMode`, `setPure`, `isPure`,
    `loadInitPatch`. The setters wrap a host change gesture.
- State schema **3** ([`PluginProcessor.h`](../../src/plugin/PluginProcessor.h)):
  a missing or malformed ENGINE value means the default of the schema that wrote
  the state: Legacy for no marker, 1 and 2; Circuit for 3. Literal 0/1 values are
  always taken as stored; a missing value never inherits the live instance.
  PURE CS-80 always falls back to off.
- **Bug fixed on the way:** a state restored from XML carries the schema marker
  as a string property (`ValueTree::fromXml` keeps attributes as strings), and
  the restore code accepted only int/double vars, so every XML-restored state
  was read as schema 1. Nothing observable depended on schema 2, so this was
  latent; the schema-3 rule needs it. A complete decimal string now counts as a
  marker; anything else still means schema 1.
- The Advanced page labels (SOURCE PERFORMANCE / PARITY ISOLATION) are not
  renamed here: ws8 builds the toolbar ENGINE and PURE CS-80 switches over the
  API above.

### Tests

- `tests/SourcePerformanceHostTests.h`: the default check now expects the
  circuit engine (intentional change, commented); all old-session checks
  (no schema marker, missing/malformed values, user presets, factory recall)
  are unchanged and pass.
- `tests/EngineHostTests.h` `testEngineDefaultHost`: core defaults still Legacy;
  host names and value text; a fresh instance renders `sourcePerformance`; PURE
  renders isolation whatever ENGINE says; legacy factory programs keep Legacy;
  `loadInitPatch` gives the circuit engine with the original SUB and keeps PURE;
  the schema table (no marker, 1, 2, 3) x (missing, garbage, 0, 1) with the live
  value set opposite to the expected fallback; states are written as schema 3 and
  a saved Legacy choice survives.
- UI self-test: "initial program values agree" now expects ENGINE on (the fresh
  instance is program 0 plus the circuit engine, by design). Fixtures that
  exercise the legacy-start contract select Legacy (compat) explicitly, each
  with a comment: expression persistence, keyboard hand-off, source routing,
  session restore of rank identities, default-off source selector audio, output
  LOW trim, the keyboard-pressure host fixture and the readiness/Advanced-page
  fixture. The performance-control state check now lets ENGINE restore its
  saved value.
- ws7's panel golden images (`tests/PanelEditorTests.h`, after the merge): the
  goldens were recorded on a Legacy fresh instance; the Circuit default adds the
  status chip "MODE SOURCE PERFORMANCE" next to PANEL (block mean 70.7 / 60.3,
  limit 8.0, the only changed region). The fixture pins Legacy (commented), and
  both views then match at 0.0000 % changed. Re-recording the goldens on the
  Circuit default would cover that chip instead.

## Phase 3 output: GENERAL jack and headphone listening model

**State: done.**

### Implementation

- New appended parameter **LISTENING OUTPUT** (`output_jack`): 0 LEFT/RIGHT jacks
  (default), 1 GENERAL mono jack, 2 PHONES. Physical setup like HIGH/LOW: presets
  neither set nor clear it. Circuit engine only; Legacy (compat) keeps its
  historical output arithmetic for every choice. Both are original outputs, so
  they stay selectable under PURE CS-80.
- Engine hook (the circuit output block of `processInternalSample`): the preamp
  sources `driveL/driveR` feed the three always-clocked output networks
  (`OriginalOutputCircuit`) **and** two headphone stages; the selection only picks
  which node the host hears. GENERAL is the existing node: the 0.47 active sum
  formed before coupling (docs/OUTPUT_REALTIME_PROPOSAL.md), now audible on both
  host channels. `Engine::getListeningSample()` exposes the selected internal
  sample (render thread) for tests.
- Headphone amplifier: realtime
  [`src/core/HeadphoneAmpCircuit.h`](../../src/core/HeadphoneAmpCircuit.h), truth
  [`src/reference/HeadphoneAmp.h`](../../src/reference/HeadphoneAmp.h):

  | Element | Value | Evidence class |
  | --- | --- | --- |
  | HL/HR taken before the coupling capacitors and HIGH/LOW switch | topology | source-derived: PRA54, OUTPUT_LEVEL_SOURCE_REVIEW.md |
  | IC11 feedback amplifier, 100 k in / 150 k feedback | gain −1.5 | source-derived (SUB29, service manual p29) |
  | Inverting configuration | sign | declared inference; alternative non-inverting (+2.5) |
  | 22 Ω output resistor dividing against the load (feedback before it) | ×RL/(RL+22) | resistor source-derived; feedback point declared (alternative: after it, no division) |
  | Headphone load | 32 Ω | declared inference; alternatives 8, 300, 600 Ω |
  | Jack wiring | direct (Δ-marked drawing) | declared choice; alternative 470 µF + 10 k bias (PN5 p67), about 6 Hz high-pass into 32 Ω, implemented and tested |
  | Large-signal limit | the preamp driver's ceiling and C1 tanh knee (−1 dB) | declared: shared ±15 V rails; current limiting and crossover distortion not modelled |

  Small-signal gain into 32 Ω: −1.5 × 32/54 = −0.889 (−1.0 dB) relative to the
  preamp source; the stage reaches its rail knee about 3.5 dB earlier than the
  LEFT/RIGHT driver.

### EXP volume optics (ws4 hand-off)

**State: done (circuit engine).** Source placement (FOOT_CONTROL_EVIDENCE.md):
OE1/OE2 → PRA I2/I3 optical expression → stereo VOLUME → output preamps; the
optical branches I2 → VL1 and I3 → VL2 are driven by the same EI pedal line as the
wah photoresistor, so "a linear final-output CC11 multiplier does not reconstruct
these networks".

- [`src/core/ExpressionOptics.h`](../../src/core/ExpressionOptics.h)
  `ExpressionOpticalCell`: ws4's `PraWahCircuit::Profile` constants and
  recurrence (heel 1 MΩ, toe 2 kΩ, 12 ms brightening / 60 ms darkening in log R),
  with log R mapped back linearly to the 0..1 pedal position. That position is
  the circuit engine's expression gain (`smoothedVolume * 4 * gain`). It replaces
  the 5 ms CC11 one-pole: the cell is the smoothing. It starts on the pedal at
  the first sample after a reset (no swell), and a settled cell returns the
  pedal itself, so a static pedal keeps its exact gain.
- Evidence class: placement and drive are source-derived; the law is a
  **declared inference** (the same LDR stage as the wah). Toe = bright = full
  level, so the level rises in about 12 ms and falls in about 60 ms (log R).
  Alternatives: a shunt LDR (reversed polarity, slow opening), a different taper,
  separate VR11/VR12 trims per channel.
- Legacy keeps the immediate linear gain (byte-identical gates).
- Tests: the OutputTap and PRA-wah full-pipeline oracles use an independent
  closed form of the cell (`tests/ExpressionOpticsOracle.h`) at their existing
  tolerances; the CC11 zipper gate still bounds the HF energy of a pedal step.

### Tests (`cs80_output_listening_tests`, 40 checks; host checks in the UI self-test)

- Headphone stage versus the reference, direct and capacitor coupling, loads
  8/32/300/600 Ω, 88.2/192/384 kHz, 20 Hz–20 kHz: limits 0.02 dB and 0.5°
  (registered `fa9b8ec`); measured worst 0.0000 dB and 0.003° (capacitor
  coupling at 88.2 kHz). |H(1 kHz)| into 32 Ω: 0.8889 direct, 0.8877
  capacitor-coupled (10 k bias in parallel). Static transfer through the knee
  within 1e-6.
- Engine: GENERAL listening equals the GENERAL node exactly, sample by sample,
  in source and PURE policies; LEFT/RIGHT listening equals the jacks exactly;
  GENERAL is mono and measures −0.537 dB against LEFT for an equal pair (the
  0.94 active sum); PHONES measures −0.802 dB against LEFT, which is
  1.5·32/54 over the LEFT jack's own loaded gain (0.975); Legacy renders
  bit-identically for all three choices.
- Host (`testListeningOutputHost`): the choice follows OVERSAMPLING with AU hint
  9 and default Left/Right; GENERAL is mono and audible through
  `processBlock`; factory recall keeps it; a state round trip restores it.

## Phase 3 noise: declared PSDs, shared versus per voice

**State: done.** Statistics below are of this software's sources; the CS-80's
noise level and spectrum are not measured.

### Decision: which sources are shared and which are per voice

| Source | Model | Evidence class |
| --- | --- | --- |
| Panel WHITE NOISE (rank I/II NOISE levels) | **one generator for all voices**: every voice of the circuit engine reads the same sample, scaled by its rank's level | source-derived fan-out: one Zener generator on the SUB board (SUB29), rank level VCAs C1/C2 → NO1/NO2 → M-board NI43 (FACTORY_TONE_INTEGRATION_PROPOSAL route 6). Ranks I and II share the generator (declared: both NO1/NO2 derive from it) |
| SUB oscillator NOISE function (NO3) | same generator (not wired here: ws4 owns the SUB) | source-derived (SUB29) |
| Background hiss (ANALOG NOISE overlay) | mix-bus/output op-amp noise: **one independent source per output channel**, shared by all voices, scaled by VOLUME as on a mix bus | declared inference |
| Per-voice device noise (IG00156/IG00151, oscillator) | not modelled | declared: below the bus floor; alternative: independent per card and gated by each VCA |

Legacy keeps its independent uniform draws from the shared RNG. The circuit
engine still consumes those same draws (discarded), so oscillator start phases
keep their sequence and the noise is the only audible change.

### Declared PSDs (truth model `src/reference/NoiseSpectrum.h`)

- Panel generator, from SUB29 component values: 0.047 µF into 100 k
  (33.86 Hz high-pass), IC1 1 M / 4.7 k (gain 213.8), 10 µF NP into 10 k
  (1.59 Hz high-pass), 10 k / 10 M inverting stage (gain 1000). With a declared
  NJM4558-class gain-bandwidth product of 3 MHz (the type SUB30 names for
  IC6/IC12 on the same board), the stages close at 14.03 kHz and 3.00 kHz. So
  the "white" noise is band-limited: −3 dB at about 3 kHz, −12.7 dB at 10 kHz.
  Alternatives: a faster device (higher corners), a capacitor across 10 M (lower),
  rail clipping of the very high chain gain (flatter spectrum, non-Gaussian),
  ideal white (the previous model). Gaussian, unclipped.
- Hiss: white with a flicker corner, two low shelves (15/5 Hz and 160/80 Hz),
  within 0.64 dB of the op-amp model 1 + 100 Hz/f over 20 Hz–20 kHz. Alternative
  corners 50 or 200 Hz.
- Level: both sources keep the audio-band power (20 Hz–20 kHz) the previous
  circuit engine had at a 48 kHz host (uniform white, variance 1/3, over
  0–48 kHz); a software calibration, not volts. The previous sources were white
  per internal sample, so their audible power fell with host rate and factor; the
  new ones are defined as densities and do not.
- Realtime ([`src/core/NoiseSource.h`](../../src/core/NoiseSource.h)): Marsaglia
  polar Gaussian on a 32-bit xorshift, variance N0·fs/2; first-order sections
  with exactly matched poles, the low-pass zeros placed to match the analog
  magnitude at min(20 kHz, 0.4 fs) (within 0.03 dB from 88.2 kHz).

### Measured statistics (`cs80_noise_tests`)

| Internal rate | Panel PSD worst 1/3-oct error | Panel RMS | Panel kurtosis | Hiss PSD worst error | Hiss RMS | Hiss kurtosis | Hiss L/R correlation |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 88.2 kHz | +0.47 dB @ 126 Hz | 0.3748 | 3.010 | -0.23 dB @ 800 Hz | 0.5523 | 3.007 | +0.0007 |
| 96 kHz | +0.43 dB @ 63 Hz | 0.3755 | 3.004 | +0.20 dB @ 159 Hz | 0.5761 | 3.003 | +0.0006 |
| 176.4 kHz | +0.31 dB @ 79 Hz | 0.3756 | 3.006 | +0.46 dB @ 63 Hz | 0.7786 | 3.003 | +0.0004 |
| 192 kHz | +0.26 dB @ 318 Hz | 0.3751 | 2.999 | +0.40 dB @ 63 Hz | 0.8123 | 3.003 | +0.0006 |
| 352.8 kHz | +0.55 dB @ 63 Hz | 0.3752 | 2.996 | +0.57 dB @ 63 Hz | 1.0995 | 3.003 | +0.0005 |
| 384 kHz | +0.24 dB @ 63 Hz | 0.3748 | 3.000 | +0.34 dB @ 63 Hz | 1.1468 | 3.003 | +0.0001 |

White Gaussian generator (2^20 samples): mean +0.00151, sigma 0.99945, kurtosis 3.0041.
Two equal voices versus one, panel noise only: circuit engine x2.00000 (coherent: one shared generator), legacy x1.4193 (independent draws, sqrt 2 = 1.414).

Method: 17 Welch segments of the smallest power of two above fs/1.4 samples
(bins below 1.4 Hz), Hann, 50 % overlap; 1/3-octave bands 63 Hz–16 kHz compared
with the analytic PSD averaged over the same bins. Limits (registered in
`fa9b8ec`): ±1.0 dB per band, band power 100 Hz–10 kHz within 0.5 dB across
rates, |kurtosis − 3| ≤ 0.1, |mean| ≤ 0.01σ, |ρ(L,R)| ≤ 0.02. The full-band
RMS of the hiss grows with the internal rate because it is white up to fs/2;
its audio-band power, the part the decimator passes, is constant.

Mutation checks: unshared panel noise (per-voice draws in the circuit engine)
caught by the fan-out ratio; σ not scaled with the rate caught (PSD and rate
independence); uniform instead of Gaussian samples caught (kurtosis, PSD); the
3 kHz stage removed caught (PSD). For the listening gate: GENERAL routed to the
LEFT node caught (exactness and level); a non-inverting headphone gain caught
(response, static transfer, level).

## Phase 6 host parameter groups

**State: done.** Sample-accurate automation: **not done** (see below).

### Implementation

`createParameterLayout()` now builds a `juce::AudioProcessorParameterGroup` tree.
JUCE flattens a group depth-first in insertion order
(`AudioProcessor::addParameterGroup` → `getParameters(true)`), so grouping keeps
the flat order only when every group is a **contiguous run** of the shipped
order. The tree is therefore built from contiguous runs only:

| Top-level group | Host indexes | Contents |
| --- | --- | --- |
| Global | 0–61 | 12 subgroups, one per contiguous `ParamDef.group` run: MASTER, PERFORMANCE, SUB OSC, RING MOD, CHORUS, MASTER, VOICE, PERFORMANCE, MODULATION, FX, ARP, MACROS |
| Channel I, Channel II | 62–91, 92–121 | the 30 channel controls, flat |
| Programmer | 122–227 | the two tone selectors, then subgroups Memory 1–4 |
| SUB OSC, KEYBOARD, FOOT, MASTER, ENGINE | 228–244 | the shipped extension runs, named by `ParamDef.group` |
| Engine I/O | appended (245–246 here) | OVERSAMPLING, LISTENING OUTPUT |

Why some `ParamDef.group`s are split or not used:
- MASTER (0–6 and 29) and PERFORMANCE (7–12 and 37–38) are not contiguous in the
  shipped global order, so each becomes two host groups with the same name
  (unique IDs `global_N`). Merging them would reorder host indexes.
- Inside a channel block VCO and VCA interleave (VCO, VCA, VCO, …, VCA), so per-row
  subgroups would split VCO twice and VCA four times; each channel is one group.
- Other workstreams' parameters appended after this block stay top-level unless
  they group their own contiguous blocks; that keeps their order too.

IDs, indexes, AU version hints and the APVTS state (a flat PARAM list) are
unchanged; the existing UI self-test order checks (0–121, sources, memories,
228–244) pass unchanged.

### Tests

`tests/EngineHostTests.h` `testParameterGroupsHost`: the tree flattens to exactly
`getParameters()`; the first 245 host IDs equal an independently built list
(core 0–121, sources, memories, extensions); the top-level names; every shipped
parameter is grouped; each Global subgroup's members carry its `ParamDef.group`;
the saved state stays a flat PARAM list.

### Sample-accurate automation: skipped

Not done in this workstream. Parameters are read once per host block (as before)
and the circuit engine now smooths every continuous control over 5 ms. That
removes the audible block-rate zipper, but not the up-to-one-block timing jitter
of automation points. JUCE's `AudioProcessor` API delivers parameter values per
block (its VST3 wrapper applies each queue's last point at the block start), so
true sample-accurate automation needs either wrapper-level access to the queues
or host block splitting. `processBlock` is also edited by ws3, ws6 and ws8, so
this belongs to the post-integration pass. Open item.

## New parameters

| Core enum (Params.h) | Host ID | Range | Default | Host type | AU hint | Preset behavior |
| --- | --- | --- | --- | --- | --- | --- |
| `P_oversampling` | `oversampling` | 0–2: Eco (2x), Standard, HQ (8x) | 1 Standard | Choice "Oversampling quality" | 9 | session setting: factory and user presets keep the live value |
| `P_outputJack` | `output_jack` | 0–2: Left/Right jacks, General (mono) jack, Headphones | 0 Left/Right | Choice "Listening output" | 9 | physical setup: factory and user presets keep the live value |

Renamed, IDs/indexes/hints unchanged: `parity_mode` → host name "PURE CS-80"
(core name "PURE CS-80"); `source_performance` → host name "Engine", value text
"Circuit"/"Legacy (compat)", host default on (core name "ENGINE: CIRCUIT", core
default unchanged off).

## Entries for HARDWARE_COVERAGE.md (2026-10-31)

- **2 Main volume** — 2026-10-31 (ws2): circuit engine smooths VOLUME with the
  declared 5 ms one-pole (was a 0.003 per-sample constant, rate dependent).
  Legacy unchanged.
- **19 Ribbon / performance controllers** — 2026-10-31 (ws2): circuit engine
  smooths ribbon, bend, CC1/CC74 and poly/channel pressure over 5 ms (declared);
  a new note starts from its own touch. Legacy unchanged.
- **17/18 Foot controller EXP (volume)** — 2026-10-31 (ws2, ws4 hand-off):
  circuit engine routes the expression gain through the PRA I2/I3 optical cell
  (ws4's declared LDR law: 12 ms brightening, 60 ms darkening in log R; static
  gain unchanged). Open: the networks' own steady-state law and polarity, VR11/12
  trims. Legacy keeps the immediate gain.
- **25 White noise level** — 2026-10-31 (ws2): circuit engine uses ONE SUB29
  generator for every voice (source-derived fan-out C1/C2 → NO1/NO2 → NI43),
  with a declared Gaussian PSD from the SUB29 component values (33.9 Hz and
  1.6 Hz high-passes; 14.0 kHz and 3.0 kHz gain-bandwidth corners from an
  inferred 3 MHz NJM4558-class GBW), level-matched to the previous audio-band
  power and rate independent. Open: generator amplitude, rail clipping, exact op
  amp type.
- **GENERAL mono output** — 2026-10-31 (ws2): LISTENING OUTPUT = General makes
  the existing GENERAL node (0.47 active sum before coupling) audible on both
  host channels in the circuit engine. Open: active-stage gains and trims.
- **Headphone jack/amplifier** — 2026-10-31 (ws2): optional PHONES listening
  model: HL/HR before coupling, −1.5 inverting stage (SUB29 100 k/150 k), 22 Ω
  into an inferred 32 Ω load, direct-wired (Δ) with the 470 µF variant
  implemented, rail limit shared with the preamp driver. Open: current limiting,
  crossover, installed jack arrangement.
- **Output amplifiers and coupling networks** — 2026-10-31 (ws2): ANALOG NOISE
  hiss is a declared mix-bus op-amp PSD (white + ~100 Hz flicker corner), one
  independent source per channel, rate independent.
- **Engine / numerical backend** (new software row) — 2026-10-31 (ws2):
  OVERSAMPLING Eco/Standard/HQ (2x/4x/8x at 44.1/48 kHz), offline HQ for the
  circuit engine, 63-sample latency in every mode; fresh instances default to
  the circuit engine (schema 3).

## Open items

- Re-measure `kStandardOversampling` when ws1's oscillator lands (the −70 dB alias
  target is not reachable with the PolyBLEP oscillator at any factor here).
- CPU numbers are from a shared, heavily loaded container; repeat on a quiet
  machine with `cs80_engine_audit --suite cpu`.
- The editor's INIT button still calls `loadFactoryPreset(<Init Patch index>)`
  (legacy); ws8 should call `Cs80AudioProcessor::loadInitPatch()` so the INIT
  action is a circuit-engine new preset.
- Sample-accurate automation (Phase 6 optional) not done.
- The SUB NOISE function (ws4) could read `CircuitNoiseSources::panel()` so it
  shares the generator, as SUB29 shows.
- Per-voice device noise not modelled (declared).
- The EXP volume optical cell reuses the wah cell's law; the I2/I3 networks' own
  steady-state law, polarity and VR11/VR12 trims are unrecovered.

## Merge with the integration branch (ws4, ws7)

Merged `claude/adoring-fermi-alhg4x` at `bd524f9` (commit `1327147`; no textual
conflicts). Semantic follow-ups:
- ws4's new `PraOpticalCellTests` engine trajectory assumed two internal ticks per
  host frame; it now uses the engine's factor. ws4's `Ig00150SubTests` band-limited
  replay hard-codes 88.2 kHz; it pins Eco. Both are commented.
- ws7's golden-image fixture pins Legacy (see 1.7 Tests); the goldens are
  unchanged.
- ws4 hand-off done (see "EXP volume optics"): the circuit engine's expression
  gain is the optical cell, legacy byte-identical, both expression oracles
  updated (`tests/ExpressionOpticsOracle.h`). Mutation check: restoring the
  immediate gain fails both oracles (96 321 / 81 303 failing checks).
- My smoothing bank covers the ring, chorus and SUB fields ws4's circuits read,
  so their panel controls are smoothed too; ws4's own speed smoothing is unchanged.

## For the integrator

- **Re-pin `SOURCE_AUDIO_V2`** after merging: intentional circuit-engine changes
  are Standard = 4x, the 5 ms control smoothing, the shared SUB29 noise and
  declared PSDs, and the optical expression gain. Static circuit renders without
  noise (Eco, no mid-render control moves) are byte-identical to `f6b126d`.
- **Fixtures whose oracle ticks at 2x** must pin Eco (`P_oversampling = 0`) or
  read `Engine::getOversamplingFactor()`; any workstream test written against the
  old circuit-engine 2x will fail under Standard 4x.
- **Fresh instances are now circuit engine.** Plugin tests that assume a legacy
  fresh instance must call `setEngineMode(EngineMode::legacy)`. The panel goldens
  are pinned to Legacy; any newly recorded golden should state which engine it
  shows.
- `tests/test_parity_capture.py` now accepts appended core parameters (count
  >= 139); other workstreams that append core parameters need this too.
- Hooks: ws8's INIT button → `Cs80AudioProcessor::loadInitPatch()`; ws8's
  toolbar ENGINE / PURE CS-80 switches → `setEngineMode` / `getEffectiveEngineMode`
  / `setPure` / `isPure`; quality and listening controls → the `oversampling` and
  `output_jack` host parameters (presets keep them). ws1 → `kStandardOversampling`
  in `src/core/Oversampling.h`. ws4 (optional) → `CircuitNoiseSources::panel()`
  for the SUB NOISE function.
- `Engine::getLatencySamples()` is 63 in every mode; the processor re-reports it
  after any change.
