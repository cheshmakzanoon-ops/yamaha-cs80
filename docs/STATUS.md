# Current status

This is the one page that states where the project is **now**. Other documents
are specifications, dated evidence or history; when they disagree with this page
about the current state, this page wins and the other document is stale.

Update this page whenever a phase step in [IMPROVEMENT_PLAN.md](IMPROVEMENT_PLAN.md)
lands. Record exact commits; do not restate test counts without one.

Last updated: 2026-11-01, qualification branch `finish/cs80-qualification-20261031` from
`main` `db24a47170610f81236e8829ef5536af69dfc94d`. Earlier eight-workstream
history is in [workstreams/HANDOVER.md](workstreams/HANDOVER.md).

## Where to read what

| Question | Document |
| --- | --- |
| What are we building, in what order? | [IMPROVEMENT_PLAN.md](IMPROVEMENT_PLAN.md) (plan), [PARITY_ROADMAP.md](PARITY_ROADMAP.md) (workstream detail) |
| What did each workstream do, and what is open? | [workstreams/HANDOVER.md](workstreams/HANDOVER.md) and the `wsN-*.md` reports beside it |
| What is the fidelity target and evidence policy? | [ANALOG_FIDELITY_REQUIREMENT.md](ANALOG_FIDELITY_REQUIREMENT.md) |
| Which control is implemented how, and what is still open? | [HARDWARE_COVERAGE.md](HARDWARE_COVERAGE.md) |
| How does audio flow through the engine? | [ENGINE.md](ENGINE.md) |
| What do the render modes mean? | [SOURCE_PERFORMANCE.md](SOURCE_PERFORMANCE.md), [PARITY_ISOLATION.md](PARITY_ISOLATION.md) |
| Editor layout and rules | [UI_DESIGN_SPEC.md](UI_DESIGN_SPEC.md), [PANEL_CONFORMANCE.md](PANEL_CONFORMANCE.md) |
| Release, installers, licence | [release/RELEASE_QUALIFICATION.md](release/RELEASE_QUALIFICATION.md), [../LICENSE](../LICENSE) (AGPLv3) |
| Build, packages, CI evidence | [README.md](../README.md), [CI_DISTRIBUTION_STATUS.md](CI_DISTRIBUTION_STATUS.md) |

## Engines

- **ENGINE: CIRCUIT** (host parameter `source_performance`, "Engine") renders the
  circuit models below. Fresh instances and new presets use it (plan 1.7, state
  schema 3).
- **ENGINE: LEGACY (compat)** keeps the older voicing byte-identical, so sessions
  saved before schema 3 and the Legacy Library sound unchanged.
- **PURE CS-80** (`parity_mode`) strips every modern addition (MSEG, matrix,
  modern FX, LFOs, arpeggiator, MPE, microtuning, voice modes, MIDI learn,
  Program Change, voice dispersion and drift) and runs the circuit models only.

## Model register

Which implementation produces audio today, by subsystem. "Circuit engine" means
the Source Performance and PURE CS-80 policies.

| Subsystem | Legacy | Circuit engine | Evidence / reference |
| --- | --- | --- | --- |
| Oscillator | PolyBLEP `Oscillator` (`Dsp.h`) | reconstructed event-driven WSC, PWL sine, shared residual basis (`ReconstructedOscillator.h`) | [OSCILLATOR_PROMOTION.md](OSCILLATOR_PROMOTION.md) |
| HP/LP filters | TPT `Svf` with input `softClip` | IG00156 nodal stage: input networks, 1.2 MOhm/180 pF branch, 1 uF couplings, in-loop OTA, declared Q block and cutoff calibration (`Ig00156Filter.h`) | [IG00156_NODAL_MODEL.md](IG00156_NODAL_MODEL.md), truth `reference/IG00156Network.h` |
| Envelopes | `Adsr`/`FilterEnv` float path | inferred single-RC law | [ENVELOPE_RC_MODEL.md](ENVELOPE_RC_MODEL.md) |
| VCA | linear multiply | IG00151: LI node, exponential SUB input, pedestal, AC coupling, constant-power pan (`Vca.h`) | [IG00151_VCA_MODEL.md](IG00151_VCA_MODEL.md) |
| Voice variation | uniform tune/pan trims, 0.031 Hz sine drift | declared 5 % R / 10 % C card draws into tune, cutoff, Q, level, EG times; 1/f thermal drift with warm-up; sine as "worn unit" (`ToleranceModel.h`) | [TOLERANCE_DRIFT_MODEL.md](TOLERANCE_DRIFT_MODEL.md) |
| Idle voices | frozen, reset on reuse | all 16 chains free-run (closed-form idle oscillator, decimated stages, snapshot replay); untriggered filter envelopes settle continuously | [workstreams/ws3-keyboard.md](workstreams/ws3-keyboard.md), truth `reference/FreeRunningVoiceTruth.h` |
| Keyboard | MIDI note to channel | KAS scan option, SH7 hold and droop, ninth-key reconsideration, KBC contour network | ws3 report, `KasScanner.h`, `KbcContour.h` |
| Touch | raw MIDI velocity/pressure | TWS a/b/C nodes, TRG5 diode-OR bus | ws3 report, `TouchCircuit.h` |
| Tone tabs | legacy bridge voicings | `SourceFactoryTones.h` (7 complete, 15 inferred; 25 cells are labelled hypotheses) | [PRESET_CIRCUIT_LEDGER.md](PRESET_CIRCUIT_LEDGER.md), ws6 report |
| SUB | legacy `Lfo` or `OriginalSubOscillator` | IG00150 exponential converter, band-limited saw/square | [ORIGINAL_SUB_OSCILLATOR.md](ORIGINAL_SUB_OSCILLATOR.md), ws4 report |
| Ring modulator | provisional law, ideal multiply | ADJ p19-anchored converter, offset multiplier with declared feedthrough | ws4 report |
| Chorus / tremolo | generic `Chorus` | OE2 BBD with true L/R paths; OE1 four-VCA tremolo | [BBD_CHORUS_MODEL.md](BBD_CHORUS_MODEL.md), ws4 report |
| Wah / EXP | SVF band-pass stand-in, linear expression | `PraWahCircuit` with declared LDR optics; expression through the same cell | [REALTIME_PRA_WAH_MODEL.md](REALTIME_PRA_WAH_MODEL.md), ws2/ws4 reports |
| Noise | per-voice uniform draws | one SUB29 generator with declared PSD; mix-bus hiss PSD | ws2 report, truth `reference/NoiseSpectrum.h` |
| Output | scalar LOW trim + soft clip | -6 dB reference, driver ceiling, `OutputTapCircuit`; LISTENING OUTPUT L/R, GENERAL, PHONES | [OUTPUT_REALTIME_PROPOSAL.md](OUTPUT_REALTIME_PROPOSAL.md), ws2 report |
| Smoothing | per-sample constants | 5 ms one-pole on every continuous control (`Smoothing.h`) | ws2 report |
| Oversampling | 2x | Eco 2x / Standard 2x / HQ 8x; offline HQ; 63 + 8/N samples latency | `Oversampling.h`, ws2 report |

Evidence classes: every law cites a schematic or manual page, or is labelled a
declared inference with its alternatives
([ANALOG_FIDELITY_REQUIREMENT.md](ANALOG_FIDELITY_REQUIREMENT.md)). Nothing in
this register is a measurement of a CS-80. The in-app evidence view
(Studio > SETTINGS) and per-control help compile the same register
(`src/plugin/EvidenceRegister.h`).

## Phase progress

| Plan step | State | Commit |
| --- | --- | --- |
| Plan recorded | done | `97cc815` |
| 0.1 Coefficient caching | done; bit-identical | `7b55d34` |
| 0.1 Split, fast math, structure of arrays | global/rank/mix/output split and 16-chain SoA done; oscillator, Jacobian and receiver caches done; whole-engine CPU target still open | `1207068`, `6bf6055`, `031a333`, `c3c0f5c` |
| 0.1 Silent rank cadence | 3 kHz to 1.5 kHz qualified for the free-running circuit ranks; strict V2 re-pin complete; measured CPU target still open | audio `18400c2`, pin `9f990ae` |
| 0.2 Parameter smoothing | done (circuit engine), ws2 | `d29f6eb` |
| 0.3 State schema version | done (schema 3 since ws2) | `eb486cb`, `83f8a0e` |
| 0.5 This status page | done | `eb486cb` |
| 1.1 IG00156 filter, first stage | done | `f6b126d` |
| 1.1b IG00156 nodal stage | original full-band small-signal limits pass at 2x/4x after phase correction; ADJ p11 HP voltage discrepancy remains evidence, not fitted | `4917708`, `651c74d` |
| 1.2 Oscillator promotion | done; CPU over budget | `a6a574f` |
| 1.3 RC envelopes | done; first-assignment and Sustain II filter charge retained | `131f72b`, `c54c476` |
| 1.4 IG00151 VCA | done; model doc and mutation checks `0dfe4ee`; steal criterion met, retrigger 1.51 on one legacy-bank program (scorecard) | `5e857f5`, `1edc6c0`, `0dfe4ee` |
| 1.5 Gain staging | done | `228459c` |
| 1.6 Oversampling quality | done; Standard set to 2x from the measured alias | `ed877d4`, `ebbe75b` |
| 1.7 Engine default flip | done (plugin layer; INIT and toolbar wired) | `83f8a0e`, `230f367` |
| 2.1 Free-running voices | done; ported to the nodal stage and WSC at the merge; cold circuits warm | `69b8db6`, `342b9be`, `3961c1c` |
| 2.2 KAS scan and sample/hold | done (declared scan period, droop) | `2f274b3` |
| 2.3 Keyboard contour | done (declared key map; C2 conflict reported) | `7f36d1b` |
| 2.4 Touch | done (declared acquisition) | `aeb6719` |
| 2.5 Tolerances and drift | done (declared distributions) | `22b6854`, `a0f6dd5` |
| 3 SUB, ring, OE1/OE2, PRA | done | ws4 merge `195e5e3` |
| 3 Output modes and noise | done | `62e18c1` |
| 3 EXP volume optics | done | `3fcff91` |
| 4.1 Tone tabs | done as review records; cells stay hypotheses | `31e3bab` |
| 4.2 Memory panels | electrical model drives selected memory/PANEL banks in the circuit engine; interface/taper are declared inferences | `d66d298` |
| 4.3 Curated library | done: 301 circuit-engine programs + 222-program Legacy Library; recalibrated for the final audio | `9beed5e`, `82b9e14` |
| 4.4 Program Change, ribbon CC | done | `ec4d853`, `7c65aaf` |
| 5.1-5.2 Conformance audit, re-layout | done | ws7 merge `bd524f9` |
| 5.3-5.4 Toolbar, Studio view, interaction | done; modern and tuning pages mounted | ws8 merge `5af3b77`, `230f367` |
| 5.5-5.7 Readability, code health, UI tests | done; electrical programmer host oracles corrected without changed limits; full integrated UI smoke passes | `bd524f9`, `41363ea`, `61dad94` |
| 6 Modern layer | done (MSEG, LFO waves, matrix, FX, tuning, MPE, voice modes, MIDI learn, undo, A/B, randomise, host groups) | ws5 `5263172`, ws3, ws6, ws8 |
| 6 Separate chorus/tremolo tab positions | done; historic default preserves original exclusive switch, circuit source can select both off/on | `6f1c4aa`, pin `adc8bbb` |
| 6 Sample-accurate automation | VST3 native host gate done; AU scheduled adapter implemented with Linux contract gates, native macOS host/mutations pending CI | `3a37594`, `780658d` |
| 7 Release qualification | identified Linux validator-host lifetime mechanism removed by checked fixed-host pin; full continuation CI/release pending; signing remains owner-only | `f39af47`, `9af0b35` |

## Scorecard (2026-10-31, `00ceccf`)

`cs80_engine_audit --suite all --rates 44100,48000,96000` and
`cs80_source_factory_benchmark` on this container (4 cores, shared). Circuit
engine unless stated.

| Metric | Target | Before the plan (PDF) | Now |
| --- | --- | --- | --- |
| Worst saw/pulse non-harmonic, ring off | <= -70 dB | -33.8 dB (48 kHz) | **-88.3 dB** (48 kHz), -88.5 (44.1), -89.5 (96) |
| Ring-on rows | — | — | -43.0 dB at every factor: ws4's declared RMO carrier feedthrough, a spectral line, not fold-over |
| Worst tab/program peak at default volume | <= 0 dBFS | +3.9 dBFS | **-0.24 dBFS** (Guitar 1 + Guitar 2 tab, 44.1 kHz); met |
| Worst steal click | <= 1 | 1.17 | **0.92** (Harpsichord 1 + 2, 44.1 kHz); met |
| Worst retrigger click | <= 1 | — (1.26 at ws1's last run) | **1.51** (legacy-bank "Bass Stack" forced onto the circuit engine, 44.1 kHz) and 1.01 (Tone I: Guitar 1, 48 kHz; Legacy reads 1.05 on that row); 281 of 283 rows <= 1 |
| CPU, 8 held voices, 48 kHz, 256 frames | 0.15-0.20 RTF | 0.27 (source) | **0.51-0.61 median, p99 0.66-1.00, 0-12 deadline misses per 1024 callbacks** (min of 3 rounds; worst rows Funky 3+4 and the moving-tab/pressure/wah scenario); not met |
| Idle CPU | <= 2 % | 6-7 % | 0.113 RTF (11 %); not met |
| Legacy CPU, same row | — | — | 0.21-0.27 median |

The retrigger outlier ws1 traced to two same-pitch voices (Funky 3+4 at
44.1 kHz, 1.26) no longer appears: with free-running voices the circuit is never
reset at assignment and the VCA output is AC-coupled. The full audit finds a new
one, "Bass Stack" (a legacy-bank program rendered on the circuit engine by the
audit) at 1.51 against 0.63 in Legacy; not investigated to a cause yet (open). Note-off click scores (not part
of the plan's criterion) reach 1.65 (Funky 1 + 2, 48 kHz; 16 of 283 rows above 1, Legacy has 25 rows above 1) on percussive programs; open.

CPU: the reconstructed oscillator's event solver (about 29 % of instructions)
and the nodal stages' Newton solve (about 22 %) dominate. The bounded-error tan
was the one libm call worth replacing. The realistic route to the target is a
structure-of-arrays, segment-cached fast path for the oscillator (compute the
next branch event once per analytic segment rather than per sample); not done.


## Continuation: cold physical-time conservation

Audio `46a7a5a2ce077e95b9025de81da85b3480134176`, separate V2 pin
`6ab46dc8e1f75f4b559b279c54a254ace0480fbd`: the first idle scheduling
stagger no longer counts as elapsed physical time, and one real sample
seeds cold oscillator/filter controls. No fade, envelope law, shared RNG
sequence or 3 kHz idle cadence changes. The registered 2,121-check gate
goes from 1,928 failures to zero; worst elapsed error is 8.000711e-12 s
(limit 1e-8), cold phase error 6.82539e-7 cycles (limit 1e-4). The original
wake, cold filter and note-off gates pass. Legacy default/tails, V1 and
re-pinned V2 are byte-identical in `check-compat-gates.sh --strict-v2`.

[Cold-clock evidence](COLD_IDLE_TIMING.md) preserves three mutation
rejections and the original Bass full-rate comparison. The physical-PANEL
Bass metric remains above one at 48/96 kHz; the identified time defect is
fixed without claiming that every click score passes. The full integration
run also identifies a pre-existing touch-smoothing reference mismatch
(warm IL capacitor versus a fresh zero-charge capacitor); its numerical
limits remain unchanged and its fixture repair is being qualified separately.
New three-round CPU results and final full-suite qualification follow at
the performance integration checkpoint.

## Continuation: oscillator segment cache

Audio commit `6bf605592e162a1910c71f9cc5ebf7d014008527` caches the next
physical branch event for an analytic oscillator segment. The state, frequency,
threshold and idle-wake invalidations are explicit. Changing the search bracket
introduces only bounded numerical roundoff; this is still an intentional V2
change and has its own pin commit. The frozen legacy and V1 paths are unchanged.
See [KERNEL_CACHE_QUALIFICATION.md](KERNEL_CACHE_QUALIFICATION.md).

The independent continuation comparator measures maximum pin error
`3.3957e-10 V`, event-time error `2.39e-15 s`, and 307 root searches over 8,192
samples (`0.03748` per sample). All four cache-invalidity mutations are detected.
The limits were committed in `3bef95d10646c3317e8126d2db938bb8cdaa4b32` before
candidate evaluation.

Three 1,024-callback benchmark rounds before and after, at 48 kHz / 256 frames,
are archived under `docs/evidence/continuation-2026-10-31/cpu/`. They are fresh
same-session measurements on this shared cloud host, not replacements for the
older four-core scorecard above. Held-pair minimum-of-three median range is
`0.61189–0.71425` before and `0.58808–0.82658` after; moving-controls
`0.73268 → 0.78753`; quiet idle `0.13711 → 0.13328`. Some pairs improve by
4–7 percent, while other rows regress under shared-host contention. These
measurements do **not** establish the overall 0.15–0.20 target or idle ≤2%.
The compiler, source hashes, complete row distributions, CPU time and deadline
misses remain in the raw evidence. Subsequent kernel and engine-layout changes
are qualified separately.

## Continuation: constant nodal Jacobian cache

Audio commit `031a3330ca86e4ee2251ffa39bb665ebb7032d3f` caches only the
coefficient-dependent entries of the IG00156 scalar Jacobian. Nonlinear OTA
derivatives still follow the current signal; the existing `1e-7` residual
limit and four-step cap are unchanged. A converged starting estimate skips
unneeded Newton steps. Forced coefficient rebuild comparison is exactly zero;
maximum accepted residual is `9.99993e-8`. Four cache/residual mutations fail
the preregistered gate. A stale nonlinear-Jacobian alternative was rejected
because it was slower and failed an existing hard-noise row.

Three full 48 kHz / 256-frame rounds are archived beside the oscillator
measurements. Held-pair minimum-of-three medians are `0.58495–0.81223` RTF;
moving controls `0.77993`; quiet idle `0.13394`. Compared with the preceding
oscillator tree, this is not a material overall speedup. CPU targets remain
open. Strict compatibility passed after the separate pin commit
`85e048eac316969764acfb5791716412638ad33b`: both legacy captures and V1 remain
byte-identical, and V2 matches the published audio commit. See
[KERNEL_CACHE_QUALIFICATION.md](KERNEL_CACHE_QUALIFICATION.md) for the exact
oracle, residual guard and rejected alternative.

## Continuation: pipeline layout and exact receiver reuse

`12070683b1e77b98df28beec70488b84aef0b5b8` separates global, rank, mix and
output work and stores all sixteen chains as aligned arrays by rank field.
The sample arithmetic, voice order and RNG draw order are unchanged. Strict
compatibility on that frozen commit passes all four captures, including the
preceding V2 pin. The three-round held-pair range is `0.59516–0.82834` RTF,
quiet idle `0.13589`; the layout refactor alone does not establish a speedup.

`c3c0f5c250c83e74340c629d0f0e188631aee30e` reuses source receiver-network
readouts only when every input and resonance value is exactly identical.
All envelopes and TWS nodes continue to advance. Every parameter/selection
transaction invalidates the cache. The three-round held-pair range becomes
`0.55860–0.83918`, moving controls `0.80646`, quiet idle `0.09685` RTF. The
idle improvement is useful but remains well above the 0.02 target. Strict
compatibility on the frozen receiver-cache commit passes all four captures. Raw timings
for both changes are archived with their actual source commits.

## Continuation: full-band nodal accuracy

Audio commit `4917708268d8eed177670f4d270dbc50fc08e831`, followed by pin
`9246dbb2584eb2d87d744b7a52b43661bc7f15ce`, replaces the frequency-dependent
trapezoidal phase error with a fourth-order Padé phase correction and its
causal input-history term. The continuous network, OTA law and all previously
registered error limits remain unchanged. The full-grid assertions now cover
every registered cutoff, Q, configuration and rate; the earlier passing-subgrid
restrictions are removed. This strengthens the gate.

The 88.2 kHz full-band relative error improves from about `0.1403` to
`0.04884` (limit `0.08`), and the 192 kHz result from about `0.03495` to
`0.001954` (limit `0.02`). The worst core-band error is `0.008916` (limit
`0.03`). Independent full-matrix/direct measurements, nonlinear trajectories,
cache rebuilds and all 73,918 idle-wake checks pass; phase-removal and
input-history-removal mutations fail. See
[IG00156_PHASE_CORRECTION.md](IG00156_PHASE_CORRECTION.md).
Strict V2 verification on the separate pin commit passes all four captures:
legacy default, legacy tails and V1 are byte-identical to their frozen pins,
and V2 is byte-identical to the published phase-correction audio commit.

This accuracy correction has a measured CPU cost. Compared on snapshots with
the two kernel caches but before the pipeline/receiver changes, held-pair
minimum-of-three medians rise from `0.58495–0.81223` to `0.73705–1.09177`,
moving controls from `0.77993` to `1.04817`, and quiet idle from `0.13394`
to `0.14053`. These are software measurements on this shared cloud host;
the CPU target is not met and further optimization remains necessary.

The native ADJ review confirms the printed-p11 setup (PDF page13), including
its CP3 probe and VR7/VR8 adjustment sequence. The HP amplitude discrepancy is
retained. M41 also shows the LP AI 150 kΩ as a shunt to ground, conflicting
with the current declared series-source interpretation. Neither observation
is curve-fit into a gain correction. See
[evidence/continuation-2026-10-31/NATIVE_FILTER_EVIDENCE.md](evidence/continuation-2026-10-31/NATIVE_FILTER_EVIDENCE.md).

## Continuation: electrical memory and PANEL routing

Audio commit `d66d2985c93e1ec0fa9788bebf667c2919d02b50` connects the
existing memory-panel electrical model to both circuit-engine ranks. The
explicit 26-row programmer adapter covers the four memories and PANEL;
legacy and unannotated physical-unit core frames retain their previous path.
No parameter or stored-memory schema is changed. The pot coordinate, taper,
50 kΩ resistance and loading are declared model inferences with alternatives,
not hardware measurements or a fitted correction.

The independent electrical gate passes **840 checks**, with bus error
**0.184 mV** (limit 2 mV), diagonal impedance error **0.00623%** (limit 1%),
and equivalent-source error **4.44e-16 V** (limit 1e-9 V). All seven
preregistered mutations fail. The exact note-off capacitor-continuity gate
still passes all 540 checks. After the separate V2 pin commit, the complete
strict compatibility run passes legacy default, legacy tails, V1 numerics and
V2 audio. See the archived `compat-programmer-frozen.log`.

The phrase diagnostic's Bass Stack retrigger score becomes
**0.281583 / 0.297915 / 0.391481** at 44.1 / 48 / 96 kHz. This improvement
is a consequence of the loaded programmer's changed envelope response; it is
**not** evidence that the separately identified cold-envelope charge defect
has been fixed. That defect and its correction are qualified independently.
Funky 1+2 note-off scores remain **1.495963 / 1.584353 / 1.246159**;
omitting note-off produces **1.495821 / 1.615021 / 1.275743**, with exactly
matching pre-event samples and no event-time physical-capacitor jump. The
original note-off score criterion remains open. No output fade was added.

Three 1,024-callback benchmark rounds at 48 kHz / 256 frames are archived
with the preceding phase-correction rounds. The held-pair minimum-of-three
median range is **0.74404–1.05964 RTF**, moving controls
**1.03699**, quiet idle **0.13826**. These isolated DSP snapshots do
not include the separately qualified pipeline/receiver optimizations.
The CPU target remains open.

The coordinate, electrical equations, mutation results and causal comparison
are in [PROGRAMMER_INTEGRATION.md](evidence/continuation-2026-10-31/PROGRAMMER_INTEGRATION.md).

## Continuation: filter-envelope charge continuity

Audio commit `c54c476c2ee8a3d332846579ce9433f219fe996c` removes a genuine
zero-time state discontinuity. The old first assignment replaced zero stored
filter-envelope charge with IL: **1.440 V** on the original physical PANEL
fixture, **0.590678 V** after the loaded programmer mapping. The corrected
untriggered idle state approaches IL through the existing release RC law,
and every circuit assignment preserves the current charge. Sustain II also
retains released filter charge. Legacy reset behavior is unchanged.
No new time constant, affine offset, warm-state shortcut or output fade is used.

The registered gate passes **1,519 checks**, and all six deliberate continuity
mutations fail. The existing active-tail gate passes **49,732 checks**, note-off
continuity **540**, source integration **135,739**, and the unchanged wake
accuracy bounds pass **73,965 checks**, worst residual **−43.08 dB** against
−40 dB. The original Sustain II assertion has been made policy-explicit: all
16 circuit capacitors retain charge in both circuit policies and all 16
Legacy capacitors still reset. See [COLD_FILTER_ENVELOPE.md](COLD_FILTER_ENVELOPE.md).

Bass Stack's new-programmer retrigger scores are
**0.281643 / 0.297817 / 0.390698** at 44.1 / 48 / 96 kHz. The small change
from the preceding programmer-only scores correctly separates charge
continuity from the much larger programmer response change. Funky 1+2
note-off scores remain **1.582095 / 1.569018 / 1.227796**, while the same
render with note-off omitted gives **1.581942 / 1.601897 / 1.258930**.
The original score gate remains open even though the event-time capacitor
continuity regression is exactly zero.

Three 1,024-callback rounds at 48 kHz / 256 frames are archived. The
held-pair minimum-of-three median range is **0.76142–1.10352 RTF**,
moving controls **1.05485**, quiet idle **0.14609**. These snapshots
share the preceding DSP-only base; separately qualified CPU optimizations
are integrated and measured independently. After the separate pin commit,
strict compatibility passes all four complete captures: both Legacy traces,
V1 numerics, and V2 audio against the published charge-preservation commit.

## Continuation: complete host/UI ownership and live monitoring

`3c08e8e8d25f0ee1427f2467a199f37940a2ea3d` feeds the monitor from coherent
engine taps and includes all sixteen physical voices when the second engine
is active. The physical slot selector updates immediately after a capacity
change and has explicit keyboard focus order. Output metering and tagged
on-screen key pressure include both engines; assignment serials prevent a
stolen or reused slot from accepting an old pressure gesture. Pure remains
eight voices and keeps its modern-feature masks. Telemetry is read-only and
only collected when the monitor is enabled.

Earlier UI commits `0d9f522` and `2c0866e` add balanced host gestures around
undo/redo and complete context menus/MIDI learn for APVTS-backed switches,
combo boxes and memory miniatures. All four production APVTS copy/replace
sites explicitly synchronize under the undoable state's tree lock.
`9af0b35` gives graphics resources instance-owned teardown; this is defensive
plugin cleanup, separate from the diagnosed validator-host defect.

The dedicated UI continuation passed the entire real-window smoke suite:
**151 pressure-ownership checks**, all new focus/gesture/monitor checks,
seven deliberate UI/ownership mutations, teardown loops and both original
image goldens (0.0000% difference). The modern-control audit has 314 controls
with zero undersized targets; the 26 existing hardware exceptions remain
explicit. Combined-feature UI/native-host qualification and final screenshots
are repeated on the frozen integration tree. The complete strict audio check
after the separate V2 pin passes both Legacy captures, V1 numerics, and V2
against the published ownership commit. See
[workstreams/ui-completion.md](workstreams/ui-completion.md).

The electrical memory-programmer integration exposed **385 stale host-fixture
assertions**. They assumed raw physical PANEL/memory targets, cold filter
reset to IL, zero receiver readouts outside factory tabs, or a continuous
five-position chorus parameter. Preregistered test-only commit `61dad944b46e1fe9ff80afe988343f5c24880181`
uses independently selected APVTS banks, separate primitive envelope
histories and stored-bit exact receiver checks. All 30 direct test APVTS
snapshots now synchronize under the undo tree lock. No production behavior
changed. The unchanged complete UI smoke on the combined AU/VST3 tree
passes with **zero failures**, both original goldens **0.0000%** different,
314 modern targets and none undersized. Four deliberate mutations reject
(348/594/67/2 assertions); restored sources pass again. See
[workstreams/ui-programmer-oracles.md](workstreams/ui-programmer-oracles.md)
and the complete integrated log
`evidence/continuation-2026-10-31/ui-integrated-au-root-smoke.log`.

## Continuation: exact nodal coefficient setup

`44d11d50584c7c4800775e4234ff5dc2e1ab62ab` reduces the phase-corrected
filter's coefficient work: sparse fixed-network products, one setup for
the selected clock method, and a nonlinear Jacobian only after a failed
`1e-10` residual check. The original Padé phase correction, OTA nonlinearity
and all full-band limits remain unchanged. A frozen pre-refactor continuation
compared **960,000 output floats bit for bit**, all equal. The four existing
filter/cache/wake suites pass without adjusting any limit.

Three complete 48 kHz / 256-frame rounds of 1,024 callbacks per row are
archived. The held-pair minimum-of-three median range is
**0.75730–1.00781 RTF**, with moving controls
**1.04121** and idle **0.14414**. Against the preceding cold-filter
DSP snapshot, the worst-pair median decreases from 1.10352 to
1.00781; the easiest pair is effectively unchanged. This is a
modest speedup, and the CPU target is still open. The strict frozen audio
captures passed both Legacy traces, V1 numerics and V2 bytes against the
unchanged ownership pin `3c08e8e`; no circuit sound or golden was re-pinned.

See [IG00156_PHASE_CORRECTION.md](IG00156_PHASE_CORRECTION.md) and
[evidence/continuation-2026-10-31/filter-coefficient-cache-qualification.txt](evidence/continuation-2026-10-31/filter-coefficient-cache-qualification.txt).

## Continuation: independent physical chorus and tremolo tabs

`6f1c4aad967e58fcd013747ceaa6bf00b6dcc7d9` adds an append-only
`chorus_tabs` engine parameter (AU version hint 16, choice number 191) with
Historical, Both Off, Chorus, Tremolo and Both On positions. Historical is the
neutral default and keeps the older `chorus_on` choice and the original Legacy
toggle behavior. The explicit tab states drive the separate OE1 and OE2
circuits in Source and PURE CS-80; the UI exposes both independent tabs and
keeps the earlier legacy gestures. The prior circuit audio is retained at the
default. Host parameter parity counts have been updated, without renumbering
any earlier parameter.

The pre-registered source matrix covers both ranks, all explicit combinations,
Legacy and PURE policy behavior, and the 144,037-sample continuity oracle.
Five deliberate source mutations fail this gate. The focused and complete
real-window UI regressions both pass after the electrical programmer oracle
update, without adjusting any limit. The separate audio pin is
`adc8bbb8de4cd8631423e1edcfb513be841fac1c`. See the
frozen capture under
`evidence/continuation-2026-10-31/compat-chorus-frozen.log`.

## Continuation: VST3 sample-accurate host automation

`3a37594b11c8dd696e5e3e622d8dd60a3f36480e` bridges every point of
Steinberg's native `IParamValueQueue` into Source engine control targets at
the intended sample. The 18 enum-named keys cover cutoff, resonance, VCA
level, pulse width, global volume/mix and tuning on both ranks. Every timed
change reaches both engines in sixteen-voice mode. The original JUCE 8.0.11
wrapper remains pristine; an exact source-hash gate precedes generation of
the reviewed build-tree wrapper. Moving ramps set control targets per sample,
while constant spans remain batched. No parameter or state IDs change.

The preregistered `host-automation.json` tolerance commit `6807df1`
precedes evaluation. The independent curve gate reports normalized error
**2.22045e-16** against `1e-7`. A native Linux VST3 SDK host loads the
built module, feeds native automation queues and MIDI, and compares
1/7/64/256/1024/4096-frame partitions with a per-sample oracle at **zero
audio difference**, including sixteen voices. The bridge preserves every
zero-queue rendering byte; Legacy and PURE retain their previous JUCE
last-point behavior. Deliberate queue/bridge/source-ownership mutations
are detected. The local rebuilt native CTest pair passed **2/2**. See
[HOST_AUTOMATION.md](HOST_AUTOMATION.md) for protocol references,
fixtures, and the pending native macOS Audio Unit gate.

The separate pin is `aaf58559b5e761c7abfa47e2d2b40ff038de34e2`.
The frozen strict capture passes all four gates: Legacy default/tails exact
against `d56911a`, circuit V1 exact against `22d6a32`, and V2 exact against
the published audio commit `3a37594`. Full results are archived at
`evidence/continuation-2026-10-31/compat-vst3-frozen.log`.

## Continuation: Audio Unit scheduled control offsets

`780658da1e83012ae4c7e39658d261d432b005e3` extends the verified VST3
timeline to Apple's Audio Unit v2 scheduled immediate and ramp events. A
separately reviewed JUCE 8.0.11 wrapper, generated only in the build tree,
reports bounds, scope and capacity errors instead of dropping events.
The 18 key controls advertise ramp capability; other parameters retain
JUCE's earlier behavior. A 2,048-event preallocated queue resolves signed
ramp starts, overlaps and buffer offsets without allocation in the audio
callback. PURE and Legacy retain their block-start host semantics.

The Linux AU curve oracle passes all registered block partitions at maximum
normalized error **2.22045e-16**, and the production hook body passes its
AU protocol contract fixture. Ten deliberate curve/hook mutations fail.
The actual macOS `AudioUnitScheduleParameters` component-host test and its
five serial component mutations are CI gates; **they must pass before AU is
called fully qualified**. The native VST3 host gate remains green after the
shared-interface extension. The separate circuit pin is
`068456506d8bb9c8606059ae551045b4d7de4e08`. Its frozen strict
capture passes all four gates: Legacy default/tails byte-identical to
`d56911a`, circuit V1 exact against `22d6a32`, and V2 exact against
the published AU audio commit `780658d`. The full capture is archived at
`evidence/continuation-2026-10-31/compat-au-frozen.log`.

## Pinned compatibility gates

- `dsp-measurements.yml` **legacy-compatibility**: default-off audio byte-identical
  to `d56911a`. Legacy must never change.
- `dsp-measurements.yml` **source-factory-compatibility**: circuit V1 numerics
  (`22d6a32`) and circuit-engine audio V2, now
  **`780658da1e83012ae4c7e39658d261d432b005e3`** (AU scheduled
  host timing). Pin commit `068456506d8bb9c8606059ae551045b4d7de4e08`.
  Earlier V2 pins, for historical circuit-engine renders: `3a37594`
  (VST3 host timing), `6f1c4aa` (independent
  chorus/tremolo tabs), `3c08e8e`
  (complete live voice ownership), `c54c476` (filter
  charge continuity), `d66d298` (electrical
  programmer), `4917708` (full-band
  nodal phase correction), `031a333` (Jacobian
  cache), `6bf6055` (oscillator
  event cache), `00ceccf` (integrated
  model),
  `f6b126d` (Phase 1.1 filter), `131f72b` (Phase 1.3 envelopes), `228459c`
  (Phase 1.5 gain staging), `6e5892f` before that.
- The original integrated strict V2 check passed at `ef9bc35`. The oscillator
  continuation passed `scripts/check-compat-gates.sh --strict-v2`: legacy default,
  legacy tails and V1 numerics remain byte-identical; V2 is byte-identical to
  the published oscillator audio commit `6bf6055`. Complete repeated captures
  were used after two interrupted output files; shared portions of those
  interrupted files were also byte-identical, but were not counted as passes.

### CI on the integrated tree

At `e048dca` (the head merged into `main` as `ec72eed`):
[build 41208373099](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41208373099)
passed all six jobs (Linux, Windows, macOS arm64 and Intel builds and tests;
pluginval strictness 10 on Linux VST3 headless, GUI and seeded, Windows VST3,
both macOS VST3, AU and AU GUI) and
[DSP measurements 41208373089](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41208373089)
passed all five jobs, including the ASan + UBSan suite (72 tests). Fixed on the
way, without changing an assertion:

- The sanitizer build had stopped at a GCC 13 UBSan constant-evaluation rule in
  `FactoryToneHypotheses.h` since the ws6 merge (`fd0e851`); three tests then
  outlived Release-build timeouts under the sanitizers, scaled x4 there by
  `CS80_TEST_TIMEOUT_SCALE` (`a2b2452`).
- macOS-only UI-smoke failures: ws8's first-run tour opens by itself in a real
  window and covered the keybed of the real-pointer fixture whenever macOS
  window activation outlasted the tour's first timer tick (diagnosed in
  `b2c2302`, fixed in `e048dca`; the window also floats above other
  applications, `a721935`). The same tour covered the 1120x700
  source-performance screenshot (`a26f583`). The F1 help-card check raced
  JUCE's dismissal of call-outs in a background process (the CI harness is
  never the foreground application on macOS); it now checks before any
  message is dispatched (`df4ba22`, after macos-15-intel failed it on `main`
  at `ec72eed`).

On `main` after [#3](https://github.com/cheshmakzanoon-ops/yamaha-cs80/pull/3) (`55bb043`):
[build 41218764120](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41218764120),
[release 41218772353](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41218772353)
(unsigned: no signing secrets) and
[DSP measurements 41218763836](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41218763836)
passed every job; the README links their artifacts.

A passing Linux pluginval run is not a fix for the intermittent exit 139.

### Intentional circuit-engine sound changes in the `00ceccf` pin

- ws1: nodal IG00156 stage (frequency-dependent resonance, 1 uF coupling corners,
  OTA limit on the gm part, calibrated cutoff); reconstructed oscillator (WSC
  rising saw, comparator pulse, shaped sine, reset plateau, coupling memory,
  8 internal samples of delay); IG00151 VCA (SUB tremolo in dB, LI smoothing,
  AC-coupled -52 dB pedestal, constant-power pan); plan 2.5 card tolerances and
  1/f drift instead of the 22-cent sine.
- ws2: 5 ms control smoothing; shared SUB29 noise with declared PSDs; optical
  expression gain; GENERAL/PHONES outputs (selectable; default unchanged).
- ws3: free-running voices with warm cold circuits; Sustain-I hold and droop;
  ninth-key reconsideration; KBC contour network; TWS a/b/C and TRG5 bus.
- ws4: IG00150 SUB converter; ring converter and offset multiplier; OE1 tremolo;
  OE2 L/R paths, loss, hold, noise, clock feedthrough; PRA optics.
- Integration: Standard oversampling 2x (ws2 had 4x); bounded-error tan.

## Integration decisions

- Fresh instances: factory program 0 on the circuit engine with the original SUB
  panel (ws2). INIT (`loadInitPatch`) is the same kind of start: the factory
  "Init Patch" on the circuit engine with the original SUB. The curated
  "Init Circuit" stays a library program.
- Factory recall zeroes the 16 matrix amounts (they are parameters) and keeps the
  matrix routing (sources, destinations, via, ranks), which is session state like
  MIDI learn; a recalled program is therefore modulation-neutral until an amount
  is raised.
- Drift model is a physical-setup parameter (the unit's condition): program recall
  keeps it.
- The goldens show the circuit-engine default.

## Open issues

- Final cross-platform continuation build/pluginval, DSP and screenshot CI and
  release dispatch remain to be completed. The historical Linux RunLoop
  lifetime mechanism has been identified and removed from the pinned validator
  host; see [VALIDATOR_HOST_LIFETIME.md](evidence/VALIDATOR_HOST_LIFETIME.md).
- CPU above target (above). Idle above target.
- ADJ p11 HP setup voltage remains discrepant; native evidence and the M41
  topology conflict are recorded. Full-band small-signal numerical limits now pass.
- Note-off click scores above 1 on some percussive circuit-engine programs.
- MTS-ESP is vendored and its native/shim/fallback tests pass locally
  (`fa2016c2`, upstream 0BSD licence); final cross-platform plugin CI
  qualification remains in progress.
- All 25 tone-tab cells have native-crop review records; unresolved markings
  and board revision choices remain labelled hypotheses. Independent physical
  chorus and tremolo states now have a neutral-default, append-only parameter;
  complete integrated host/UI smoke passes locally; CI is pending.
- Sample-accurate VST3 key automation is published and qualified with a
  native host SDK. AU scheduled automation is implemented and Linux
  curve/hook gates pass; native macOS component tests and serial mutation
  gate must pass in CI before declaring AU complete.
- Signing and notarization need the owner's certificates; real-DAW sessions,
  trademark review and AAX are owner actions
  ([release/RELEASE_QUALIFICATION.md](release/RELEASE_QUALIFICATION.md)).

## Continuation: automation state-boundary repair

Audio commit `647426521fefcc2d5e31f5d149d19e749205ee43` and separate V2
pin `32260112971289761f9a6e8e7289916bfc7df95f` repair delayed host
queues after an external parameter or state change. The processor refreshes
an automation lane only when the parameter's actual readback differs from
its last published readback. This preserves the accepted double-precision
trajectory across empty blocks while recognizing a later UI, preset or host
state edit. No parameter is added and Pure/Legacy policy is unchanged.

The acceptance contracts were published in `ce8b127` and `23270c7` before
repair evaluation. The processor witness changes from 336 failures to zero;
maximum normalized error changes from `0.45365307582292042` to
`2.2204460492503131e-16`. The native Linux VST3 boundary comparison changes
from maximum audio error `0.000974897` to exactly zero. Partition, sixteen-
voice and inert-reader comparisons also return zero. Actual-source mutations
for stale refresh, repeated Float32 projection and a state-writing native
reader are rejected. The original native oracle and tolerances remain intact.

Evidence and full raw logs are in
[evidence/continuation-2026-10-31/automation-state-boundary/](evidence/continuation-2026-10-31/automation-state-boundary/).
The separate Intel AU endpoint-publication defect is still being qualified;
it is not covered by the successful VST3 result. Its original normalized
limit remains `1e-7`. The strict compatibility log attached to this checkpoint
records both Legacy captures and V1 numerics unchanged and V2 matching the
published audio commit. This host repair does not establish the CPU target.

## Qualification: exact lazy filter histories

Commit `476c2f1fed4dcec18c5a42bc8d29ae542dab02b9` defers the
IG00156 interchange-history reconstruction until a state read, coefficient
change or clock transition actually needs it. The corrected audio solver
continues to use the same physical nodes, previous OTA currents and input
history. The frozen comparator checks 52,408,128 values with zero bit
mismatches; seven actual-source mutations are rejected. Steady 2,048-sample
processing performs zero unused reconstructions and one subsequent state
export performs exactly one. Moving coefficients retain the required work.

At that exact source revision, all 90 core tests pass. The strict compatibility
script reports both Legacy captures and V1 numerics byte-identical and V2
byte-identical to `46a7a5a`, so this exact optimization needs no audio re-pin.
[Qualification evidence](evidence/qualification-2026-10-31/lazy-filter-histories/)
keeps the complete core/compatibility logs. Fresh starting-main and inherited
three-round CPU baselines are in
[evidence/qualification-2026-10-31/cpu/](evidence/qualification-2026-10-31/cpu/).
The after-change timing is recorded at the next CPU checkpoint; the overall
performance targets remain open until measured.

## Qualification: synchronized state and exact MSEG restore

Audio commit `bbc57fe4aa912180cd672f6853cf146737a1a761`, followed by
separate V2 pin `c487177cc1ff251f4cf0e3a0ec55094b7e152138`, makes
host snapshots, modern-layer edits and undo history use the common APVTS
state lock. Worker snapshots synchronize external edits and overlay current
parameter atomics on a detached tree; an unfinished user gesture retains
its message-thread undo ownership. The audio mailbox still uses a try-lock
and never waits on these tree operations. Undo/redo preserves host gestures.

MSEG coordinates now serialize with enough significant digits to recover
every admitted Float32 value. The old six-decimal writer changed a tested
time coordinate by nine ULPs and a level by one ULP after redo. The live
edit, undo/redo and session restore now retain the same values. This is an
intentional modern circuit-engine restore correction; Pure remains inert.
No host parameter or state-schema field is added.

The preregistered state fixtures pass, including 1,701 host-value cases
(161 exposed the extra normalized round trip), 14 MSEG coordinate cases
with nine admitted subnormal fields, and zero bit mismatches. All 14
actual-source mutants are rejected. A fresh integration build against the
repaired automation headers passes both state-tree and automation-boundary
checks; the existing full UI suite also passes. See
[STATE_TREE_SYNCHRONIZATION.md](STATE_TREE_SYNCHRONIZATION.md) for raw logs,
source hashes and separately labelled environment setup failures.

The strict compatibility checkpoint retains all four original captures:
Legacy default/tails and V1 numerics remain byte-identical; V2 matches
`bbc57fe`. The local script now runs each independent baseline/current
capture pair concurrently, waits for both processes, and fails if either
fails. It does not cache or omit a render, change a comparison or relax a
threshold; injected failures on either side verify propagation.

The completed lazy-filter timing checkpoint is also recorded here: held
paired-tone medians `0.647884–0.926200` RTF, quiet idle `0.100304`, moving
controls `0.927262`, minimum of three unchanged 48 kHz / 256-frame rounds.
The target remains unmet. The dormant-noise small-mode fault-injection
witness was registered before its additional evaluation. The intended AU
coherent-publication extension was absent from this checkpoint: its first
supplementary local evaluation is retrospective. The correction below
records the actual publication order; no earlier result is relabelled.


## Qualification: AU accepted endpoint ownership

Audio commit `6300163e8272feb9973e99a87d3e47d09783cb50`, followed by
separate V2 pin `7c9c116a0cbc05e62dc9f6767cee195189248c90`, preserves the
accepted normalized Float32 AU endpoint alongside its physical parameter
readback. A coherent atomic publication and revision ownership prevent an
older setter from reclaiming a newer write. External edits and lifecycle
changes invalidate publication; the native setter, scheduling queue, VST3
path and Pure audio law are unchanged. This fixes the measured normalized
round-trip error without relaxing the original `1e-7` endpoint tolerance.

The integrated AU bridge and macro-environment tests pass, and the strict
compatibility script reports Legacy default, Legacy tails and V1 circuit
numerics byte-identical to their original pins. V2 is byte-identical to
`6300163`. Native Intel and ARM host results are still pending at this
checkpoint; local bridge results do not substitute for those native gates.
See [AU endpoint evidence](evidence/continuation-2026-10-31/au-intel-endpoint/).

Publication correction: the original limits were committed in `aa3cadb`
before evaluation. The intended supplementary concurrency/identical-state
contract extension was accidentally absent from `9dd7762`, so its first
local evaluation is retrospective. The actual extension first appears in
`bf817e51256037ef63c91efa46dafee853efce8a`, before the repaired native
runs. The original failed integration log, original qualification record
and explicit correction notice remain in the evidence. No tolerance was
changed and no rerun is presented as repairing the earlier chronology.

The separate exact BBD/PRA CPU checkpoint `f79b6f7` has now completed three
unchanged factory-benchmark rounds: held medians `0.664905–0.973445` RTF,
quiet idle `0.084363`, moving controls `1.011854`. Idle improves from the
previous `0.100304`, while held and moving timings rise on this shared
executor. Neither CPU target is closed. All raw rounds, source hashes and
the current-pin strict capture are retained in the qualification evidence.

## Qualification: silent rank cadence and exact V2 re-pin (2026-11-01)

Circuit-engine audio commit `18400c29a54ac2204564b355c0e52ffd3b0dcb8e`
changes the free-running rank oscillator/filter idle cadence from 3 kHz to
1.5 kHz. This is a declared software scheduling approximation, with no
claim of a hardware clock or component measurement. Legacy processing and
the awake circuit equations are unchanged. The published acceptance limits
and fixtures predate the candidate; they were not adjusted or skipped.

The original free-running gate records **73,965 checks, zero failures**;
the worst wake residual is **−43.33 dB** against its existing **−40 dB**
limit. Cold elapsed time has 2,121 checks, maximum error
`3.89466237039e-12 s`; the cold filter and note-off gates have 13,966 and
540 checks, respectively, with zero failures. An actual-source 750 Hz
over-decimation mutant fails two original wake assertions, and the restored
1.5 kHz implementation passes them. The original integration passed all
four focused CTest suites. The measured three-round CPU comparison follows
as an independent checkpoint; no held or idle CPU target is claimed here.

The three unchanged 48 kHz / 256-frame, 1,024-callback factory-benchmark
rounds are now archived in
[cpu/rank-idle-1500](evidence/qualification-2026-10-31/cpu/rank-idle-1500/).
Minimum-of-three held medians are **`0.621701–0.941828` RTF**, selected-round
p99 **`0.986629–3.543655`**, with **1,305** held deadline misses. Quiet idle
is **`0.069984` RTF** and moving controls **`0.890971`**. The immediately
preceding unused-LFO checkpoint had held `0.706473–1.148390`, idle
`0.128088` and moving `1.056194`; however unrelated Rust compilers occupied
several cores during this shared-host comparison, and the paired Legacy
control also varied. These are real raw measurements with intact three-round
selection, but **they do not prove an isolated speedup**. The unchanged
0.15–0.20 held and ≤0.02 idle targets remain **open**. A separately
pre-registered, immutable same-runner CPU workflow (`0035ef2`, `afd2b37`,
runner `840496f`) is collecting more comparable evidence.

**Three-commit audio sequence:** the audio commit above was pushed first;
separate commit `9f990ae12d999b46407d0b34ca18d23dbaf029fb` then set
`SOURCE_AUDIO_V2` to the full audio SHA; this status/evidence commit follows
the strict re-pin. `scripts/check-compat-gates.sh --strict-v2` succeeded:
Legacy default and tails remain byte-identical to `d56911a`, V1 circuit
numerics remain byte-identical to `22d6a32`, and V2 now matches `18400c2`
byte-for-byte. See the [rank cadence evidence](evidence/qualification-2026-10-31/rank-idle-1500/)
including the archived strict gate log.


## Qualification: native Intel BBD history ownership (2026-11-01)

Apple Clang 17 rewrote the stored-bit BBD history predicate to a floating
comparison. With DAZ enabled, that comparison misclassified stored
subnormal history as zero. Audio commit
`1f842b9a18c117a31c09e7d4dae1aaa26c8dce4e`
keeps the predicate in the integer domain through an explicit volatile
integer observation; no floating environment or BBD equation is changed.
The native Intel diagnostic has 1,245 checks: original 1 failure, repair
0 failures, reintroduced floating-comparison mutation 1 failure. Its
16-case DAZ/FTZ probe records 4 original and 0 repaired misclassifications.

The audio commit was published first; separate pin commit
`475063ca4b5640c1b17cb45eb89a23706ad18873` sets `SOURCE_AUDIO_V2` to its
full SHA. The strict compatibility run then passed all four captures:
legacy default/tails and V1 numerics remain byte-identical to the original
pins; V2 is byte-identical to the new audio commit. This is the third
evidence/status commit in that sequence. The full core and plugin suites
are still running at this checkpoint, and the CPU and click targets remain
open. See the [native compiler mechanism and raw evidence](evidence/qualification-2026-10-31/bbd-native-predicate/README.md).
