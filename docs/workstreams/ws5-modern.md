# ws5-modern — modern modulation and FX layer (Phase 6)

Branched from integration commit `6719ce4`. Scope: [IMPROVEMENT_PLAN.md](../IMPROVEMENT_PLAN.md)
Phase 6 "Modulation sources", "Modulation routing" (matrix and keyboard modulation page) and
"FX", plus their state, host parameters and UI pages. Goal: a modern layer at least equal to
Arturia CS-80 V4's, **inert under PURE CS-80** and **neutral by default**.

Everything in this workstream is a modern software extension. None of it is a CS-80 circuit,
and no number below is a CS-80 measurement. Where a law has an outside source it is cited;
every other constant is a **declared engineering inference** with the alternatives listed.

## Summary

| Step | State | Where |
| --- | --- | --- |
| 1a. Three MSEGs: loopable, tempo-synced (host BPM, 120 BPM fallback), curve per segment | done | `src/core/Mseg.h`, `src/reference/MsegReference.h` |
| 1b. LFO waveforms beyond sine (sine stays default and byte-identical) | done | `src/core/ModulationLfo.h` |
| 2a. 16-slot matrix: bipolar amount, optional via, rank target, one offsets struct | done | `src/core/ModMatrix.h`, `src/core/ModernLayer.h`, Engine hooks |
| 2b. Keyboard modulation page (engine side): velocity, aftertouch, wheel, key curves | done | `responseCurve` in `src/core/ModMatrix.h` |
| 3a. Gain-compensated FX chain (in/out loudness matched) | done | `src/core/FxLoudness.h`, `src/core/FxSlot.h` |
| 3b. Better plate (Dattorro) and tape (transport-law wow and flutter) | done | `src/core/FxModels.h` |
| 3c. BBD ensemble and spring reverb | done | `src/core/FxModels.h` |
| 3d. Per-slot oversampling for the drive (1x/2x/4x/8x) | done | `src/core/FxOversampler.h`, `DriveFx` |
| 4. State (versioned child ValueTree), host parameters (AU hint 12) | done | `src/plugin/ModernLayerState.*`, `createParameterLayout()` |
| 5. UI pages (MSEG editor, LFOs, matrix, keyboard curves, FX) and UI self-test | done | `src/plugin/ModernModulationUi.*`, `tests/ModernLayer*Tests.h` |

**Compatibility.** Before the merge of the integration branch, `legacy default`, `legacy
tails`, `v1 circuit numerics` **and** `v2 circuit-engine audio` were byte-identical to their
pins after every ws5 commit (`scripts/check-compat-gates.sh`). After merging `bd524f9` (ws4 +
ws7), the v2 gate differs from its pin because of ws4's documented circuit-engine changes; the
legacy and v1 gates stay byte-identical, and the merged tree's v2 and legacy probe output is
byte-identical to the integration commit `bd524f9` itself (scratch script
`v2_vs_integration.sh`: `git archive bd524f9`, same probes). This workstream makes **no
intentional circuit-engine sound change**: nothing to re-pin for ws5.

**Pre-registration.** All acceptance tolerances are in `tests/ModernLayerTolerances.h`,
committed first (`de82334`) before any realtime model existed. None was changed afterwards.

## Architecture and hook points

The engine knows the layer through one member and a handful of hook lines in
`Engine::processInternalSample` (no function was split):

1. `modern.beginSample(...)` once per internal sample, after the shared pressure is formed:
   advances the shared (mono/free) MSEGs, evaluates global destinations.
2. SUB: `speed * subSpeedRatio` on every SUB path (legacy LFO, original six-function, and
   ws4's IG00150 circuit converter after the merge) and `subValue *= subDepthGain`.
3. `modern.processVoice(vi, ...)` once per active voice before its ranks: advances the poly
   MSEGs, evaluates the per-rank destinations into one `RankOffsets` per voice and rank.
4. Rank chain consumption, each **appended at the end** of the existing expression: `+ mod.pitch`,
   `+ mod.pulseWidth`, `+ mod.hpCutoff` / `+ mod.lpCutoff` (both filter paths and both policies),
   `modulatedQ(q, mod.hpQ/lpQ)`, `+ mod.level` (both level paths), `+ mod.pan`.
5. FX: the slot loop body became one `fxSlots[slot].process(controls, ...)` call; the slot's mix
   gets `+ modernGlobal.fxMix[slot]`.
6. LFOs: `modLfos[i].tick(rate, sr, global.lfoWave[i])` (was a hard-coded `0`).
7. `channelPressure()` remembers the last message for the channel-pressure source.
8. `buildSettings()`: reads the new parameters; PURE CS-80 disables the layer, forces sine and
   model 0 and turns loudness matching off.

When nothing is routed (the default) or under PURE CS-80, every offset is an exact identity
(0, or a ratio of exactly 1.0f), so `x + 0.0f` and `x * 1.0f` leave each value unchanged and
`modulatedQ` returns `q` untouched. That is why ws5 leaves every compatibility probe's output
byte-identical.

`FxSlot` moved, unchanged in behaviour, from the private part of `Engine.h` to
`src/core/FxSlot.h` so the slot can be tested on its own. Its classic effects are pinned
bit-identical to a frozen copy of the original `Engine.cpp` code (`tests/ModernFxTests.cpp`,
`testClassicByteIdentity`): no compatibility probe exercises the FX slots, so this is the
only guard on them.

## Step 1a — MSEGs (`src/core/Mseg.h`)

- Up to 32 points per shape (absolute time, level 0..1, curve of the segment it starts).
  Times are seconds when free, beats (quarter notes) when synced.
- **Curve law** (declared inference): `y = (1 - e^(-a t)) / (1 - e^(-a))`, `a = ln(1000) * c`.
  c = +1 is a single-RC charge reaching -60 dB of its span at the segment end, the same time
  law as the circuit-engine envelopes ([ENVELOPE_RC_MODEL.md](../ENVELOPE_RC_MODEL.md)); c = -1
  is its mirror. Alternatives considered: power law `t^(2^c)` (cusp at t = 0 for c > 0) and a
  quadratic Bezier bend (cheaper, but no physical meaning).
- **Loop modes:** one shot; sustain loop (loops while the gate is held; on release it jumps
  to the loop end and the next segment starts from the level reached, so an early release never
  plays the rest of the attack); loop always. Loop start = end is a sustain point.
- **Trigger modes:** poly (one envelope per voice, restarted at note-on), mono (one shared
  envelope, restarted when a key goes down with none held), free (one shared envelope that
  never restarts; when synced and the host transport plays it follows the song position).
- **Tempo sync:** host BPM from `getPlayHead()->getPosition()` each block; no or unusable
  tempo falls back to **120 BPM**. Position is double precision, so loops do not drift.
- Live shape edits are safe on running envelopes (the runner re-locates its segment).
- Rate: host parameter `msegN_rate` in octaves (-3..+3, i.e. 0.125x..8x, linear so the default
  round-trips exactly through host state) plus the matrix MSEG-rate offset, also in octaves.

**Reference:** `src/reference/MsegReference.h` computes the level after k ticks in closed
form (no stepping) in double precision, including loop folding and the release rule.

**Results** (`cs80_mseg_tests`, 185 checks; 88.2, 96, 384 kHz; 13 shapes; 3 rates):

| Gate | Tolerance | Result |
| --- | --- | --- |
| held level vs closed form | 2e-5 | pass on all shapes/rates/loop modes |
| release (6 release points per shape) | 2e-5 | pass |
| segment boundary, release jump | 1 sample | pass (exact) |
| loop drift over 100 loops | 1 sample | pass (exact) |
| synced beat length at 120/90/174 BPM, fallback | 1 sample | pass |
| transport lock position | 2e-5 | pass |

## Step 1b — LFO waveforms (`src/core/ModulationLfo.h`)

LFO 1-3 gain `lfoN_wave`: sine (0, default), triangle, saw up, saw down, square, sample &
hold, smooth random (raised-cosine interpolation between random levels, zero slope at each
period start). All shapes are phase-aligned with the sine.

Finding: the historical float phase accumulator `phase += rate / sr` is systematically biased
at low rates (rounding of a tiny increment against a coarse ulp), so its period is off by many
samples per cycle at 1 Hz / 96 kHz. Sine must stay byte-identical, so **only the sine keeps
that law**; the new shapes run on a double phase advanced in parallel and are rate-exact.
Random shapes use a private, re-seeded generator per LFO, never the shared `modRng` stream
(so the arpeggiator and SUB random patterns cannot shift).

**Results** (`cs80_modern_lfo_tests`, 93 checks): sine bit-identical to `Lfo::tick(..., 0, ...)`
over 200k samples at 4 rates x 5 LFO rates; shapes within 1e-6 of an independent analytic
definition; period error <= 1 sample over the run; S&H holds and steps at wraps; smooth random
never exceeds its slope bound; waveform inert at depth 0 and under PURE CS-80.

## Step 2a — Modulation matrix (`src/core/ModMatrix.h`, `src/core/ModernLayer.h`)

`offset[dest] += amount * source * via * scale(dest)`, per slot, with the via source
optional (1 when none) and a rank target (I+II, I or II). Enum values are saved in state:
append only.

| Sources (24) | |
| --- | --- |
| original controls tapped | ribbon, poly pressure, channel pressure, shared pressure (TRG5 diode-OR maximum of held keys), keyboard contour (KBC ladder HIGH - LOW weight, `PerformanceControls.h`), SUB output, ring-modulator envelope, filter EG and VCA EG of the rank (previous sample) |
| interface / modern | velocity, mod wheel, key follow (61-key playing surface MIDI 24..84 as -1..1, the instrument's own software convention in `PerformanceControls.h`, not the unresolved KAS map), LFO 1-3, MSEG 1-3, expression, pitch bend, CC74, note random, constant |

| Destination | Full scale at amount 1 (declared) |
| --- | --- |
| pitch | 12 semitones |
| pulse width | 0.4 duty |
| cutoff (HP+LP), HP cutoff, LP cutoff | 5 octaves |
| HP / LP resonance | 2 octaves of Q, **clamped to Q 0.5..5**, the documented IG00156 endpoints (Yamaha CS servicing guide p34; CS-80 IC sheet p42) |
| VCA level | 1 level-control unit (the panel LEVEL range) |
| pan | full width |
| SUB speed | 3 octaves (ratio), SUB depth x(1 + offset) clamped 0..2 |
| FX 1-3 mix | 1 |
| MSEG 1-3 rate | 3 octaves |

Global destinations use per-voice sources from the newest voice (last-note priority).
Filter-EG values are normalised by `kProvisionalFilterEnvelopeOctaves` (3).

Alternatives considered: a per-voice matrix evaluated at control rate (cheaper, but zipper on
pitch); multiplicative level modulation (rejected: the engine's existing level controls are
additive control sums).

**Results** (`cs80_mod_matrix_tests`, 145 checks): every destination equals
`amount * source * scale` within 1e-6 of full scale; via multiplies; slots add; rank targets
isolate; each rank reads its own EG; key follow and contour map as specified; global
destinations follow the newest voice; zero amount / no source / no destination / disabled are
exact identities and removing the last route returns to identity at once. Engine level: pitch
and LP-cutoff offsets reach `RankInfo` exactly (44.1..192 kHz); synced mono MSEG follows 90 BPM
and the 120 BPM fallback through the engine within 2 samples; mono restarts only with no key
held; free never restarts; fully modulated engine finite and bounded at 44.1..192 kHz.

**Inertness:** a fully configured layer (16 routed slots, all MSEG modes, LFO waves, curves,
FX types) with zero amounts is byte-identical to defaults in Legacy, Source Performance and
PURE; with full amounts it is byte-identical to defaults under PURE CS-80 and audible in the
other two.

## Step 2b — Keyboard modulation page (engine side)

`responseCurve(x, s) = x^(2^(-2s))`: s = 0 linear (exactly the input), +1 soft (x^1/4), -1 hard
(x^4); key follow uses the odd extension. Curves shape the **matrix sources only**
(velocity; poly, channel and shared pressure; mod wheel; key follow). They are an
interface-layer adapter for different MIDI controllers, labelled as such, and never touch the
original initial/after-touch paths (TWS/TRG), which belong to ws3's 2.4 touch work.
Alternatives considered: exponential (`(e^(kx)-1)/(e^k-1)`) and S-curves; the power law keeps
both ends fixed and inverts exactly (s and -s are inverses).

## Step 3 — FX (`src/core/FxSlot.h`, `FxModels.h`, `FxOversampler.h`, `FxLoudness.h`)

Each slot gains `fxN_model` beside the classic `fxN_type`: 0 classic (the type is used,
arithmetic unchanged), 1 BBD ensemble, 2 spring reverb, 3 plate reverb, 4 tape echo, 5 drive.
Two model controls `fxN_param_a/b` and `fxN_oversampling` (drive). Mix laws: equal power for
the decorrelated reverbs/echo, linear for the correlated ensemble and drive.

| Model | Design source | A / B |
| --- | --- | --- |
| BBD ensemble | three-phase multi-tap string-ensemble style (declared): 3 taps per channel at 6 ms, slow 0.63 Hz +/-2.2 ms and fast 5.7 Hz +/-0.35 ms sweeps, 120 deg tap spacing, right taps 60 deg off, 9 kHz anti-alias/reconstruction, soft headroom | rate 0.5x..2x / depth |
| Plate | J. Dattorro, "Effect Design Part 1", JAES 45(9) 1997: published tank lengths (29761 Hz, rescaled), diffusion 0.75/0.625/0.70, decay diffusion 2 = decay + 0.15 clipped to 0.25..0.5, modulated tank all-passes, seven-tap stereo outputs | RT60 0.8..8 s at 1 kHz / brightness |
| Spring | Valimaki, Parker and Abel, "Parametric spring reverberation effect", JAES 58(7/8) 2010 (and Abel et al., AES 121, 2006): 2 springs, 48 stretched all-passes (a = -0.6, K = fs / (2 x 4.3 kHz)), transit 37.1 / 43.9 ms, loop low-pass at fC. HF chirp path omitted (declared simplification) | RT60 0.8..4.8 s at 1 kHz / tone 2..8 kHz |
| Tape echo | transport law: with speed v0(1+s(t)) the read delay solves `integral[t-D, t](1+s) = T`, so `D = T - (A/w)[cos(w(t-T)) - cos(w t)]` per sinusoidal speed component (pitch error s(t) - s(t-T), as on a machine); wow 0.55 Hz / 0.18 %, flutter 6.1 Hz / 0.04 % (declared); pre/de-emphasis, tape saturation, head loss and loop high-pass in the loop (loop gain never above the repeat gain), head bump on the output only; ping-pong; time glides like a speed change | time 60 ms..1.2 s / repeats as echo-train T60 0.3..8 s |
| Drive | asymmetric tanh, `[tanh(gx+b) - tanh(b)] / (g sech^2 b)` (unity small-signal gain), b = 0.2; 10 Hz DC blocker; tone LPF; built-in K-weighted loudness follower | drive 0..30 dB / tone 1.5..18 kHz |

**Oversampling:** polyphase IIR half-band stages (two first-order all-pass chains in z^-2),
coefficients from the elliptic half-band design used by L. de Soras' HIIR (after Valenzuela and
Constantinides, IEE Proc. 1983), transition 0.1 fs, 100 dB target: 6 coefficients per stage.
1x/2x/4x/8x of the internal rate (which is already 2x host). No latency is reported (group
delay of a few samples).

**Loudness:** ITU-R BS.1770-4 K-weighting re-derived for any rate from the analogue prototype
(libebur128 formulation); it reproduces Annex 1 Tables 1-2 at 48 kHz to 1e-6.
- *Static calibration:* each model's wet path carries a measured 5 x 5 table (A, B) of trims so
  that at full mix pink noise at -18 dBFS RMS keeps its K-weighted loudness
  (`cs80_modern_fx_tests --calibrate` regenerates the tables; they are a calibration of the
  models' own output, not a measurement of any instrument).
- *FX LOUDNESS MATCH* (`fx_gain_comp`, default off): per-slot matcher, 1 s K-weighted
  energies of slot input and output, gain sqrt(Ein/Eout) +/-18 dB, 150 ms slew, held while the
  input is more than 20 dB below its recent peak or under -70 dBFS (so tails do not pump).
  Works for classic types too. Default off keeps every old session byte-identical.

**Results** (`cs80_modern_fx_tests`, 241 checks; tool output captured 2026-10-31):

| Gate | Tolerance | Measured |
| --- | --- | --- |
| K-weighting vs BS.1770-4 Tables 1-2 (48 kHz) | 1e-6 | pass; response rate-independent 44.1..384 kHz |
| half-band up/down cascade 20 Hz..20 kHz (1-3 stages) | 0.1 dB | pass |
| up-sampler image rejection (1, 10, 20 kHz) | <= -90 dB | pass |
| drive alias ratio, 4.1 kHz, -6 dBFS, drive max, 96 kHz: 1x / 2x / 4x / 8x | - / -45 / -70 / -80 dB, >= 10 dB per doubling | **-26.5 / -47.1 / -87.6 / -112.0 dB** |
| 4x harmonics 1-4 vs 64x double truth | 0.5 dB | within 0.01 dB (-27.57, -65.56, -37.23, -65.73 dB) |
| static loudness, 5 models x {0.1,0.5,0.9}^2 | 1.5 dB | worst -0.22 dB (tape 0.9/0.9); ensemble <= 0.12, spring/plate <= 0.05, drive <= 0.02 |
| matched loudness, 10 types x mix {0.5, 1} x {-30,-18,-6} dBFS | 0.5 dB | -0.03 .. +0.05 dB |
| plate RT60 at 1 kHz (a = 0.1/0.5/0.9; 88.2/96/192 kHz) | 25 % | 1.11/1.01, 2.65/2.53, 6.46/6.35 s (+10, +5, +2 %) |
| spring RT60 at 1 kHz | 25 % | 0.94/0.96, 1.87/1.96, 3.67/4.01 s (-2, -5, -9 %) |
| spring dispersion (400 Hz energy later than 3 kHz) | - | pass |
| first tape echo at the set time (60 ms, 198 ms, 1.2 s) | 2 % | pass; repeat gain follows T60 within 1 dB |
| stability: every model at max, 6 s full-scale bursts + 12 s silence, 44.1/48/96/192 kHz | peak <= 8, tail <= -60 dB | peaks 1.9..5.4; tails -91..-880 dB |
| classic slot effects vs frozen original code (3 rates x 5 types x 2 mixes) | bit-identical | pass |
| zero mix, PURE CS-80 | byte-identical | pass |

Before -> after for the existing classic slot effects at full mix (pink noise -18 dBFS RMS,
K-weighted, 96 kHz, scratch tool `classic_loudness.cpp` on `FxSlot`): chorus -2.29 -> 0.00,
phaser -1.55 -> -0.09, tape delay -3.82 -> +0.01, plate -8.14 -> 0.00, drive +5.95 -> 0.00 dB
with FX LOUDNESS MATCH on (off remains the historical behaviour).

Model fixes made while evaluating (the tolerances were not touched): the plate first measured
0.74 s for a 0.54 s target at short decay; Dattorro's decay-diffusion-2 rule had been left out and
the fixed 0.70 tank all-passes ring for about 0.4 s by themselves, so the rule was added and the
decay range now starts at 0.8 s, which the structure can honour. The tape repeat check first used
a 3 kHz burst that the in-loop head-loss filter attenuates; the law holds in the passband, so the
check uses 500 Hz.

## Step 4 — State and host parameters

`src/plugin/ModernLayerState.*` keeps the MSEG shapes and matrix routing as a versioned child
of the APVTS state:

```xml
<MODERN_LAYER version="1">
  <MSEG index="0" points="0 0 0.5;0.08 1 0.6;..." loopStart="2" loopEnd="2" loop="1" trigger="0" sync="0" bipolar="0"/>
  ...
  <MATRIX><SLOT index="0" source="1" via="0" destination="1" target="0"/> ...</MATRIX>
</MODERN_LAYER>
```

- It travels with `getStateInformation`, user presets and (when ws8 adds one) the APVTS
  `UndoManager` without further hooks; edits use `apvts.undoManager` and replace the child in
  one step, so one edit is one transaction.
- A fresh instance writes no child until edited; a state without the child (every session saved
  before this layer) restores the defaults, which are inert until an amount is raised. A later
  `version` loads every field it understands; malformed values are sanitised.
- The listener sits on `apvts.state` itself, because `replaceState` reports the redirection only
  to listeners of the assigned object.
- Threads: edits and restores from any non-audio thread; the audio thread takes the newest
  snapshot with a `SpinLock` try-lock (never waits) in `applyToEngine`, which also passes the host
  transport and publishes the MSEG playhead for the editor.
- Factory-preset recall (`applyValues`) leaves the routing in place but resets every amount to 0,
  so the layer is inert after it. Whether factory recall should also clear the routing is an
  integrator/ws6 decision.

### New parameters (core enum `[ws5-modern]` block, host AU version hint 12)

All are `AudioParameterFloat` (like the existing LFO/FX selectors, and because the UI self-test
counts the shipped Bool parameters exactly); discrete ones display names.

| id | range | default | notes |
| --- | --- | --- | --- |
| `lfo1_wave` .. `lfo3_wave` | 0..6 | 0 (sine) | Sine, Triangle, Saw up, Saw down, Square, S&H, Smooth random |
| `mseg1_rate` .. `mseg3_rate` | -3..+3 oct | 0 (1x) | speed 2^value; shown as a multiplier |
| `mod1_amount` .. `mod16_amount` | -1..1 | 0 | bipolar slot amounts |
| `mod_velocity_curve`, `mod_pressure_curve`, `mod_wheel_curve`, `mod_key_curve` | -1..1 | 0 (linear) | keyboard modulation page |
| `fx1_model` .. `fx3_model` | 0..5 | 0 (classic) | Classic, BBD ensemble, Spring, Plate, Tape echo, Drive |
| `fx1_param_a` .. `fx3_param_a`, `fx1_param_b` .. `fx3_param_b` | 0..1 | 0.5 | per-model meaning (see table above) |
| `fx1_oversampling` .. `fx3_oversampling` | 0..3 | 2 (4x) | drive only; 1x/2x/4x/8x of the internal rate |
| `fx_gain_comp` | 0..1 | 0 (off) | FX LOUDNESS MATCH, all slots |

Order in the enum: lfo waves, MSEG rates, 16 amounts, 4 curves, then per slot model/A/B/
oversampling, then `fx_gain_comp` (39 parameters). The parity-capture metadata test pinned
exactly 139 parameters; it now checks the append-only invariant (`>= 139`, count equals the
settings list), which every workstream that appends needs.

## Step 5 — UI pages (`src/plugin/ModernModulationUi.*`, namespace `cs80::ui::modern_layer`)

Constructors take `Cs80AudioProcessor&` (APVTS attachments plus `getModernLayer()`):

| Component | Content |
| --- | --- |
| `MsegEditor` | drag points (Shift fine), drag a segment to bend it, double-click to add/delete, S/E loop flags that snap to points, Delete key, playhead, seconds/beats grid |
| `MsegPage` | MSEG 1/2/3 selector, editor, loop mode, trigger mode, Sync, Bipolar, rate slider, shape presets |
| `LfoPanel` | per LFO: waveform (new) plus the existing rate, depth and direct destination |
| `ModMatrixPage` | 16 rows: source, via, destination, rank, amount (double-click resets) |
| `KeyboardModulationPage` | four curve sliders with live curve plots |
| `ModernFxPage` | per slot: model, classic type, mix, A/B (labels follow the model), oversampling; loudness match |
| `ModernModulationPage` | the Studio "Modulation" tab: MSEG page above, LFOs and a scrolling matrix below |

PluginEditor.cpp and AdvancedPanel.cpp are untouched; ws8 mounts the pages. Type uses ws7's
embedded panel typeface (`cs80::art::displayFont`, Barlow Condensed) with the 10.5 px screen
legend floor; colours come from `UiComponents.h`. The pages are not mounted in the main view,
so ws7's golden images are unaffected.

**UI self-test** (`tests/ModernLayerHostTests.h`, `tests/ModernLayerUiTests.h`, appended as the
phase "modern modulation and FX layer" of `CS80Snapshots --self-test`):

- host: fresh instance writes no state; every ws5 host parameter has AU hint 12 and its declared
  range/default; named values (`lfo1_wave` 4 = "Square", `fx2_model` 3 = "Plate reverb"); state
  round trip; a state without the child restores defaults over an edited instance; a version-9
  state with unknown fields loads; malformed points/indices/enums are sanitised; user presets
  carry the layer; routing + amount changes the audio through `processBlock`; PURE CS-80 render
  unchanged by routing and amounts; synced MSEG at 90 BPM and the 120 BPM fallback within 0.01
  beat (the playhead is published at block start, so it is one block old).
- UI: `ModernModulationPage` (1100 x 640 and 1560 x 820), `KeyboardModulationPage`
  (1100 x 360), `ModernFxPage` (1100 x 380) construct and paint; every control has bounds inside
  its page; construction and painting write no APVTS state; dragging a point, bending a
  segment, double-click add/delete, the loop-end flag and the Delete key edit the session state;
  matrix combos and slider write routing and amounts; external routing changes and a state
  restore refresh the open pages; keyboard, FX model and loudness-match controls write their
  parameters. Snapshots: `build/ui-smoke/modern-*.png`.

Results: `ctest --preset linux-release -R "ui_smoke|locale"`: cs80_ui_smoke and the four
locale tests pass (5/5).

## Mutation checks

Every gate was mutation-checked: the old or wrong behaviour was restored temporarily, the test
rebuilt and run, then the file restored (runner scripts kept outside the repo). The UI/state
mutations ran the full UI smoke under `scripts/run-ui-smoke-linux.sh`; one run hit the wrapper's
environment exit 70 (Openbox did not start on the shared machine) and was repeated.

| Gate | Mutation | Result |
| --- | --- | --- |
| LFO sine byte-identity | sine on the double phase | detected (20 failures) |
| LFO shapes | triangle not phase-aligned | detected |
| LFO period | shapes on the historical float phase | detected (9 failures): confirms the float-law rate error |
| MSEG release | no level continuity | detected |
| MSEG loop drift | wrap drops the remainder | detected |
| MSEG curve | exponent 6 instead of ln 1000 | detected |
| MSEG sync | ignores host tempo | detected |
| MSEG release | does not leave the loop | detected |
| MSEG timing | one-sample output lag | detected |
| matrix via | via ignored | detected |
| matrix target | rank target ignored | detected |
| matrix global | first voice instead of newest | detected |
| PURE CS-80 | layer not disabled | detected |
| key follow | centred on MIDI 60 | detected |
| consumption | LP cutoff offset not consumed | detected |
| Q range | clamp removed | detected |
| default invariance | idle consumer adds 1e-6 semitone | equivalent mutant (below half an ulp at MIDI 60) |
| default invariance | idle consumer adds 1e-4 semitone / pan 1e-5 | survives the in-tree test by construction (both renders carry it); **detected by `check-compat-gates.sh`** (legacy default/tails DIFFER, v2 differs) |
| K-weighting | shelf gain 4.5 dB | detected |
| half-band | down-sampler polyphase order swapped | detected |
| drive oversampling | factor ignored | detected |
| classic FX | tape feedback 0.36 -> 0.37 | detected |
| static loudness | calibration not applied | detected |
| loudness match | gain not applied | detected |
| plate RT60 | decay law exponent | detected |
| spring dispersion | all-pass sign flipped | detected |
| tape timing | delay 5 % long | detected |
| PURE FX | model not zeroed | equivalent (PURE also zeroes the mix) |
| PURE FX | model and mix not zeroed | detected |
| FX tails | tape loop gain 1 | detected |
| state restore | `valueTreeRedirected` ignored | detected (round trip, future version, user preset, page refresh) |
| state to audio | published configuration never applied | detected (audio routing, host tempo) |
| serialisation | segment curves not written | detected (round trip, future version, user preset) |
| MSEG editor | point drag not committed | detected |
| matrix page | external changes ignored | detected (external routing, state restore refresh) |

## Dated entries for HARDWARE_COVERAGE.md (2026-10-31)

These rows are modern extensions; none changes an original-control row.

- 2026-10-31 (ws5-modern) **Modern LFO waveforms**: sine/triangle/saw up/saw down/square/S&H/
  smooth random; sine is the historical law, byte-identical. Software extension; inert under PURE.
- 2026-10-31 (ws5-modern) **MSEG 1-3**: 32-point drawable envelopes, RC curve law, sustain/
  always loops, poly/mono/free, host-tempo sync (120 BPM fallback). Software extension; PURE inert.
- 2026-10-31 (ws5-modern) **Modulation matrix (16 slots)**: taps ribbon, pressure (poly/channel/
  TRG5 shared), keyboard contour, SUB, ring envelope and both EGs as sources without changing
  their original circuits; Q modulation clamped to the documented 0.5..5. PURE inert.
- 2026-10-31 (ws5-modern) **Keyboard modulation curves**: interface-layer response curves for
  matrix sources only; the TWS/TRG touch paths are unchanged.
- 2026-10-31 (ws5-modern) **Modern FX**: BBD ensemble, Dattorro plate, dispersive spring, tape
  echo, oversampled drive, K-weighted loudness calibration and matching. The original OE1/OE2
  chorus/tremolo and PRA rows are unaffected; classic slot FX bit-identical. PURE inert.

## Files touched outside the ownership list

- `src/core/Dsp.h`: `DelayLine::readHermite`, `tap`, `capacity` added (no existing code changed).
- `src/core/Engine.h` / `Engine.cpp`: the hooks above; `FxSlot` moved to `src/core/FxSlot.h`;
  `Lfo modLfos[3]` became `ModulationLfo` (same reset/tick arithmetic for sine).
- `src/core/Params.h`: `[ws5-modern]` enum and ParamDef blocks only.
- `src/plugin/PluginProcessor.h/.cpp`: include, `modernLayer` member after `apvts`,
  `getModernLayer()`, one `applyToEngine` line in `processBlock`, the `[ws5-modern]` host block.
- `src/tools/ui_snapshot.cpp`: two includes and one appended phase before the pass/fail print.
- `tests/test_parity_capture.py`: the 139-parameter pin relaxed to the append-only invariant.
- `CMakeLists.txt`: tests and plugin sources below the ws5 markers.

## Integration notes

- Mount `ModernModulationPage` (Modulation tab), `KeyboardModulationPage` (Keyboard/MPE/Tuning
  tab) and `ModernFxPage` (FX tab) in the Studio view; or the finer `MsegPage`, `LfoPanel`,
  `ModMatrixPage`. Pages are self-sizing; tested at 1100 x 640 and 1560 x 820.
- If ws8 gives the APVTS an `UndoManager`, MSEG and matrix edits become undoable automatically
  (the editor calls `beginNewTransaction` at mouse-down).
- ws2 (smoothing): the FX slot mix is now `SlotControls::mix` built in the FX loop; a smoother
  belongs on `global.fxMix` before `+ modernGlobal.fxMix`. The modern offsets are already
  per-sample.
- ws1/ws3 editing the rank chain: keep the `+ mod.*` terms at the end of their expressions (or
  move them with the expression); they must stay exact identities when unrouted.
- ws4 (SUB, merged): the SUB-speed ratio multiplies the target of the IG00150 circuit converter
  too, and the depth gain scales the SUB output after every SUB path. The ring-envelope source
  reads `ringEnv`, which ws4's ring law still produces from the ring AD envelope.

## CPU (2026-10-31, shared 4-core build machine under load; relative numbers only)

Scratch tools `modern_bench.cpp` / `idle_bench.cpp` (kept outside the repo): 8 held voices,
Legacy, 48 kHz host, best of 3-5 runs of 4 s, real-time factor (render time / audio time).

| Case | RTF |
| --- | --- |
| idle layer, integration `bd524f9` without ws5 | 0.2345 / 0.2241 (two runs) |
| idle layer, ws5 merged tree | 0.2338 / 0.2406 (two runs): no difference above the run-to-run noise |
| all 16 slots live, 3 poly MSEGs with curved segments (worst case) | 0.375 (+0.14) |
| one slot: BBD ensemble / spring / plate / tape / drive 4x | 0.248 / 0.256 / 0.258 / 0.262 / 0.262 |
| classic plate, without / with loudness match | 0.235 / 0.240 |

The worst matrix case is dominated by per-sample MSEG curve evaluation (an `exp` per curved
segment per voice) and 16 x 2 x 8 slot evaluations; typical patches route 2-4 slots. A control-rate
matrix or incremental curve stepping is the obvious lever for the post-integration CPU pass.

## Open items

- The MSEG playhead telemetry shows the newest voice only.
- LFO tempo sync is not offered (a synced looping MSEG covers it); could be added as a
  parameter.
- The spring model omits the paper's high-frequency chirp path.
- CPU of the fully routed worst case (+0.14 RTF at 8 voices), see above.
