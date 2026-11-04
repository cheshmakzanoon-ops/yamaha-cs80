# Current status

This page states the project's current implementation and qualification. It wins
when older documents disagree. Historical observations remain in
[the dated status archive](STATUS_HISTORY_2026-11-01.md) and the linked raw evidence.

Last updated: **2026-11-02 UTC**. Current production change:
**`83b7f7670f000a95c6b44f96ffb0df3894755d14`**, on
`finish/cs80-qualification-20261031`, based on original main
`db24a47170610f81236e8829ef5536af69dfc94d`.
[PR #6](https://github.com/cheshmakzanoon-ops/yamaha-cs80/pull/6) remains the
landing PR. The final registered three-rate engine audit now has a complete, independently
checked 885-row artifact at source `39ae6c2684456950302cb4ed10ecfae980c37ddd`
([run 41336194253](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41336194253)).
Its numerical musical-click gates still fail, even though the capture workflow
succeeded. Feature-branch build push/PR runs 41338140448/41338144467 passed at
`7c864f2`; the WSC-oracle test-only commit `5809a528` has since advanced
PR #6's development branch without changing production DSP, and its build,
DSP and screenshot runs 41339574562/41339574637/41339574530 passed.
A later feature-branch screenshot run 41369573898 also passed at `5809a528`
and published preview provenance; checked-in `main` screenshots still reference
historical `55bb043`.
These are not final-main qualifications. The CPU
budgets, musical clicks, main merge and new release remain unfulfilled.

## Current assessment

The requested oscillator/Jacobian caches, processing split, sixteen-chain
component arrays, free-running idle reductions, filter charge/time fixes,
full-band nodal accuracy, proven Linux validator-host repair, electrical
programmer, independent original chorus/tremolo switches, native automation,
MTS client, parameter interactions, undo gestures and two-engine telemetry are
implemented and qualified within the scopes below. The native Intel stored-bit
BBD repair and exact zero-history FIR optimization are also published. The
free-running ring phase and precise envelope charge are retained at key events.
The VCA coupling output now remains connected through envelope retirement;
explicit reset and hard panic clear its stored charge. Charged inactive tails
also consume current-sample modern pan while preserving original scheduled
trigger/expression ownership. The complete full-bank audit is reported below; source musical-click gates remain red.

**The held and idle CPU budgets and the original physical-PANEL Bass/percussive
note-off score criteria remain open.** Inspection of all 25 disputed tone cells
is complete, but historical installed-value/revision uncertainty remains
labelled. The ADJ p11 HP amplitude discrepancy also remains evidence, without
an invented correction. None of the source or software results is a hardware
measurement or complete analog-equivalence claim.

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

Which implementation runs today, by subsystem. Circuit audio rows cover
Source Performance and PURE CS-80 unless a row specifies its extension
policy. Host and UI rows document software ownership.

| Subsystem | Legacy | Circuit engine / current implementation | Evidence / remaining boundary |
| --- | --- | --- | --- |
| Oscillator | Frozen PolyBLEP path | Reconstructed WSC with analytic-segment branch-event cache; explicit state/frequency/threshold/wake invalidation | [Kernel qualification](KERNEL_CACHE_QUALIFICATION.md); audio 6bf6055. Numerical continuation qualified; whole-engine CPU target remains separate. |
| HP/LP filters | Frozen TPT SVF | IG00156 nodal stage with constant Jacobian reuse, residual-guarded Newton steps, full-band Padé phase correction, exact coefficient setup and lazy interchange histories | [Phase correction](IG00156_PHASE_CORRECTION.md), [lazy-history evidence](evidence/qualification-2026-10-31/lazy-filter-histories/). Original small-signal limits pass; HP service-point amplitude remains open. |
| Envelopes and idle voices | Historical reset/freeze semantics retained | Existing declared RC law preserves filter charge at first/reused assignment and Sustain II; cold scheduling counts only elapsed physical time; all sixteen chains retain their free-running model | [Cold filter charge](COLD_FILTER_ENVELOPE.md), [cold time](COLD_IDLE_TIMING.md). Original physical-PANEL click score is still a separate unmet criterion. |
| VCA | linear multiply | IG00151 LI/EI inputs, pedestal and existing AC coupling; the closed-envelope capacitor discharge remains connected to the mix at audio rate, with no duplicate idle decay | Coupling audio `b75b4d6`, current-sample tail-pan audio `83b7f76`, pin `14ef653`; [handoff investigation](evidence/click-20261101.md). The 0.1 s pole remains the existing declared inference; no fade or new component fit. |
| Voice variation | uniform tune/pan trims, 0.031 Hz sine drift | declared 5 % R / 10 % C card draws into tune, cutoff, Q, level, EG times; 1/f thermal drift with warm-up; sine as "worn unit" (`ToleranceModel.h`) | [TOLERANCE_DRIFT_MODEL.md](TOLERANCE_DRIFT_MODEL.md) |
| Keyboard | MIDI note to channel | KAS scan option, SH7 hold and droop, ninth-key reconsideration, KBC contour network | ws3 report, `KasScanner.h`, `KbcContour.h` |
| Touch | raw MIDI velocity/pressure | TWS a/b/C nodes, TRG5 diode-OR bus | ws3 report, `TouchCircuit.h` |
| Tone tabs | Frozen bridge values and V1 numerics | Seven complete and fifteen inferred tones; all 25 disputed cells have page-cited native readings and explicit alternatives | [Native cell review](evidence/continuation-2026-10-31/TONE_CELL_NATIVE_REVIEW.md), source 5a3027d. All 25 retain hypothesis status; fingerprint e5f877a025fc52c6 unchanged. |
| Memory/PANEL controls | Previous APVTS path retained | MemoryPanels electrical receiver feeds the selected memory/PANEL controls of both circuit ranks | [Programmer integration](evidence/continuation-2026-10-31/PROGRAMMER_INTEGRATION.md), d66d298. Pot coordinate, taper and loading remain declared inferences. |
| SUB | legacy `Lfo` or `OriginalSubOscillator` | IG00150 exponential converter, band-limited saw/square | [ORIGINAL_SUB_OSCILLATOR.md](ORIGINAL_SUB_OSCILLATOR.md), ws4 report |
| Ring modulator | provisional law, ideal multiply | ADJ p19-anchored converter and declared feedthrough; carrier phase and actual double envelope charge survive key events | Audio `481c7e9`, pin `8a85faa`; [source topology and continuity contract](RING_EVENT_CONTINUITY_PROPOSAL.md). Legacy retains historical event semantics. |
| Chorus/tremolo | Historical exclusive toggle retained | Separate OE1/OE2 states, including both off and both on, through append-only chorus_tabs; Historical is the neutral default | 6f1c4aa; AU hint 16; 191 core parameters. These are original circuit switches and work in PURE; modern overlays remain masked there. |
| Wah / EXP | SVF band-pass stand-in, linear expression | `PraWahCircuit` with declared LDR optics; expression through the same cell | [REALTIME_PRA_WAH_MODEL.md](REALTIME_PRA_WAH_MODEL.md), ws2/ws4 reports |
| Noise and always-on circuits | Original independent draws unchanged | Shared SUB29 panel noise and separate output hiss remain the declared source laws. Exact BBD/PRA work omission preserves every phase, optical state and nonzero tail | f79b6f7; [exact idle evidence](evidence/qualification-2026-10-31/idle-exact/). Dormant-noise integration state: deferred; no conditional-Gaussian realization change has been adopted. |
| Output | scalar LOW trim + soft clip | -6 dB reference, driver ceiling, `OutputTapCircuit`; LISTENING OUTPUT L/R, GENERAL, PHONES | [OUTPUT_REALTIME_PROPOSAL.md](OUTPUT_REALTIME_PROPOSAL.md), ws2 report |
| Control smoothing | Historical per-sample constants retained | Existing 5 ms control and EXP optical recurrences omit work only at a proven exact fixed point; signed-zero/nonfinite/first-movement behavior preserved | 8dac795; [stationary-control evidence](evidence/qualification-2026-10-31/stationary-controls/). 373 exact checks, seven source mutations; three-round timing remains separate. |
| Oversampling | Original 2x audio retained | Eco 2x / Standard 2x / HQ 8x; offline HQ; 63 + 8/N samples latency. Exact complete-zero FIR history skips convolution only under its original arithmetic proof | `Oversampling.h`; FIR `23a7b8d`; both Legacy audio gates and V2 remain byte-identical. Timing qualification is separate. |
| Engine layout | Same audio law and RNG ordering | Global/rank/mix/output pipeline with sixteen rank chains stored in arrays; exact settled receiver and control-transform reuse | Source 1207068, c3c0f5c, 50930af; final timing [nine-checkpoint CPU evidence](evidence/qualification-2026-11-01/cpu-serial-fir-run/README.md). A layout change alone is not a speedup claim. |
| Host automation | Earlier block-start wrapper semantics retained | Source Performance uses 18 named native VST3/AU timed lanes on both engines; external state edits refresh delayed trajectories; AU normalized publication preserves accepted knots | [Host automation](HOST_AUTOMATION.md), 6474265, 6300163. PURE retains its earlier wrapper semantics. Native AU result: actual AU component host plus seven production mutations passed on both Intel and Apple Silicon; [native evidence](evidence/qualification-2026-11-01/host-features/README.md). |
| Tuning extension | Neutral at default mode | Unmodified ODDSound MTS-ESP client plus native/shim/fallback checks; Scala/KBM and MTS SysEx retained | [Vendoring record](../deps/mts-esp/README.vendor.md), fa2016c2, upstream 0BSD. PURE masks modern tuning; a live installed master is not claimed by no-master tests. |
| State, undo and MSEG | Historical parameter IDs retained | Common APVTS tree lock and synchronized detached snapshots; message-thread undo ownership; exact Float32 MSEG replay/restore | [State synchronization](STATE_TREE_SYNCHRONIZATION.md), bbc57fe. The audio mailbox still uses its existing try-lock. Arbitrary concurrent processor destruction is outside these fixtures. |
| Monitor and pressure | Read-only telemetry | Coherent engine taps for all active physical slots; both engines own output metering and tagged on-screen pressure in sixteen-voice mode | 3c08e8e; [UI completion](workstreams/ui-completion.md). Assignment serials reject stale gestures; PURE remains eight voices. |

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
| 0.1 Kernels and component arrays | Event/Jacobian caches, global/rank/mix/output split and sixteen-chain component arrays implemented; receiver/transform/lazy-history/control reductions qualified. Full lane SIMD is not implemented; whole-engine CPU remains open | `6bf6055`, `031a333`, `1207068`, `50930af`, `476c2f1`, `8dac795` |
| 0.1 Silent rank cadence | 3 kHz to 1.5 kHz qualified for the free-running circuit ranks; strict V2 re-pin complete; measured CPU target still open | audio `18400c2`, pin `9f990ae` |
| 0.1 Exact zero-history FIR | Complete-history/rounding proof and ten source mutations pass; no V2 re-pin. Local and serial GitHub three-before/three-after timing complete: idle improves, held and moving-control results do not establish a speedup. Final twelve-checkpoint / 36-round serial CI confirms improved idle, not a held speedup; both original CPU budgets remain open | `23a7b8d`; contract `156d82e` |
| 0.2 Parameter smoothing | done (circuit engine), ws2 | `d29f6eb` |
| 0.3 State schema version | done (schema 3 since ws2) | `eb486cb`, `83f8a0e` |
| 0.5 This status page | done | `eb486cb` |
| 1.1 IG00156 filter, first stage | done | `f6b126d` |
| 1.1b IG00156 nodal stage | original full-band limits pass; permanent 176.4 kHz coverage closes the remaining 44.1 kHz/4x grid gap without a DSP change. HP voltage discrepancy remains evidence, not fitted | Audio `4917708`, `651c74d`; supplemental contract `2f18afc`, gate `99d5198` |
| 1.2 Oscillator promotion | done; CPU over budget | `a6a574f` |
| 1.3 RC envelopes | done; first-assignment and Sustain II filter charge retained | `131f72b`, `c54c476` |
| 1.4 IG00151 VCA and event continuity | Filter charge/time, ring/precise-charge retention and disconnected idle coupling repaired. The new 361-check handoff grid and 59-check primitive pass; original click-score criteria remain separate | `c54c476`, `46a7a5a`, `38e99f4`, `481c7e9`, `b75b4d6`; [handoff evidence](evidence/click-20261101.md) |
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
| 4.1 Tone tabs | Native inspection and page-cited transcription of all 25 requested cells complete; ambiguous/revised branches remain hypotheses; original V1 values unchanged | `4a86e51`, `5a3027d`, `0ea5807` |
| 4.2 Memory panels | electrical model drives selected memory/PANEL banks in the circuit engine; interface/taper are declared inferences | `d66d298` |
| 4.3 Curated library | done: 301 circuit-engine programs + 222-program Legacy Library; recalibrated for the final audio | `9beed5e`, `82b9e14` |
| 4.4 Program Change, ribbon CC | done | `ec4d853`, `7c65aaf` |
| 5.1-5.2 Conformance audit, re-layout | done | ws7 merge `bd524f9` |
| 5.3-5.4 Toolbar, Studio view, interaction | done; modern and tuning pages mounted | ws8 merge `5af3b77`, `230f367` |
| 5.5-5.7 UI and ownership | Switch/combo/memory-miniature contexts, MIDI learn, undo gestures, engine taps and sixteen-voice meter/pressure ownership complete; native feature review verified | `2c0866e`, `0d9f522`, `3c08e8e`, `0ea5807` |
| 6 Modern layer | done (MSEG, LFO waves, matrix, FX, tuning, MPE, voice modes, MIDI learn, undo, A/B, randomise, host groups) | ws5 `5263172`, ws3, ws6, ws8 |
| 6 Separate chorus/tremolo tab positions | done; historic default preserves original exclusive switch, circuit source can select both off/on | `6f1c4aa`, pin `adc8bbb` |
| 6 Sample-accurate automation | Eighteen named lanes; VST3 native host qualified, AU component host and seven source mutations pass on Intel and Apple Silicon; external-state and endpoint ownership repaired | `3a37594`, `780658d`, `6474265`, `6300163`, evidence `662a1a2` |
| 7 Validator and release | Proven Linux validator-host callback lifetime mechanism removed; native Intel BBD compiler mechanism repaired. The complete three-rate capture and feature-build CI passed their infrastructure gates, but numerical CPU/click acceptance failed. Main merge, exact-main CI and release remain pending; owner exclusions remain open | `f39af47`, `a88ef86`, `1f842b9`, `e1c5ee9` |
| 6 MTS-ESP client | Actual pinned ODDSound client plus 0BSD licence vendored; real-client, shim and fallback compile/test on all four native platforms | `fa2016c2`, evidence `662a1a2` |
| 6 State snapshots and undo | Common APVTS lock and sync-before-copy/replace; exact Float32 MSEG replay/restore and balanced undo host gestures | `bbc57fe`; [state evidence](STATE_TREE_SYNCHRONIZATION.md) |

## Scorecard and measured boundaries

Every target below remains unchanged. A model/source qualification, event-state
continuity result and full-bank score are different observations.

| Item | Target | Qualified result / scope |
| --- | --- | --- |
| Full-band nodal small-signal error | Existing full-grid limits: 0.08 at 88.2/96 kHz internal; 0.02 at 192/384 kHz; core band 0.03 | Phase correction `4917708`: 0.04884 / 0.04392 / 0.001954 / 0.00005825, respectively; original limits pass. See [derivation and gates](IG00156_PHASE_CORRECTION.md). |
| Nodal 44.1 kHz host / 4x | Preregistered supplemental full-band 0.02, core 0.03, matrix equivalence 2e-6 | At 176.4 kHz: 0.002967 full band, 0.002642 core, 1.5297e-7 matrix difference. Expanded permanent gate: 853 checks; a 176.4 kHz-only defect fails three assertions. [Fresh review](evidence/nodal-review-20261101/README.md). |
| ADJ printed-p11 HP setup | 4.3 +/- 1 Vpp | Model prediction 1.692 Vpp, outside 3.3–5.3 Vpp. No component/gain fitting. |
| ADJ printed-p11 LP setup | 2.2 +/- 1 Vpp | Model prediction 1.648 Vpp. Native M41 trace confirms 150 kΩ in series CP7→AI and a separate 33 kΩ shunt. |
| Exact event continuation | Zero bit mismatches at zero elapsed time; original cold-time bound 1e-8 s | 7,438 full checks pass in both policies; all 15 source mutations and both test-strength controls qualified. [Event evidence](CLICK_EVENT_CONTINUITY.md). |
| Immutable compatibility | Byte identity | Legacy default/tails, V1 numerics and current V2 all pass after the native BBD repair and the exact FIR candidate. |
| Eight held voices | 0.15–0.20 callback realtime factor, 48 kHz / 256 frames | Final 12-checkpoint same-host EPYC 7763 run: `83b7f767` **0.605709–0.888572** held median RTF (p99 up to 1.050427, 49 selected deadline misses); original `db24a471` **0.588123–0.696543**. **Target open.** [Frozen 36-round evidence](evidence/qualification-2026-11-01/cpu-serial-fir-run/FINAL_12_CHECKPOINTS.md). |
| Quiet idle | <=0.02 callback realtime factor | Final EPYC 7763 run: `83b7f767` **0.067570125 (6.757%)** versus original `db24a471` **0.143822625 (14.382%)**. Idle reduces 53.02% relatively; **2% target open**. [Frozen same-host evidence](evidence/qualification-2026-11-01/cpu-serial-fir-run/FINAL_12_CHECKPOINTS.md). |
| Full-bank source phrase peak, steal and 48 kHz ring-off spectrum | ≤0 dBFS; click score ≤1; alias ≤−70 dB | Final-audio three-rate audit: phrase peak **−0.370566 dBFS** passes; worst voice-steal score **1.03039** fails (1 of 283 source phrase rows); 48 kHz ring-off saw **−71.14 dB**, pulse **−76.36 dB**, both pass. Other source click gates fail as detailed immediately below. Historical `00ceccf` measurements are not substituted. |

### Complete three-rate final-source engine audit (2026-11-01)

[GitHub Actions run 41336194253](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41336194253)
completed successfully, retaining the original
[artifact 13097476296](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41336194253/artifacts/13097476296)
(ZIP SHA-256 `a76dd6ee33dcfa4f14be4223ecb06331330d19d90a0baf84147745cef2c88f19`;
retention currently through 2027-01-30). A [permanent evidence branch](https://github.com/cheshmakzanoon-ops/yamaha-cs80/tree/evidence/full-engine-audit-20261102/docs/evidence/qualification-2026-11-01/full-engine-audit-41336194253)
at `0151b869` preserves the unmodified CSVs and the losslessly reconstructable original ZIP;
all 18 published file hashes match the verified local copies. The collector used the frozen
`.github/full-engine-audit-request.json` and unchanged numerical reporter at
`39ae6c2684456950302cb4ed10ecfae980c37ddd`, with production audio
`83b7f7670f000a95c6b44f96ffb0df3894755d14`, source tree
`7dd159d491012fd47b46c0aa651e86ea959ca289` and prior registration
`00da4498792523cf8d97fd40c17e12f215e190b8`. Ten coverage/source mutations
were rejected. Independent retrieval checked the ZIP CRC, provenance/digests,
all five CSVs and **885/885 rows**: 40 latency, 168 alias, 102 FX, 566 phrase
(283 Source Performance + 283 Legacy), and 9 click-causality. No numeric CSV
cells were nonfinite. A green capture is not a pass of its audio acceptance criteria.

| Final-source audit result | Observation | Unchanged gate / conclusion |
| --- | --- | --- |
| Source phrase peak, all rates | Worst **−0.370566 dBFS** (Guitar 1+2, 96 kHz) | ≤0 dBFS **PASS** |
| Source phrase retrigger click | Worst **1.23689**, **1/283** rows above 1 | ≤1 **FAIL** |
| Source phrase voice-steal click | Worst **1.03039**, **1/283** rows above 1 | ≤1 **FAIL** |
| Source phrase single-note-off click | Worst **1.62689**, **22/283** rows above 1 | ≤1 **FAIL** |
| Source phrase chord-note-off click | Worst **1.84628**, **37/283** rows above 1 | ≤1 **FAIL** |
| Original physical-PANEL Bass causality | **2/3** rates above 1; worst **1.440924** | ≤1 at all rates **FAIL** |
| Funky 1 + Funky 2 tone-tab causality | **3/3** rates above 1; worst **1.626889** | ≤1 at all rates **FAIL** |
| Electrical-programmer Bass causality | **0/3** rates above 1 | ≤1 at all rates **PASS** |
| Source ring-off saw/pulse alias, 48 kHz | Worst **−71.14 / −76.36 dB**, respectively | ≤−70 dB **PASS** |
| Source FX stress peak, 44.1/48/96 kHz | Worst **−7.82851 / −6.116 / −5.97371 dBFS** | Within digital full scale; **PASS** |
| Legacy FX stress peak, 44.1/48/96 kHz | **+0.774436 / +0.725615 / +0.822149 dBFS** | Recorded diagnostic; not misrepresented as Source phrase criterion |
| Source decimator latency | **62.9927–63.0082** host samples measured; engine reports **67** (63 + 4 internal oscillator contribution) | Measured/reported scopes separated |

The 48 kHz ring-*on* non-harmonic value near −43 dB is recorded as a
feedthrough diagnostic (the declared `0.005*x` signal term), not substituted
for the ring-*off* alias gate. The successful audit's source/audio SHA and zero
nonfinite count establish capture integrity only, not physical-instrument parity,
CPU acceptance, or closure of the click failures.

### Final twelve-checkpoint CPU benchmark (latest complete production audio)

[GitHub Actions run 41292549292](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41292549292)
collected **36 serial rounds over twelve immutable source checkpoints**, on
one AMD EPYC 7763 runner with four virtual CPUs. All builds preceded timing,
all 27 source mutations were rejected, and independent verification matched
346 archived hashes, 884 source hashes, 1,836 finite rows and twelve summaries.
The [final same-host source scorecard](evidence/qualification-2026-11-01/cpu-serial-fir-run/FINAL_12_CHECKPOINTS.md)
includes ring and connected VCA tail changes.

| Same host, 48 kHz / 256 frames | Original `db24a471` | Final `83b7f767` | Unchanged target |
| --- | ---: | ---: | ---: |
| Eleven held-tone median RTF range | 0.588123–0.696543 | **0.605709–0.888572** | 0.15–0.20; **open** |
| Quiet-idle median RTF | 0.143822625 | **0.067570125** | ≤0.02; **open** |
| Selected held deadline misses | 1 | **49** | Diagnostic |
| Moving-controls median / p99 RTF | 0.669017 / 0.850359 | **0.849516 / 1.052272** | Diagnostic |

Idle is 53.02% lower main-to-final on this same host. The adjacent
pre-/post-FIR pair improves idle 11.8995%; neither comparison establishes a
held speedup, a 2% idle pass or analogue/DAW validation.

#### Earlier nine-checkpoint CPU qualification (different host; history)

[Run 41262684915](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41262684915)
completed **nine immutable checkpoints, three rounds each**, on one AMD EPYC
9V45 runner with four virtual CPUs. All nine builds finished before measurement;
the 27 measurement intervals ran serially in the preregistered round-major
order. The stock 48 kHz, 256-frame, 1,024-callback driver and all 27 cases/51 rows
per round are retained. The frozen reporter selects each fixture's minimum
callback median and takes p99 and deadline misses from that same round.
The circuit rows use the benchmark's isolation/PURE policy; they are not a
general modern-FX Source Performance or real-DAW load estimate.

| Source on the same EPYC 9V45 runner | Held median RTF range | Held p99 RTF range | Selected held misses | Quiet idle RTF | Moving-controls median RTF |
| --- | ---: | ---: | ---: | ---: | ---: |
| Original main `db24a471` | 0.353231–0.416845 | 0.369217–0.466853 | 0 | 0.083520375 | 0.416251688 |
| Pre-FIR `475063c` | 0.372466–0.545617 | 0.390135–0.653069 | 0 | 0.042132938 | 0.530522813 |
| FIR checkpoint `23a7b8d` | 0.388281–0.557422 | 0.397795–0.715900 | 8 | 0.036052500 | 0.537153750 |

The [independent CPU review](evidence/cpu-20261101.md) freshly verified all
262 artifact hashes, 662 source records and 1,377 result rows. A further serial
request now appends immutable ring (`481c7e9`), VCA (`b75b4d6`) and final
current-sample tail-pan (`83b7f76`) checkpoints to the existing nine. All twelve
source/benchmark identities resolve exactly; the unchanged collector accepts
36 serial rounds and all 27 request/source mutations are rejected. The 48 kHz,
256-frame, 1,024-callback fixtures and frozen reporter remain unchanged.
[Request validation](evidence/integration-20261101/cpu-final-request-validation.json)
and [source map](evidence/integration-20261101/cpu-final-source-sha-map.json)
are preserved before CI collection. Its completed timing, hashes and selected round results are now independently verified and included above.

For the isolated FIR pair, the selected idle median falls **14.4316%**, and
every after-round idle median is below every before-round median. Held and
moving-control observations worsen: the latter's p99 rises from 0.703994 to
0.935590 RTF and selected deadline misses from one to nine. Those results are
retained; no held-speed improvement or attribution solely to scheduler noise
is claimed. All 306 before/after non-timing rows match, and all 1,377 rows in
the nine-source run have zero nonfinite counts. Both CPU targets remain open.

The [complete nine-checkpoint evidence](evidence/qualification-2026-11-01/cpu-serial-fir-run/README.md)
preserves the original ZIP, 27 raw rounds, compiler commands, independent
262-entry artifact and 662-source-record verification, frozen reporter and
source distinction. The original-main-to-final comparison includes the
full-band filter and electrical programmer, so it does not isolate a cache
speedup. The adjacent FIR pair has identical normalized production compile
and link commands and benchmark source.

#### Earlier serial run (different host; retained as history)

[Run 41257213038](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41257213038)
completed seven immutable checkpoints, three rounds each, on an AMD EPYC 7763
runner with four virtual CPUs. **Its absolute timings cannot be compared with
the EPYC 9V45 run above to claim a code speedup.** The 48 kHz,
256-frame, 1,024-callback method, all 27 cases/51 rows, and same-round p99 and
miss counts are retained. The original-main aggregate frame and subsequent
electrical-programmer constructor are separately sealed source blobs; their
one-line distinction is recorded in the [complete durable evidence](evidence/qualification-2026-11-01/cpu-serial-run/README.md).
RTF measures wall-clock callback duration divided by the audio budget; it is
not operating-system thread utilization or DAW certification.

| Source | Held median RTF range | Held p99 RTF range | Selected held misses | Quiet idle RTF | Moving-controls median RTF |
| --- | ---: | ---: | ---: | ---: | ---: |
| Original main `db24a471` | 0.589659–0.695422 | 0.601415–0.808634 | 0 | 0.144685 | 0.670410 |
| Inherited completion `2ee33dc` | 0.613537–0.893367 | 0.627465–1.048145 | 57 | 0.102848 | 0.854164 |
| Lazy histories `476c2f1` | 0.608570–0.893615 | 0.630247–1.054142 | 64 | 0.100299 | 0.851390 |
| Exact BBD/PRA `f79b6f7` | 0.608242–0.888042 | 0.623219–1.056947 | 55 | 0.089710 | 0.856346 |
| Stationary controls `8dac795` | 0.605332–0.889552 | 0.619235–1.058104 | 60 | 0.088383 | 0.849536 |
| Unused LFO `193f1a3` | 0.603647–0.889994 | 0.622794–1.054589 | 55 | 0.087235 | 0.852503 |
| Silent rank cadence `18400c2` | 0.603543–0.890282 | 0.618008–1.054760 | 56 | 0.077991 | 0.849753 |

All outputs in all 21 rounds were finite. This run supports the idle reductions;
it does not establish the held or idle targets. The added full-band filter and
programmer change the cost of the inherited implementation, so this is not a
single-change cache speedup. [The profile and feasibility note](evidence/continuation-2026-10-31/cpu-feasibility-followup.md)
distinguishes recorded instructions, static estimates and future lane batching.
The rejected column layout remains outside production after exact NaN/rounding
failures; no tolerance or oracle was changed to admit it.

The separate exact FIR source is `23a7b8d`. Three pristine local before rounds
at `475063c` and three after rounds on a clean checkout of the published FIR
source are retained in [the complete FIR qualification](evidence/qualification-2026-11-01/zero-fir/README.md)
(evidence `a0f37fa`). All 306 corresponding non-timing rows match exactly.

| Local FIR comparison, minimum of three callback medians | Before `475063c` | After `23a7b8d` |
| --- | ---: | ---: |
| Circuit held median RTF range | 0.632455–0.957676 | 0.626982–0.908651 |
| Circuit quiet-idle median RTF | 0.070246875 | 0.060575438 |
| Circuit moving-controls median RTF | 0.976010812 | 0.911899687 |
| Circuit moving-controls p99 RTF, same selected round | 1.805770312 | 2.153501813 |

The selected quiet-idle median falls by 13.77%, from 7.02% to 6.06% of realtime.
Substantial shared-host scheduling noise remains in all six raw rounds; the
lower medians and worse moving-control p99 do not establish a held speedup.
Neither requested CPU target passes. FIR work omission applies to both policies
when admitted; Legacy idle is therefore not an unchanged timing control, even
though both Legacy audio captures remain byte-identical.

[Serial run 41261598525](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41261598525)
failed before any benchmark ran. Its mutation witness removed one item from an
assumed seven-entry request; with two appended checkpoints it instead hit the
independent prefix-identity guard. `ef166ea` makes that witness retain exactly
six historical entries and adds its file to the workflow's trigger paths.
The original seven-entry request and the current nine-entry request each pass
all 27 mutations after this correction. The request, collector, reporter,
acceptance contract and nine checkpoint identities are unchanged. The completed
fresh run is recorded above; the witness correction itself makes no performance
claim. [Original failure and correction proof](evidence/qualification-2026-11-01/cpu-runner-prefix-fix/README.md).

### Original click criteria

The named-fixture values below were freshly captured with connected VCA
discharge `b75b4d6`. Funky is unchanged from the pre-VCA capture; physical-PANEL
Bass changes only slightly at 44.1 and 96 kHz. The completed full-bank audit corroborates the remaining musical-click failures.
These are named SOURCE PERFORMANCE fixtures; the separate full-bank worst cases
are listed above. Each original requirement remains **score <=1**.

| Rate | Original-main physical-PANEL Bass | VCA-corrected physical-PANEL Bass | Original-main Funky note-off | VCA-corrected Funky note-off |
| ---: | ---: | ---: | ---: | ---: |
| 44.1 kHz | 1.510516 | 0.567656 | 1.441274 | 1.626889 |
| 48 kHz | 1.308168 | 1.440924 | 1.647965 | 1.291310 |
| 96 kHz | 1.246674 | 1.365311 | 1.290939 | 1.220958 |

Funky's omitted-note-off controls also score above one (1.626888 / 1.380503 /
1.238904); all pre-event PCM comparisons are exactly equal. Bass's later
48/96 kHz peaks occur about 7.8 ms after assignment, while the new gate finds
no exposed-state reset at the event. These controls establish the investigated
continuity mechanisms; they do not delete, relax or automatically reclassify the
original score criterion. Electrical-programmer Bass and first-strike controls
remain separately labelled. [Full evidence and regression](CLICK_EVENT_CONTINUITY.md).

## Compatibility and qualification

| Capture | Immutable full baseline / current pin | Result |
| --- | --- | --- |
| Legacy default | `d56911a7c5f6953cd244457226b126cc567a538b` | Byte-identical |
| Legacy tails | `d56911a7c5f6953cd244457226b126cc567a538b` | Byte-identical |
| Circuit V1 numerics | `22d6a32c48c85f9b813506edcd856f73f4293c2d` | Byte-identical |
| Circuit V2 audio/state | `83b7f7670f000a95c6b44f96ffb0df3894755d14` | Byte-identical; strict gate passes |

The latest intentional audio sequence is current-sample connected-tail modulation:
audio **`83b7f7670f000a95c6b44f96ffb0df3894755d14`**, followed by the separate
full-SHA pin **`14ef653ed60be67ff98656966f9181c5e9b6b4fc`**. Contracts
`4e5dc03`, `9bb6ee0` and `f31bbb4` preceded the respective evaluations. A
charged inactive tail consumes the current modern matrix pan before mixing;
extra updates preserve the original scheduled note/RNG, mono-trigger,
global-gate and expression-clearing ownership. Route removal clears cached pan,
while steady no-pan, default, PURE and Legacy paths retain their cadence.
The shared poly-MSEG runner advances actual elapsed time for pan; other routes
sharing that runner see its current value. No cloned runner, fade or circuit
coefficient change was introduced.

The 618-check engine grid and six direct scheduling checks pass. The existing
59-check VCA primitive and 361-check handoff grid also pass. The fourteen-defect
mutation audit is still running at this receipt and is not yet claimed complete.
Three local before and after stock-baseline rounds were retained at 48 kHz / 256
frames / 128 callbacks; these are diagnostic timing under shared-host load,
separate from the requested 1,024-callback serial CI qualification.

Both the non-strict comparison against preceding V2 `b75b4d6` and
`scripts/check-compat-gates.sh --strict-v2` against current V2 `83b7f76`
pass all four comparisons. The integrator independently compared the complete
saved byte pairs and verified the tested `src` tree against the published audio
commit; [raw strict result and capture hashes](evidence/integration-20261101/tail-pan-strict.json)
retain that receipt. The standard V2 fixture does not activate the repaired
nondefault pan route, which is covered separately by the registered tests.

The preceding connected-VCA sequence remains audio **`b75b4d6`**, separate pin
**`fb43c04`**, then strict receipt/STATUS **`ac93191`**. Its contracts `61adb02`
and `8c5cf64` preceded evaluation, and all eight actual defects/contract
mutations were rejected. The original 0.1 s capacitor pole and 1e-30 numerical
floor remain unchanged; explicit cold reset and MIDI All Sound Off clear stored
charge. Its [independent byte receipt](evidence/integration-20261101/vca-first-strict.json)
is retained. Ring/precise-charge retention precedes it as audio `481c7e9`,
separate pin `8a85faa`.

The earlier intentional audio sequence for native Intel BBD history ownership was
audio `1f842b9`, separate full-SHA pin `475063c`, then strict gate/status evidence
`e1c5ee9`. Apple Clang 17 had rewritten the magnitude-bit predicate into a floating
comparison; DAZ then discarded stored subnormal ownership. The native original,
repair and reintroduced floating-comparison mutation give 1 / 0 / 1 failures
out of 1,245 checks. The 16-mode probe gives 4 / 0 original/repaired mismatches.
[Mechanism and raw native evidence](evidence/qualification-2026-10-31/bbd-native-predicate/README.md).
The exact FIR optimization kept that earlier V2 pin after all four comparisons
passed. Earlier audio/pin/evidence sequences remain in the dated history.

| Qualification | Confirmed scope |
| --- | --- |
| Core Release | Prior integration `1e58938` (production `23a7b8d`) passed 102/102 CTests. Current integration is in progress. A contended pre-VCA run passed 100/103, hitting the unchanged note-off, source-factory integration and library-audit timeouts; its red log is retained for the final complete comparison. |
| Linux plugin preset | Prior production `23a7b8d` passed the complete plugin build and 110/110 CTests, including real-window UI smoke. Current full build/tests are in progress. |
| Native host features | Fresh retrieval of build 41269271356 at `8a85faa` verifies 45 feature results across all four platforms, including actual AU hosts/mutations and Linux VST3/UI tests. [Independent review](evidence/host-ui-20261101/README.md) preserves the exact source scope before the VCA change. |
| MTS-ESP | Real client, shim and fallback pass on all four platforms. Actual upstream licence is **0BSD**, at ODDSound `f214739b8832e7f297cb9970d0c0efbf783f1462`; a live external master is not claimed. |
| Filter comparator | Fresh and restored positives each pass 781,314,302 exact assertions; five source mutations fail normally. [Evidence](evidence/continuation-2026-10-31/column-layout-rejected/README.md). |
| Event regression | 7,438 full assertions; 15 source mutations; zero-limit weakening and missing-rank-II controls checked. [Evidence](CLICK_EVENT_CONTINUITY.md). |
| FIR | Oversampling 85, exact history 227,117, rounding 3,276,237 and core 11,512 assertions pass; all ten source mutations rejected. [Complete local qualification and six rounds](evidence/qualification-2026-11-01/zero-fir/README.md). Final serial timing is complete above; final native workflow qualification remains separate. |
| UI/tone review | Context IDs, MIDI learn, undo gestures, taps and two-engine ownership verified; 70 native crop payloads and all 25 cells rechecked. [Evidence](evidence/qualification-2026-11-01/ui-tone-review/README.md). |
| Final CI / landing | Complete 885-row three-rate full engine audit captured at `39ae6c` (run 41336194253), with numerical click failures retained. Feature build push/PR passed at `7c864f` (41338140448/41338144467), and isolated WSC-oracle build/DSP/screenshots passed at `5809a528` (41339574562/41339574637/41339574530). These do not substitute for accepted CPU/click gates or three exact-main push workflows. Main merge, release, README installer refresh and final PDF remain pending. |

The Linux validator fix is a demonstrated host mechanism: its old copied file-
descriptor callbacks retained a raw RunLoop owner, and a prior callback could
destroy that owner's handler map. The fixed host uses weak/shared ownership and
checks descriptor membership. The old class reproduces the use-after-free under
the actual dispatcher; the fixed class passes; restoring the raw capture fails
again. Linux uses the SHA-checked pluginval 2.0.0-beta3 binary, strictness 10,
headless/GUI/fixed-seed coverage and one attempt. The plugin itself remains JUCE
8.0.11. [Lifetime proof](evidence/VALIDATOR_HOST_LIFETIME.md) retains original
failed CI logs; no claim assigns every historical untyped exit 139 to this cause.

Acceptance chronology remains explicit. Original AU publication limits precede
evaluation; the first supplementary concurrency/identical-state check was
retrospective because an intended extension was absent from `9dd7762`. The real
extension/correction is `bf817e5`, before repaired native evaluation. The original
Funky nonzero-charge precondition failed after the long hold; `d06648c` retained
that original fixture and preregistered an additional early-release witness.
No numerical threshold was relaxed, and original red logs remain in evidence.

## Open issues

1. **Held and idle CPU budgets.** The latest complete 12-source/36-round same-host EPYC 7763 run records final `83b7f767` held **0.605709–0.888572 RTF** (target 0.15–0.20) and quiet-idle **0.067570125 RTF** (target ≤0.02), versus original-main held 0.588123–0.696543 and idle 0.143822625. Final idle improves 53.02% relatively; held timing remains worse than original main. All raw rounds, selected p99 values and misses are retained, without claiming a held speedup. [Final CPU run](evidence/qualification-2026-11-01/cpu-serial-fir-run/FINAL_12_CHECKPOINTS.md) and [the remaining idle-control assessment](evidence/qualification-2026-11-01/cpu-idle-follow-up.md) document the gap. Multi-lane architectural reduction needs predeclared state, NaN and rounding oracles; exact electrical clocks cannot simply be paused.
2. **Original click scores.** Physical-PANEL Bass at 48/96 kHz and Funky note-off
   at all three rates remain above one. The charge/time defects are fixed and
   event regressions qualified, but the controls do not justify a fade, phase
   selection, pedestal retuning or coefficient fit to meet the metric.
3. **ADJ p11 HP amplitude.** Resolve switched source/mixer impedance and the full
   service alignment procedure before changing the 1.692 Vpp prediction. The LP
   150 kΩ series topology is resolved and is not a remaining source conflict.
4. **Tone-cell historical applicability.** All 25 requested cells were inspected
   and transcribed with pages. Ambiguous glyphs, crossed topology and revision
   applicability remain hypotheses; more source/board-revision evidence is
   needed before promotion. V1 values and fingerprint `e5f877a025fc52c6` stay fixed.
5. **Final integration and landing.** The frozen all-suite three-rate audit is complete (885 rows) but has six source musical-click failure scopes described above. Final CPU qualification also misses both budgets. Only after real fixes and new passing measured evidence may PR #6 become ready, the three exact-main push workflows be verified at merge M, and the guarded release be dispatched. The existing dispatch guard checks exact-main build/DSP/screenshot run success and screenshot-only ancestry, but does **not** itself verify CPU or click numerical acceptance; that additional qualification evidence must be independently checked before dispatch (preferably enforced by a preregistered machine-checkable gate). README downloads/screenshots, hardware coverage and final six-page PDF must then cite the actual published identities. No pending step is represented as completed.
6. **Owner-only work (outside this task).** Apple/Windows signing and notarization
   secrets; real-DAW sessions; trademark review of “CS-80” / “Yamaha”; AAX.

Other explicitly declared circuit assumptions and source uncertainties remain in
[HARDWARE_COVERAGE.md](HARDWARE_COVERAGE.md). Automated host tests do not replace
real-DAW qualification or a live installed MTS master, and no result here is a
measurement of an original instrument.

