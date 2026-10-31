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
