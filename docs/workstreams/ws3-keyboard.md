# ws3-keyboard — voices, keyboard, touch, tuning and voice modes

Workstream report for IMPROVEMENT_PLAN Phase 2.1–2.4 plus the Phase 6 tuning,
MPE and voice-mode items. Branch base: integration commit `6719ce4`. Every law
below is either cited to a source page already inspected in this repository's
evidence documents or labelled **declared inference** with its alternatives.
Nothing here is a hardware measurement. Primary-source images were not
re-downloaded in this session: `www.therogoffs.com` is refused by the session
egress proxy (HTTP 403), so all page citations are to the evidence already
recorded in `docs/*.md`.

Tolerances for every stage gate were committed before evaluation in
`tests/parity/tolerances-v2/ws3-keyboard.json` (mirrored in
`tests/Ws3Tolerances.h`), commit `test: pre-register ws3 ...`.

---

## 2.1 Free-running voices — **done**

### What changed

In the circuit engine (Source Performance and PURE CS-80/isolation) every one
of the 16 oscillator/filter chains now runs all the time. Legacy is untouched
(`scripts/check-compat-gates.sh`: legacy default, legacy tails and v1 numerics
byte-identical).

- `src/experimental/ContinuousVoiceCircuit.h`, new namespace
  `cs80::voice_continuity` (the existing kernel class is unchanged, its 151k-check
  suites still pass): the production idle path.
- `src/core/Engine.cpp`: the voice loop no longer skips a silent circuit in the
  circuit engine; `startVoice` never resets oscillator phase or filter memories
  there (it catches a silent circuit up first, `wakeIdleVoice`); `reset()` gives
  circuit-engine cold circuits declared state.
- `src/core/Dsp.h`: one additive accessor, `Svf::setState` (ws1's file; no law
  changes), needed to re-reference trapezoidal memories across clock rates.

### The cheap idle path

Clocking a silent circuit every sample was measured by the earlier experiment
at 0.19–0.23 RTF extra (docs/voice-continuity/engine-experiment); with the
Phase 1.1 Newton filter it would be higher. Instead:

1. **Oscillator phase** advances once per idle step (3 kHz, `kIdleStepHz`) by
   the sum of the per-sample increments of `Oscillator::tick`, with the
   trapezoid of the previous and present idle frequency (second-order exact
   under vibrato). It never resets.
2. **Filter memories**: both IG00156 circuit stages are clocked once per step
   at the step rate, driven by the exact block average (closed-form integral of
   the falling saw, its 30 µs reset pulse and the comparator pulse) over the
   step interval *centred* on the step instant. Trapezoidal memories are
   re-referenced whenever a stage's clock rate changes: `processCircuit` leaves
   `s = node + g·I_OTA`; keeping node voltages and OTA currents and swapping `g`
   is the rate-independent transfer (`reReferenceStage`).
3. **Wake** (`wakeCircuit`): the rank is restored to a stored idle-step
   snapshot at least W samples old (a 12-entry ring, one snapshot every 4 steps,
   ~16 ms deep) and replayed at the full internal rate with the production
   primitives up to the note-on, ending on the continuous phase. W covers the
   modes the step rate cannot represent (`kCatchUpSettleTaus` = 9 time
   constants) or mistunes by bilinear warping (`ln(error/1e-3)` time
   constants, error ≈ (π f_c/f_step)²·Q), bounded to 1–12 ms. A circuit that
   wakes before its first step replays its elapsed samples exactly.
4. Per-rank noise is omitted in the decimated step and supplied in the replay
   from a per-voice idle stream; the shared `audioRng` is never drawn by the
   idle path (a test reads the stream position across policies to prove it).

### Evidence class

| Law | Class | Source / alternatives |
|---|---|---|
| No keyboard-triggered oscillator or filter reset | source-derived | M41 (TR reaches only IC12/IC13 EG gates), IC42 (TI12 is the oscillator's own cycle comparator); ACTIVE_TAIL_CONTINUITY.md |
| Idle evolution law = the active production primitives | declared | same laws as the sounding path; alternative: a separate idle model (rejected: two models of one circuit) |
| Step rate 3 kHz, replay W, snapshot ring | numerical method | held to the per-sample truth by the 2.1 gate |
| Cold key voltage = C4# (MIDI 61) | declared | KAS6 power-on clear memorizes C4# after 2±1 s; the delay is not modelled; alternatives: MIDI 60 (earlier experiment), undefined |
| Cold phases from a fixed-seed stream | declared | alternative: all zero (rejected: a first chord would start phase-locked) |
| Retired expressions: pressure, CC74, member bend → 0; velocity (TWS b) held | declared | b is a sample/hold (TWS17); alternatives: zero velocity |
| Silent PANEL/MEMORY filter EG keeps relaxing, gate released | declared | EG capacitor has no reset path (ACTIVE_TAIL_CONTINUITY.md) |
| Reset (cold / Sustain-II-terminated) filter EG still restarts from IL | preserved behaviour | — |
| Software filter bypass: a bypassed stage is not clocked | preserved behaviour | the CS-80 has no filter bypass |

### Stage gate and results

Truth model: `src/reference/FreeRunningVoiceTruth.h` (double precision, every
sample, converged implicit solve). Gate: `tests/FreeRunningVoiceTests.cpp`
Part A, 5 scenarios (open saw, resonant low LP Q 3, 30 % pulse with raised HP,
Q 5 LP above the step band, HP above the fundamental) × host rates
44.1/48/96/192 kHz × idle 0.05/0.5/3 s, plus short gaps, vibrato while silent
and waveforms off while silent.

**First evaluation failed** (recorded in the tolerance file's erratum): worst
wake residual −8.7 dB, worst idle phase 2.1e-4 cycles, short-gap −50 dB.
Causes: (1) implementation defect, memories were not re-referenced across
rates; (2) procedure flaw, the window compared the float production chain with
the double truth over the whole run, so the production oscillator's own float
phase accumulation (present with no idle period) entered every number. The
procedure was corrected and re-registered before re-evaluation (truth
synchronised at silence; residual window compares two production chains
started from the realtime and the truth wake state). Two further implementation
revisions were needed before it passed, both recorded here: a causal block
average delayed the step input by half a step (fixed by centring), and the
first wake replay paired memories from the last step with oscillator input from
W samples earlier (fixed by replaying from stored snapshots).

| Gate (pre-registered) | Tolerance | Result |
|---|---:|---:|
| Wake residual, 20 ms window | ≤ −40 dB | worst −51.9 dB |
| Idle phase error, constant pitch | ≤ 1e-4 cycles | worst 7.9e-5 |
| Idle phase error, 5 Hz ±0.5 st vibrato | ≤ 2e-3 cycles | pass |
| Short gap (< one step), exact replay | ≤ −90 dB | pass |
| Zero-waveform decay (1 s) | ≤ 1e-3 of the state at silence | pass |

Mutation checks (each restored afterwards; `tests/FreeRunningVoiceTests.cpp`
unless noted): no re-referencing → 9 failing checks; wake does nothing → 70;
causal block average → 9; idle path draws the shared RNG → 2
(`cs80_active_tail_continuity_tests`); unconditional inactive skip → 18;
legacy reset on silent assignment → 6; no VCA reset after hard panic → 1; no
envelope-reset flag on Sustain II termination → 1.

### CPU

`cs80_source_factory_benchmark --quick --callbacks 1024` (48 kHz, 128-frame,
FTZ/DAZ), baseline built from `6719ce4` sources with identical g++ -O3 flags,
two interleaved rounds on the shared 4-core build machine (other agents were
building; p99 and overrun columns are dominated by that contention and are not
reported as results):

| Row (isolation) | baseline median RTF | ws3 median RTF |
|---|---:|---:|
| idle_quiet, round 1 / 2 | 0.0701 / 0.0746 | 0.0847 / 0.0812 |
| paired_tones pair 0 held | 0.2537 / 0.2714 | 0.2735 / 0.2653 |

Idle increment +0.015 / +0.007 RTF (mean +0.011), inside the pre-registered
0.02 budget. The engine's *total* idle cost (≈ 0.07 RTF at baseline) is
dominated by always-on global circuits owned by other workstreams (PRA, BBD,
output network, source readouts); the plan's "idle ≤ 2 % RTF" target for the
whole engine is therefore not met by this workstream alone and remains open
for the post-integration CPU pass. A wake costs one replay of 1–12 ms of a
two-rank chain (≈ 0.02–0.2 ms CPU); an eight-note chord on silent circuits
therefore adds up to ≈ 1.5 ms of CPU within one block in the worst case
(high-Q, high-cutoff patches).

### Intentional circuit-engine sound changes (re-pin V2)

- Assignment of a silent circuit no longer resets its oscillator phase or
  filter memories (they free-run); a first chord after load starts from
  declared distinct phases.
- Silent PANEL/MEMORY filter EGs keep relaxing, so a later attack starts from
  the relaxed level.

### Tests changed (intentional change named in each comment)

- `tests/ActiveTailContinuityTests.cpp`: excluded-case assignment now resets
  only in Legacy (and for never-clocked cold circuits); explicit reset returns
  the declared cold phases; the RNG-consumption check reads the shared stream
  through a Legacy cold assignment after a policy reset, which also proves the
  idle path draws nothing from it.
- `tests/SourcePerformanceAuditTests.cpp`: a policy transition yields exactly
  the cold state of a freshly prepared engine.
- `tests/SourceFactoryIntegrationTests.cpp`: three assertions that silent
  circuits do not evolve now assert that they free-run and are not resurrected.

### HARDWARE_COVERAGE.md entry (2026-10-31)

| Row | New text |
|---|---|
| Oscillators/filter state between notes | A in the circuit engine; E free-runs all 16 chains: exact idle phase, decimated IG00156 memories with rate re-referencing, snapshot replay at wake (ContinuousVoiceCircuit.h). T FreeRunningVoiceTests (gate vs FreeRunningVoiceTruth.h). Legacy unchanged. Remaining: oscillator law itself (ws1), noise topology, hardware comparison |

---

## 2.2 KAS scan and sample/hold — **done** (scan period and hold leakage declared)

### What changed (`src/core/KasScanner.h`, `Engine.cpp`; circuit engine only)

- **SH hold.** Each voice has a held K node: it holds when NAND(TR, SUS) says
  hold, i.e. the key is released (TR high; a CC64 pedal hold counts as keyed,
  a MIDI extension) and SUSTAIN I is selected (SUS pulled high). While holding,
  the glide is frozen (portamento acts upstream on KI) and K droops; in
  SUSTAIN II the switch keeps conducting and the tail keeps tracking KI (glide
  continues), as before. Master tune is applied downstream of the hold
  (declared; alternative: upstream). The droop ratio scales the VCO frequency
  and the filter key voltage, both linear in K. Legacy never holds.
- **Droop.** `dK/dt = −I/C` toward 0 V, never below.
- **Scan timing and priority** (opt-in parameter `kas_scan_timing`): note
  events are queued and acted on at the next scan instant; one scan dispatches
  lowest key first, a release before a strike of the same key. Panic discards a
  channel's queued events; switching the option off flushes the queue.
- **Ninth held key** (circuit engine): a held key that found no free circuit is
  remembered and takes the first circuit that frees (note-off, CC64 release),
  lowest waiting key first, with zero initial touch (it was not struck then).
  Released before that, it is forgotten; panic forgets it.

### Evidence class

| Law | Class | Source / alternatives |
|---|---|---|
| NAND(TR,SUS) → TC4016 track/hold, TR active low, SUS high = SUSTAIN I | source-derived | SH7/SH8, KAS4/KAS6, Panel66 (CIRCUIT_REVIEW.md); servicing guide pp25–26 corroborates (CS-50 example) |
| Glide frozen while holding | inference from topology | portamento upstream of KI; alternative: K downstream glide (would keep gliding) |
| Hold capacitance 0.033 µF | **declared** | SH7 does not print it; value of the KBC per-voice held nodes (brief and KEYBOARD_CONTOUR_EVIDENCE.md); alternatives 0.02 µF (SH7 note), 0.22 µF (CS-50 guide) |
| Leakage 1 pA toward 0 V | **declared** | CA3140T K buffer (SH8); data sheet: 10 pA typical magnitude, no sign; switch leakage taken as 0 (KI = K after release). Alternatives 0, 10–50 pA, either polarity |
| Scan period 1 ms, lowest key first, release before strike | **declared** | not in the inspected sources; alternatives 0.25–4 ms, top-down or arrival order |
| Ninth key reconsidered when a circuit frees | **declared** | YM26600 block diagram shows an 8-entry code memory and no 61-key store (KEYBOARD_MAPPING_CONSTRAINT_REVIEW.md, IC guide PDF15–16), so a held key without a channel is a new key on every scan. Alternative: edge-detecting scanner, never reconsidered (Legacy; owner manual p43 covers only the occupied case) |
| Reconsidered key has zero initial touch | **declared** | TWS b samples a strike transient; a key held still has none. Alternative: original velocity |

### Droop sensitivity (`cs80_kas_scanner_tests`, printed table)

Cents flat after 1 s / 5 s of Sustain-I hold:

| C | I | MIDI 24 (C1) | MIDI 36 | MIDI 60 |
|---|---|---|---|---|
| 0.033 µF | **1 pA (nominal)** | 0.84 / 4.2 | 0.42 / 2.1 | 0.10 / 0.52 |
| 0.033 µF | 10 pA | 8.4 / 42 | 4.2 / 21 | 1.05 / 5.3 |
| 0.02 µF | 10 pA | 13.9 / 71 | 6.9 / 35 | 1.7 / 8.7 |
| 0.22 µF | 10 pA | 1.3 / 6.3 | 0.63 / 3.2 | 0.16 / 0.79 |

A first implementation used 10 pA; the table showed 42 cents after 5 s at C1,
a strong audible claim for a leakage of unknown sign and magnitude, so the
nominal was lowered to the low end before committing. Both are listed.

### Tests (`tests/KasScannerTests.cpp`, 126 checks) and mutation checks

Gate: droop of the realtime node against `src/reference/SampleHoldTruth.h`
over C ∈ {0.02, 0.033, 0.22} µF × I ∈ {1, 10, 50} pA × K0 ∈ {1/16…2} V ×
t ∈ {0…1000} s advanced in uneven pieces: relative error ≤ 1e-6 (pass, exact
to rounding). NAND truth table, all four combinations. Engine: Sustain I tail
droops by the law at 44.1/48/96 kHz, Sustain II and Legacy do not, glide frozen
in I and continuing in II. Scanner: 500 random events all within one scan
period; scan-order dispatch; channel discard. Allocation regressions in all
three policies: 24-strike rotation, held protection, ninth key (circuit engine
reconsidered with zero touch, Legacy ignored), released ninth key forgotten,
lowest waiting key first, pedal protection and release, panic. Scan timing:
deferred to the next scan, acted on within one period, chord assigned in scan
order, opt-in, ignored by Legacy, flushed when disabled.

Mutations (restored afterwards): hold never engages → 7 failing checks; droop
ratio forced to 1 → 5; scan sort removed → 3; reconsideration disabled → 5;
glide not frozen while holding → 3.

### Intentional circuit-engine sound/behaviour changes (re-pin V2)

Sustain-I release tails hold their pitch at release (a glide in progress
stops) and droop by the declared law; a ninth held key now sounds when a
circuit frees.

### New parameter

| id | range | default | AU hint | notes |
|---|---|---|---|---|
| `kas_scan_timing` | 0..1 (switch, ≥ 0.5 on) | 0 | 10 | circuit engine only; adds ≤ 1 ms note latency |

### HARDWARE_COVERAGE.md entries (2026-10-31)

| Row | New text |
|---|---|
| Cyclic assignment and ninth held key | IB/A; circuit engine reconsiders a still-held ninth key when a circuit frees (declared YM26600 model, zero initial touch); Legacy ignores it. T KasScannerTests |
| Keyboard scanner, debounce and allocation timing | A (declared); optional `kas_scan_timing`: events act at 1 ms scan instants, lowest key first. Scan period not sourced. Debounce not modelled |
| Per-voice sample/hold, droop and retained key voltage | A; SH7 NAND(TR,SUS) hold in the circuit engine, glide frozen in Sustain I, constant-current droop (declared 0.033 µF / 1 pA). T KasScannerTests vs SampleHoldTruth.h |

---

## 2.3 Keyboard contour key map — **done** (key map declared; C2 conflict reported)

### What changed

- `src/core/KbcContour.h`: the declared physical-key → (Q1Q2Q3, octave tap,
  note tap) map and the KBC O1 voltage, evaluated by the existing
  `src/reference/KeyboardContour.h` network (all inputs clamped so it never
  throws).
- `Engine.cpp` (circuit engine): `refreshKeyboardContour()` computes, for each
  of the eight channels and the key last assigned to it, the KBC1 (brilliance)
  and KBC2 (level) voltages, then their receiving-network contributions: KC
  into FH through 120k and FL through 39k (effective cutoff CV per volt
  39/120 on HP, 1 on LP, the `SourceFactoryTones.h` map), KB into LC through
  270k (level gain per volt `levelPinPerTouch/levelUnitVolts` = 0.207/V). For
  source-selected ranks the shared FH/FL/LC bus loading by all eight channels'
  KC/KB branches is included from the compiled map. Refreshed at every
  assignment and every parameter frame (the KBC multiplexer refreshes the
  0.033 µF held nodes, so a lever move reaches held notes).
- `src/core/PerformanceControls.h`: the provisional weights
  (`kProvisionalKeyboardBrillianceOctaves/LevelRange`) **no longer reach the
  circuit engine**. They are not deleted: renamed `kLegacyKeyboard...` and
  documented as the Legacy compatibility law, because Legacy must stay
  byte-identical for old sessions that use the levers.

### The declared key map and its alternatives

| Group (OC tap) | Keys | Q1Q2Q3 | Source |
|---|---|---|---|
| G0 (OC0 = L) | C1 | 100 | KAS6 lowest-single-key code (flat at O) |
| G1 (OC1 = L) | C#1–C2 | **110** | forced by the CS-80 adjustment procedure: VR1 at C2 must act (PDF29/printed 27) |
| G2 (OC2 = (2L+R)/3) | C#2–C3 | 010 | KAS6 printed, same label |
| G3 (OC3 = R) | C#3–C4 | 110 | KAS6 printed |
| G4 (OC4 = (R+2H)/3) | C#4–C5 | 001 | KAS6 printed |
| G5 (OC5 = H) | C#5–C6 | 101 | KAS6 printed; VR2 at C5# acts (PDF30/printed 28) |

Note tap: C# = 0 … C = 11 within a group (KBC15 note ladder); C1 is a C.
Physical key = MIDI note − 24 (the engine's 61-key span, MIDI 24..84).
Trims: VR1 = 2136.29 Ω, VR2 = 4136.29 Ω, where the nominal network meets both
printed targets exactly (a declared aligned unit).

**The C2/C5# anchor conflict, openly:** read literally, KAS6 prints C2 → B
011 → electrical Q **100**, which puts C2 flat at OC1 = L where VR1 cannot act;
O1 would read **−4.018 V** at LOW minimum, outside the printed −2.76 ± 0.14 V
(`cs80_keyboard_contour_key_map_tests` prints both). C5# → Q 101 agrees with
VR2. The constraint review (KEYBOARD_MAPPING_CONSTRAINT_REVIEW.md) already
proved that no bit permutation, polarity or uniform label shift satisfies both
anchors. The declared map resolves it by reading KAS6's five columns as the
49-key (C2..C6) layout of the shared IC application — where C2 *is* the lowest
single key with code 100 — and keeping the printed codes by physical label for
C#2..C6, with the CS-80's added low octave taking the anchor-required 110.
With it the contour is continuous and monotone: LOW full at C1–C#1 (−4.02 V),
−2.76 V at C2, ramping to 0 V at C3; zero across C#3–C4; HIGH rising from C#4
to +2.76 V at C5# and +4.02 V at C6. Smoothness was not used to choose it; it
is reported as a consequence.

Alternatives listed (none implemented): (A1) literal KAS6 code at C2 (fails
the VR1 anchor); (A2) the 49-key servicing-guide OC association shifted onto
61 keys (discontinuous, per KEYBOARD_CONTOUR_EVIDENCE.md); (A3) C1 selecting
OC0 or OC1 — provably indistinguishable at every lever setting (tested);
(A4) the previous smooth provisional weights (kept for Legacy only).

### Evidence class

| Law | Class |
|---|---|
| KBC decoder, switch network, ladders, IC14 summers, O1 = 225/56 − V | source-derived (KBC1-2 p15, Panel2 p64), already in `KeyboardContour.h` |
| Key map | **declared** (table above) |
| Trim positions | **declared** aligned unit (meets the PDF29/30 targets exactly) |
| KC 120k/39k and KB 270k receiving branches | source-derived (R51–52 p39) |
| Effective CV / level gain per receiving volt | reuse of the source-factory receiver map (declared inference already used for TF/AB/IB) |
| Lever travel −1 = MINIMUM | consistent with the printed target signs; physical lever direction unverified |

### Gate and tests (`tests/KeyboardContourKeyMapTests.cpp`, 77 checks)

All 61 keys × the pre-registered 5 × 5 LOW/HIGH grid, read back from the
engine's HP and LP cutoffs (PANEL ranks, isolation) and converted to volts:
worst error **8.4e-7 V** against the reference (tolerance 1e-5 V). Anchors:
C2 −2.7600 V, C5# +2.7600 V (target ±2.76 ± 0.14). Declared code re-derived
independently for every key. Neutral levers give exactly 0 V; level levers
leave the filters alone and LOW LEVEL minimum silences the lowest key; a HIGH
lever move reaches a held top key; source-selected ranks stay finite with bus
loading; Legacy keeps its historical per-key weights.

Mutations: literal KAS6 code for G1 → 14 failing checks; HP using the LP
receiver ratio → 1; no refresh on a lever change → 1.

### Intentional circuit-engine sound change (re-pin V2)

The four contour levers now act through the KBC network: stronger at the
keyboard ends than before (up to ±4 octaves on LP and ±1.3 on HP at C1/C6 at
full lever, versus the provisional ±2 on both), zero across C#3–C4, and
different on HP and LP. At centre (the default) nothing changes.

### HARDWARE_COVERAGE.md entries (2026-10-31)

| Row | New text |
|---|---|
| 9 Keyboard brilliance LOW/HIGH | A in the circuit engine: KBC1 network (KeyboardContour.h) with a declared 61-key code map meeting both adjustment anchors; KC → FH 120k / FL 39k; Legacy keeps provisional weights. C2 KAS6-code conflict reported (ws3-keyboard.md). T KeyboardContourKeyMapTests |
| 10 Keyboard level LOW/HIGH | A in the circuit engine: KBC2 network, KB → LC 270k; same map |

---

## 2.4 Touch — **done** for the declared model (electrical calibration remains open)

### What changed (`src/core/TouchCircuit.h`, `src/core/ControllerCurves.h`, `Engine.cpp`)

Circuit engine, per voice channel, every sample (TWS17):

- **a (aftertouch)**: the key's present pressure (zero once the key is up,
  including MIDI pedal holds) through the 100k/1 µF follower, a 100 ms low-pass.
  Drives every aftertouch destination (AFTER BRILL/LEVEL, SUB VCO/VCF depth
  terms, the source TRG banks) instead of the raw MIDI pressure.
- **b (initial touch)**: at TR the acquisition window opens; the AC-coupled
  sensor signal is at rest for the contact lead, then carries the strike
  value; the 0.33 µF hold follows it through the FET on-resistance until the
  window closes, then holds (no droop nominal). Drives INITIAL BRILL/LEVEL and
  the source TRG banks instead of raw velocity. A circuit that is not struck
  (the reconsidered ninth key) acquires zero.
- **C (initial pitch transient)**: buffered b through 1 µF into 100k‖100k
  (50 ms), inverted; an ideal clamp keeps C ≤ 0 and so resets the coupling
  memory when b falls to rest at the start of the next acquisition. The pitch
  destination (INITIAL PITCH BEND) uses C; the former reset-to-velocity onset
  is now a consequence of the circuit (a fresh and an equal repeat strike give
  the same onset, tested), delayed by the 2 ms contact lead and softened by the
  0.33 ms acquisition.
- **TRG5 bus**: every channel's a feeds a 1S1555 anode; the cathodes join one
  bus with 47k to ground (high-impedance IC17 input through 330k). Solved each
  sample (warm-started Newton on the Shockley law, skipping non-conducting
  diodes), normalised to one full-pressure key, and used as the shared SUB
  speed pressure instead of the ideal maximum. Light pressure below the diode
  knee is strongly suppressed (≤ 10 % of the former linear law at 0.04).

Legacy is unchanged: it keeps the raw MIDI values, the reset-to-velocity
transient and the ideal maximum.

### Evidence class

| Law | Class | Source / alternatives |
|---|---|---|
| a = 100k·1 µF = 100 ms follower low-pass | source-derived component product | TWS17 a1 branch |
| C = 1 µF·(100k‖100k) = 50 ms high-pass, inverting, from buffered b | source-derived (conditional ideal-opamp) | TWS17 C1, TRG4 p23 (INITIAL_PITCH_TRANSIENT.md) |
| b sampled by FET1 onto 0.33 µF, held, feeding IBI/IBII/ILI/ILII banks; C feeds PB | source-derived topology | TWS17/TWS18 (2SK30, 1S1555, NJM4558), TRG19/TRG23 |
| Contact lead 2 ms | **declared** | MIDI has no sensor waveform; alternatives 0–10 ms |
| Acquisition τ = 1 kΩ × 0.33 µF = 0.33 ms | **declared** | 2SK30 on-resistance not specified (grade unknown); alternatives 0.2–3 kΩ |
| Window 10 ms = 0.01 µF × 1 MΩ | **declared** | trigger-path component product; threshold law not solved |
| No b droop | **declared** | 10 MΩ goes to the FET gate, not ground (TWS18); alternative 3.3 s |
| Ideal clamp, C ≤ 0 | **declared** | the clamp's conduction law is not established; alternative: no clamp (then an equal repeat strike gives almost no bend — tested as a mutation) |
| TRG5 diode-OR, 47k to ground | source-derived topology | TRG25 native crops (SHARED_PRESSURE_SPEED.md) |
| Diode law (Is 2.52 nA, n 1.752) | **declared** stand-in | 1N4148-class SPICE model for 1S1555 |
| 5 V a at full pressure, bus normalised to one full key | **declared** calibration | alternatives 2–10 V |
| MIDI velocity/pressure = normalised strike/force | **declared** interface | no MIDI-to-volt calibration exists |

### Gate (`tests/TouchCircuitTests.cpp`) and results

Truth: `src/reference/TouchCircuitTruth.h` (independent backward-Euler, 64
sub-steps per sample, clamp as a constraint, bus to convergence), host rates
44.1/48/96/192 kHz, strike sequences fresh / equal repeat / softer / harder,
pressure on then off.

| Gate (pre-registered) | Tolerance | Result |
|---|---:|---:|
| b hold relative error | ≤ 1e-4 | 8.6e-13 |
| C transient absolute error | ≤ 1e-3 | 4.9e-4 |
| C recovers e^-1 at 50 ms | ≤ 1e-3 | 2.8e-13 |
| a smoothing relative error | ≤ 1e-4 | 3.3e-7 |
| TRG5 bus (realtime vs converged) | ≤ 1e-3 V | 4.3e-6 V |

**First evaluation failed** the C gate (1.36e-2): the contact and window
instants fall at fractional sample positions (0.4 sample at 88.2 kHz, worth
≈ 0.011 of a fast b rise). The implementation now splits the boundary sample
exactly. Separately, my own diode-OR regression exposed that Newton iteration
on the exponential climbs only ~nVt per step from far below (a 64-iteration
solve from 0 V stopped at 2.9 V); the solver now starts from the single-diode
drop estimate. Both recorded; tolerances unchanged.

INITIAL_TOUCH_CIRCUIT_EVIDENCE.md implementation gate, item by item:

1. *b versus C destination separation*: initial brilliance holds b for the
   whole note with no pitch effect; C bends flat at onset and recovers without
   moving the filters — tested.
2. *TWS18 component identities*: used (2SK30, 1S1555, 0.33 µF hold, 10 MΩ to
   the gate); device curves/grade not available, so FET on-resistance and
   diode law are declared (table above).
3. *Switched FET/diode sequence*: declared acquisition sequence implemented
   and tested (initial touch arrives after the contact lead; repeat strike).
4. *Passive RC products kept separate from the driven response*: a and C are
   the component products; b is a driven acquisition, not a decay product.
5. *Receiving-stage gain, offsets, clamps, loading before MIDI→volts*: the
   clamp is modelled (declared ideal); the source TRG banks keep their
   compiled receiver gains; MIDI→volt calibration is declared normalised.
   **Open**: offsets and the clamp's real conduction.
6. *Regressions*: destination independence, zero-depth neutrality (velocity
   inaudible with every depth zero, sample-identical), retrigger, held notes,
   release (aftertouch relaxes after the key lifts), staggered polyphony —
   all tested at 48/96 kHz.
7. *Nominal versus measured*: every constant is marked SOURCE or DECLARED in
   `TouchCircuit.h`; nothing is presented as measured.

Mutations (restored): no clamp → 3 failing checks; no contact lead → 7;
unsmoothed a → 2; raw velocity on the initial-touch destinations → 2;
ideal maximum instead of the TRG5 bus → 2.

### Controller response curves (INTERFACE LAYER)

`applyVelocityCurve` / `applyPressureCurve` act on MIDI velocity and pressure
before any circuit model, in every policy including PURE CS-80 (they calibrate
the controller, not the instrument; alternative: bypass in PURE). Curve
exponent 4^-curve; the pressure threshold removes a controller's resting load
and rescales. At the defaults both are the exact identity (tested over 1001
points), so default renders are unchanged.

### Intentional circuit-engine sound changes (re-pin V2)

Initial-touch destinations follow b (≈ 2–3 ms acquisition after TR instead of
instantaneous); the initial pitch bend starts after the contact lead with a
0.33 ms rise; aftertouch destinations are smoothed by 100 ms; the shared SUB
speed follows the TRG5 diode bus; silent channels' held b and relaxing a now
load the source TRG buses.

### New parameters

| id | range | default | AU hint | notes |
|---|---|---|---|---|
| `velocity_curve` | −1..1 | 0 (identity) | 10 | interface layer |
| `pressure_curve` | −1..1 | 0 (identity) | 10 | interface layer |
| `pressure_threshold` | 0..0.5 | 0 | 10 | interface layer |

### HARDWARE_COVERAGE.md entries (2026-10-31)

| Row | New text |
|---|---|
| 42 Initial-touch brilliance / 43 Initial-touch level | A; circuit engine drives them from the TWS b sample/hold (declared acquisition: 2 ms contact lead, 0.33 ms FET charge, 10 ms window, no droop). T TouchCircuitTests vs TouchCircuitTruth.h |
| 12 Aftertouch → sub speed | IB/A; circuit engine: TRG5 1S1555 diode-OR bus with 47k, declared diode law and 5 V scale, normalised to one full key |
| 44/45 Aftertouch brilliance/level | A; circuit engine: TWS a 100 ms follower smoothing |
| TKC/TSB initial/aftertouch circuitry | A (declared); separate a, b, C paths now modelled in the circuit engine; sensor force law and calibration still open |
| Initial pitch bend | A; C = −HPF₅₀ms(b) with ideal clamp, onset after the contact lead |

---

## Ribbon from a MIDI CC — **done** (interface layer)

`Engine::controlChange`: when `ribbon_cc_enable` is on, the CC numbered
`ribbon_cc` positions the ribbon: 64 = centre (no bend), 127 = full travel up
(ribbonFrequencyRatio(+1)), 0 = full travel down; values between scale
linearly on each side. **Default CC 16** (General Purpose Controller 1): it has
no other meaning in this instrument and is commonly free on controllers with a
ribbon or touch strip. The mapping is **off by default**, because a session
that happens to send CC 16 would otherwise change behaviour. It is an input
adapter for an original control, like velocity, so it works in PURE CS-80 too
(it is checked before PURE's CC 1/11/64/74 filter). The ribbon's Sustain I/II
coupling and the on-screen ribbon are unchanged; both write the same ribbon
position (last writer wins). Tested in `cs80_tuning_midi_tests`.

| id | range | default | AU hint |
|---|---|---|---|
| `ribbon_cc_enable` | 0..1 (switch) | 0 (off) | 10 |
| `ribbon_cc` | 1..119 | 16 | 10 |

---

## Phase 6 tuning, MPE and voice modes — **done** except the MTS-ESP library (see below)

All of this is modern layer: PURE CS-80 forces 12-TET (`tuning_mode` 0) and
eight single voices (`voice_mode` 0), and MPE is already forced off there.
Defaults leave every render unchanged (`tuning_mode` 0 plays exactly
`(float) note`; the glide start uses the last tuned pitch, identical in 12-TET).

### Scala microtuning (`src/core/Tuning.h`)

- `.scl` parser: comments, description, note count, cents (with a dot) or
  ratios (`a/b`, `a`), trailing text ignored, locale-independent; the period is
  the last degree.
- `.kbm` parser: map size, first/last note, middle note, reference note and
  frequency, formal-octave degree, map entries with `x` for unmapped keys.
  Without a `.kbm`, the default mapping is linear with middle C (60) at
  261.6256 Hz so a 12-note equal scale reproduces 12-TET.
- `buildTable` gives each MIDI note a fractional-semitone pitch and a mapped
  flag. The engine plays a key at its table pitch (glide, filter key voltage
  and every modulation follow it), ignores an unmapped key, and retunes
  sounding notes when the table or mode changes. The KBC contour and KAS key
  map keep using the physical key, not the tuned pitch. TUNING REF still
  scales the whole table.
- Bundled test files `tests/data/tuning/`: 12-TET, 5-limit just, quarter-comma
  meantone, 19-EDO, a just diatonic scale, an A=432 Hz mapping, and a
  white-keys-only mapping with unmapped black keys (written for this project).
- Processor API (`PluginProcessor.h`): `loadScalaTuning(scl, kbm)` returns an
  error string or empty; `clearScalaTuning()`, `getScalaDescription()`. The
  table reaches the audio thread through a spin lock the audio thread only
  try-locks (never waits). The texts are saved in the session (`WS3_TUNING`
  child of the state tree), not in presets.

### MTS-ESP and MIDI Tuning Standard

- **MTS SysEx** (no library needed): bulk dump (08 01), real-time single note
  (08 02), non-real-time single note with bank (08 07), scale/octave 1- and
  2-byte (08 08 / 08 09, channel mask not applied). Parsed allocation-free
  from the raw MIDI bytes at their sample position in `processBlock`
  (`Engine::handleSysEx`); `tuning_mode` 2 plays the result.
- **MTS-ESP client: not vendored.** ODDSound's repository page states the 0BSD
  licence and that a client needs `Client/libMTSClient.h/.cpp`, but the source
  files could not be fetched byte-exactly in this session (direct download of
  the raw files was not permitted; the web reader returns summaries). The
  shim `src/plugin/MtsEspClient.{h,cpp}` compiles the client in automatically
  when those two files exist at `deps/mts-esp/Client/` (compile-time switch
  via `__has_include`); then, in `tuning_mode` 2, every block fills the table
  from `MTS_NoteToFrequency`/`MTS_ShouldFilterNote` and forwards SysEx to
  `MTS_ParseMIDIDataU`. **Missing:** the library itself (vendor it with its
  licence in-tree to ship), a compile test of that branch, per-channel
  retuning queries (the shim asks with channel −1), the master's scale name,
  and a UI connection indicator (`isMtsEspConnected()` is ready for one).

### MPE (`src/core/MpeZones.h`, Engine)

- Zone negotiation while MPE is on: MCM (RPN 6) on channel 1 sets the lower
  zone's member count, on channel 16 the upper zone's; a growing zone shrinks
  the other; an MCM resets that zone's negotiated bend ranges. RPN 0 on a
  master sets the zone's master bend range, on a member the members' range;
  null RPN deselects. Without any MCM the layout is one lower zone with 15
  members, exactly the engine's previous behaviour.
- Upper zone: notes on upper members take the channel-16 master's bend,
  pressure, CC74 and sustain pedal; lower-zone notes keep channel 1's.
  RPN is ignored when MPE is off (no change for non-MPE sessions).
- **High-resolution velocity**: CC88 supplies the low 7 bits of the next
  note-on velocity on its channel (MMA CA-031); without a prefix the velocity
  is bit-identical to JUCE's `getFloatVelocity`. MIDI 2.0 velocity is not
  available: JUCE 8.0.11 delivers MIDI 1.0 `MidiBuffer`s to VST3/AU/standalone
  processors, so there is no 16-bit velocity to read.

### Voice modes (`voice_mode`, `unison_detune`)

| Mode | Behaviour | Label |
|---|---|---|
| 0 | eight voices, the original allocator | **authentic default** |
| 1 | sixteen voices: a second, complete eight-voice instrument in the processor; a key goes to the first until it has no free circuit (`SixteenVoiceRouter`), everything else reaches both, outputs summed | **extension** (two CS-80s side by side; each keeps its own source networks, chorus and output stage) |
| 2 / 3 / 4 | unison ×2 / ×4 / ×8: each key takes N circuits, detuned symmetrically over `unison_detune` cents; a key's release releases all N | **extension**; circuits are allocated like ordinary keys (no stealing), so ×8 is first-come monophonic |

The second instrument starts when mode 1 is selected and is hard-stopped when
it is left. In mode 1 the editor's voice meter and on-screen pressure
ownership cover the first instrument only (pressure on keys that landed in the
second is not attributed — conservative).

### Tests (`tests/TuningMidiTests.cpp`, 81 checks)

Scala/KBM against hand-computed pitches for every bundled file, malformed
input rejection; MTS single-note, octave and bulk-dump messages, the no-change
marker, other-device and truncated messages; engine Scala and MTS modes,
retuning of a sounding note, unmapped key, PURE inert; MPE default layout, MCM
negotiation and shrinking, RPN 0 member range, null RPN, upper-zone master
bend and pressure isolation, member range, RPN ignored without MPE; CC88;
unison ×2/×4/×8 allocation, symmetric detune, release of all circuits, PURE
inert; sixteen-voice routing; ribbon CC default off, centre/ends, PURE, CC
number assignable.

### New parameters (AU hint 10, all plain floats; switches ≥ 0.5, choices rounded)

| id | range | default | notes |
|---|---|---|---|
| `tuning_mode` | 0..2 | 0 | 0 12-TET, 1 Scala/KBM, 2 MTS (SysEx or MTS-ESP); survives preset recall |
| `voice_mode` | 0..4 | 0 | see table above; a patch setting |
| `unison_detune` | 0..50 ct | 10 | spread across the unison circuits |

`kas_scan_timing`, `velocity_curve`, `pressure_curve`, `pressure_threshold`,
`ribbon_cc_enable`, `ribbon_cc` and `tuning_mode` are setup parameters: they
survive factory-preset recall (`isPhysicalSetupParameter`), like the
footswitch jack. Host IDs are appended after `source_performance` in enum
order; core indices follow the ws1/ws2 blocks and will shift at integration
(refer to them by name).

### UI mount points for the integrator (ws8 Studio view)

No editor code was added. A Keyboard/Tuning page needs: a file chooser calling
`loadScalaTuning(scl, kbm)` (show the returned error), `getScalaDescription()`,
`isMtsEspAvailable()` / `isMtsEspConnected()`, and the parameters above
(curves, threshold, ribbon CC enable/number, KAS scan timing, tuning source,
voice mode, unison detune).
