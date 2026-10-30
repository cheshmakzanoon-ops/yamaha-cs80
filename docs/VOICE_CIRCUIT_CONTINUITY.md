# Voice-circuit continuity: bounded non-production experiment

The current playable checkpoint is unchanged. This experiment addresses a real
omission: after a software voice becomes inaudible, its production oscillator
phase and HP/LP histories stop evolving and are reset on its next assignment.
It is **not** a new host mode, a production integration, or a calibrated CS-80.
Public evidence and explicit engineering assumptions are sufficient to develop
this correction; obtaining hardware is not a prerequisite.

## Current behavior and precise omissions

Audited core: playable `428e4b2`, with `Engine.cpp` SHA-256
`df7ba097bf37d400facd5ed7a861e26826af29f2a19710c43b15458798bb1bce`.

| Current path | Consequence |
|---|---|
| `startVoice`, lines760–801, preserves audio state only for a still-active, unheld source-mode tail | Cold/inactive reassignment randomizes both oscillator phases and clears both HP/LP memories. |
| `processInternalSample`, lines1218–1219, skips inactive voices | Oscillators, filters, per-voice glide and the nominal50ms initial-pitch transient freeze. Per-rank telemetry is then stale. |
| `updateSourceReadouts`, lines524–550 | Source-selected filter EGs already advance for all eight receivers, exactly once. Inactive touch drives are explicitly zero. PANEL/MEMORY filter EGs do not receive this idle clock. |
| HP/LP enabled branches, lines1314–1323 | Even an active voice's bypassed filter histories freeze. |
| Voice retirement, lines1357–1362 | Audible lifetime is determined by both amplitude EGs becoming Idle, not by completion of every control/circuit state. |
| `reset`, lines75–92 | Note/phase/filter/EG state resets, but fields overwritten at the next assignment, such as target/glide/controller state, are not all initialized there. This is harmless under the present skip; blindly enabling idle processing would expose stale fields. |
| Shared `audioRng` draws, lines795/1277/1431–1432 | Simply adding inactive-rank draws shifts other voices' and output-noise histories. |

A fresh 48kHz non-mutating probe against the existing core played notes48–55,
then released all eight. Source Performance with tabs `{1,3}` retired by
0.181333s. One further second left voice0 phase `0.290729403` and nonzero HP/LP
memories bit-identical while its filter EG moved `0.00605880981` to zero.
Reassignment changed phase to `0.0118877292` and zeroed both filters. A PANEL
fixture likewise froze its audio state, and also froze the filter EG at
`0.178478435`. This confirms software behavior, not an instrument measurement.

Global SUB/PWM/LFO/drift clocks and source PRA/output circuits already advance
outside the inactive-voice skip. They must not accidentally receive another tick.
The current envelope retrigger law, initial-touch `b` hold approximation, pressure
smoothing/retirement, sustain-II termination and MIDI allocator conventions are
separate fidelity questions; the new kernel does not silently replace them.

## Primary evidence and uncertainty

The independently inspected [electrical evidence review](voice-continuity/CIRCUIT_REVIEW.md)
records original source URLs, hashes, wiring and the distinction between the
CS-80 drawings and the servicing guide's CS-50 example.

- Yamaha M41 connects TR to the two envelope gates, not to a drawn oscillator
  timing or filter-capacitor reset. The oscillator's own cycle reset is not a
  keyboard reset. Continuously evolving the existing numerical states is a
  source-informed **engineering approximation**; it does not recover CV
  feedthrough or calibrate SVF memories as capacitor volts.
- SH7/SH8 show a TC4011 NAND controlling a TC4016 bilateral KI-to-K switch.
  Electrical `TR,SUS = LL/LH/HL` conducts; `HH` holds. Panel66 and reciprocal SH8
  wiring establish the pulled-high/open versus forced-low sustain switch route.
  Yamaha's servicing-guide explanation corroborates the logic and active-low
  trigger, but its four-channel latest-key example and capacitor value must not
  be imported as demonstrated CS-80 facts.
- KAS6's delayed startup key-code initialization does not establish every SH
  output's startup voltage or any oscillator phase. The experiment therefore
  requires explicit cold K and two phase inputs.
- Key voltage is a **linear-frequency quantity**, not1V/oct. The default
  `523.2 Hz/V` is the rounded service-sheet42 `.5V ->261.6Hz` anchor. Matched
  software fixtures explicitly use `midiToHz(60)/.5` to retain the production
  A4=440 interface. VCO-only modulation/footage is separate from filter key K.
- Ideal instantaneous acquisition and zero hold leakage are declared nominal
  assumptions. No capacitance, leakage constant, thermal drift or original
  startup phase is guessed from an ambiguous note on the schematic.

## First executable piece

`src/experimental/ContinuousVoiceCircuit.h` contains one physical-voice kernel:
one shared K state and two ranks, each with the **unchanged production**
`Oscillator`, `Svf` HP and `Svf` LP. It does not include or promote
`ReconstructedOscillator`, modify `Dsp.h`, or select any Engine policy.

```cpp
bool prepare(double internalRate, float explicitInitialKeyVolts,
             std::array<float, 2> explicitInitialPhases);
Output processSample(const Controls& electricalInputs);
State getState() const;
```

Inputs expose `keyInputVolts`, electrical `trHigh`/`susHigh`, and separate rank
Hz/V, VCO multiplier, PW/wave selections, noise, effective cutoff CVs, Q, bypass,
filter/sine level and output gain. Electrical high is not Yamaha's
negative-logic key-code label1. There is no hidden global-last-note rule or
hard-coded MIDI-to-TR translation.

Contract:

1. Each accepted internal sample advances two oscillators and four SVFs,
   including when output gain is zero. There is no note-on reset API. Changing
   KI/gates/controls changes the next interval, never instantly clears phase or
   audio memory.
2. Bypass is an explicit modern routing overlay. HP still processes its source;
   bypass selects raw input rather than HP output for LP. LP always processes
   that selected input; its bypass selects the input for output. This preserves
   hidden histories without inventing original hardware bypass switches.
3. Noise is externally supplied before the production `.9` input scale. The
   kernel draws no noise and consumes no Engine RNG. It does not establish the
   hardware's shared-noise correlation or choose a new noise law.
4. EGs, initial/after-touch, loaded source buses and audible allocation are
   caller-owned, exactly once per interval. The kernel receives their scalar
   consequences. Tests using gain0 prove the **circuit mute boundary**, not
   Engine Panic/retirement safety or complete envelope/touch reconstruction.
5. Preparation and complete sample transactions reject invalid values before
   changing K, either rank, coefficients or clocks. Finite values in both ranks
   are validated even when masked by mute/bypass/hold; derived Hz/cutoffs are
   checked before commit. Rejected calls return `accepted=false`, zero readouts,
   and preserve future trajectory as well as visible state.
6. The software test domain is explicit: internal16kHz–1.536MHz, K0–1024,
   Hz/V and VCO multiplier0–4096, cutoff CV±64, Q.35–9, PW.05–.95, noise±64,
   and nonnegative gains up to64. These are numerical experiment bounds, **not
   hardware voltage ratings or host parameter ranges**. Requested-rate readouts
   precede the unchanged oscillator/SVF clamps.
7. Fixed state, no allocations/locks, one bounded two-rank loop. No claim of
   machine-level WCET is made. Envelope/FX/loaded-bus work is absent here.

No existing saved state, preset, parameter ID/order/default, Legacy output,
Isolation output or Source Performance output changes. Existing goldens retain
their original meaning and tolerances.

## Reproduction and objective gates

From the repository root (C++20 compiler):

```sh
g++ -std=c++20 -O2 -Wall -Wextra -Wpedantic -Werror -Isrc \
  tests/ContinuousVoiceCircuitTests.cpp -o /tmp/continuity-tests
/tmp/continuity-tests
g++ -std=c++20 -O2 -Wall -Wextra -Wpedantic -Werror -Isrc \
  tests/ContinuousVoiceCircuitAuditTests.cpp -o /tmp/continuity-audit
/tmp/continuity-audit
g++ -std=c++20 -O3 -Wall -Wextra -Wpedantic -Werror -Isrc \
  src/tools/voice_continuity_probe.cpp -o /tmp/continuity-probe
/tmp/continuity-probe --trace /tmp/continuity-trace.csv
# Run timing without competing builds/tests; output must not already exist.
/tmp/continuity-probe --benchmark /tmp/continuity-timing.csv 256
```

The trace has528 rows, electrical edges, two-rank phase/filter histories and
requested frequencies. Row `internal_sample=0` observes the **post-tick** state
at elapsed1/96000s, not startup state at time zero.

The timing matrix has72 rows:44.1/48/96kHz ×128/256 frames ×static/moving
controls ×0/1/8 audible voices ×skip/continuous. `skip_inaudible` is a synthetic
kernel control, **not the production Engine baseline**. Both policies receive
the same supplied noise/control sequence; audible output checksums must match,
and eight-voice state checksums must match. Exact tick counts and final state
checksums make muted work observable to the optimizer. Controls/checks are
included consistently; buffers are allocated before callback timing. Report
median, p99, maximum, overruns and phase-level process CPU without discarding
outliers. Process CPU and callback wall clocks have different scopes.

| Gate | Required evidence |
|---|---|
| Electrical truth/cold state | All four NAND combinations, explicit K/phases, no unrequested initialization or negative-logic inversion. |
| State continuity | Both ranks against independently assembled production-primitive calls, mute/bypass/hold/track transitions, pitch jumps, changing cutoff/Q, zero-waveform decay, and nonzero reopening. |
| Ownership | Caller EG advances once; no hidden touch/noise clock, no VCO-only modulation added to filter K. |
| Failure/lifecycle | Invalid second-rank and masked inputs preserve all state/future trajectory; prepare boundaries and long-run finiteness. |
| Old behavior | Production files unchanged; later integration must replay the original Legacy/Isolation/**existing Source Performance** corpora byte-for-byte. |
| Incremental cost | Predeclared continuous-eight median overhead no more than10% versus matched skipped-eight; investigate a failure before the adapter. Publish the larger0/1-voice cost and all tail timings regardless. This is not a full Engine/host realtime gate. |

Owned suite: **151,578 checks passed**, strict GCC14 build. The
[independent audit](voice-continuity/AUDIT.md) passed **33,962 checks** in strict
native and ASan/UBSan runs and rejected seven targeted clock/routing/validation
mutations. Leak detection was disabled, so this is not a leak-check result;
there is no concurrent-use/TSan or hardware-accuracy claim.

### First, exclusive kernel timing result

Measured published kernel commit
`00624e2d5389b5ea30b0bb3dea2a81077d32b109`, GCC14.2 `-O3`, cloud AMD EPYC9V74.
The first result was retained, not rerun until it passed. Frozen
[criterion](voice-continuity/kernel-benchmark/predeclared-criterion.txt),
[source/compiler metadata](voice-continuity/kernel-benchmark/metadata.json),
[machine context](voice-continuity/kernel-benchmark/context.txt),
[all72 rows](voice-continuity/kernel-benchmark/results.csv),
[computed summary](voice-continuity/kernel-benchmark/summary.json), and
[528-row trace](voice-continuity/kernel-benchmark/trace.csv) are preserved.

All12 paired-eight median ratios passed the predeclared1.10 gate: the observed
range was **0.9930–1.0046**. All matched audible checksums agreed and eight-voice
state checksums agreed. Source hashes matched before and after measurement.

| Host rate | Continuous0-audible median RTF | Continuous1-audible median RTF | Continuous8-audible median RTF |
|---|---:|---:|---:|
| 44.1kHz | .1312–.1329 | .1308–.1337 | .1304–.1330 |
| 48kHz | .1431–.1448 | .1425–.1456 | .1423–.1454 |
| 96kHz | .2793–.2855 | .2795–.3042 | .2818–.2841 |

Each range spans static/moving controls and128/256 frames. Synthetic skipped
zero-audible median RTF across the entire matrix was .000409–.004514; the
continuous idle cost is real, not free. These numbers exclude source loaded
buses, actual EGs, global effects, decimation and the plugin/host.

**One deadline overrun among18,432 callbacks remains part of the result:**
continuous one-audible, static controls,96kHz/256 frames: maximum3.912669ms
against2.666667ms budget (**1.46725 RTF**). That case's median was.30421 and
p99.58923 RTF. No cause is assigned or outlier discarded. There were no other
observed kernel overruns, but this finite cloud run is not a realtime guarantee.
The reported median is the upper-middle order statistic; p99 uses the
nearest-rank `ceil(.99*N)` value. Raw per-callback traces were not retained;
all per-case aggregates and observed maxima are retained.

## Bounded next decision, not indefinite quarantine

The kernel gates passed. The recommendation is **proceed to one thin,
immutable-Engine integration experiment**, not enable a host feature. That
experiment must pin/hash checkpoint428 and its small adapter patch, keep the
original engine branch and compare its three-mode audio corpus exactly, while
the candidate drives the kernel from the real source/EG/control graph.

Before that adapter, freeze explicit choices for cold pitch/phases, per-circuit
KI/TR/SUS mapping, idle member-expression ownership, held-key versus tracking
behavior, PANEL/MEMORY idle EGs, source TP1 single-clock timing, and supplied
noise. Preserve separate audible gates so Panic/Sustain-II cannot resurrect
retired notes; test stale UI-note ownership and mode/prepare transitions at the
host boundary before any eventual activation. Those are not proven by a silent
kernel output.

The experiment should produce audio/diagnostic traces and matched0/1/8-voice
full-core timing, then explicitly **integrate / revise / reject**. Integration
requires an approved, versioned opt-in/migration policy: no silent alteration of
old Source Performance presets or Isolation semantics. No new host parameter is
authorized by this kernel work. If measured cost warrants it, a separately
identified bit-exact coefficient-cache variant may skip redundant `Svf::set`
calls while still processing every sample; the first baseline remains uncached.

Current known CPU context: existing source-idle medians were0.063–0.072 RTF and
eight-held0.238–0.290 in the [source baseline](performance/2026-10-30-source/SUMMARY.md).
[Source Performance](performance/2026-10-30-source-performance/SUMMARY.md) dry
held medians were0.236–0.274, with observed deadline overruns. Full continuity
requires16 oscillator and32 filter updates per internal sample: at48kHz/2×,
1.536 million oscillator and3.072 million filter updates/s even at rest.
Expecting idle cost to approach an eight-voice workload is an inference, not a
measured new-engine bound. No hardware capture or unsupported experimental
oscillator is required to make the next bounded integration decision.
