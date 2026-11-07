# Current status

This is the one page that states where the project is **now**. Other documents
are specifications, dated evidence or history; when they disagree with this page
about the current state, this page wins and the other document is stale.

Update this page whenever a phase step in [IMPROVEMENT_PLAN.md](IMPROVEMENT_PLAN.md)
lands. Record exact commits; do not restate test counts without one.

Last updated: 2026-11-07. Latest merged code and fully passing native build checkpoint: `59b1b3ce75148f5a9a0f5f67330b4fa1edefb534`; screenshot-only refresh: `a07cea6ff59feb12e3bae721436b5ef822517c52`. The matching build, DSP and screenshot runs passed, and all four native platform bundles are available.

## Changes since the October 31 integration

The original eight-workstream integration is recorded in
[workstreams/HANDOVER.md](workstreams/HANDOVER.md). The October 31 scorecard and
handover PDF remain dated evidence, not current-head measurements.

| Area | Current state | Source / scope |
| --- | --- | --- |
| Filter resonance cache | Reuses cutoff-dependent coefficients on Q-only updates; cutoff/rate changes retain the full path and the solver law is unchanged | `465e56c` ([PR #76](https://github.com/cheshmakzanoon-ops/yamaha-cs80/pull/76)); [cache scope and measurements](IG00156_RESONANCE_CACHE.md) |
| External modulation | Sparse FIR reconstruction skips guaranteed-zero products while retaining coefficient design, phase and accumulation order | `228cc8d` ([PR #74](https://github.com/cheshmakzanoon-ops/yamaha-cs80/pull/74)); [scope and measurements](EXTERNAL_INTERPOLATOR_SPARSE.md) |
| Delay-reader safety | A rounded circular-buffer position at capacity is wrapped to zero, preventing an out-of-bounds read | `ff9943e`; [DELAY_LINE_WRAP_SAFETY.md](DELAY_LINE_WRAP_SAFETY.md). Previously valid interpolation retains its arithmetic. |
| Host automation | Eighteen key VST3 parameters retain sample offsets in modern circuit policy; protected policies and unsupported parameters keep block-rate behavior | `1b3df5a`, endpoint correction `7c54caf`; [HOST_AUTOMATION.md](HOST_AUTOMATION.md). AU scheduled ramps are not enabled. |
| Tuning | Pinned 0BSD MTS-ESP client included, with bounded whole-table updates and missing-master fallback | `73e76ce`; [MTS_ESP_INTEGRATION.md](MTS_ESP_INTEGRATION.md). External master, per-channel tables and live DAW qualification remain separate. |
| MIDI and note lifecycle | Channel-scoped pressure/MPE routing, retained release ownership, ordered KAS short notes, partial CC121 reset and live conventional bend-range edits | [VOICE_RELEASE_OWNERSHIP.md](VOICE_RELEASE_OWNERSHIP.md), [MPE_MASTER_BEND_ROUTING.md](MPE_MASTER_BEND_ROUTING.md), [CONTROLLER_RESET.md](CONTROLLER_RESET.md), [PITCH_BEND_RANGE_AUTOMATION.md](PITCH_BEND_RANGE_AUTOMATION.md) |
| MSEG and FX | Trigger/sustain boundary fixes, mono onset retained while its routes are unused, restarted poly envelopes' rate history cleared, synced rate phase retained across host blocks, and corrections to tape, plate, Drive and loudness-match transitions | MSEG integration `59b1b3c` ([PR #80](https://github.com/cheshmakzanoon-ops/yamaha-cs80/pull/80)); transport fix `ce423e3`; [MSEG_COMBINED_INTEGRATION.md](MSEG_COMBINED_INTEGRATION.md), [MSEG_TRANSPORT_RATE.md](MSEG_TRANSPORT_RATE.md), [ws5 report](workstreams/ws5-modern.md) |
| Presets and state | A/B and Copy undo history, background-save undo preservation, saved names, metadata limits, malformed-state rejection and precision fixes | `e09ddb2`, `f757909`, `f7d1b75`; [ws6 report](workstreams/ws6-library.md), [BOOLEAN_STATE_RESTORATION.md](BOOLEAN_STATE_RESTORATION.md) |
| Voice onset | Unison circuits share the event's glide origin | `f921f54`; changes the affected modern voice-mode onset, not the hardware model |
| OE1 speed endpoint | Reviewed design evidence, source-conflict notes and a standalone diagnostic probe are preserved; shipping audio is unchanged | `fe16f0a` ([PR #9](https://github.com/cheshmakzanoon-ops/yamaha-cs80/pull/9)); [endpoint design](OE1_SPEED_ENDPOINT_DESIGN.md), [source conflict](OE1_SPEED_SOURCE_CONFLICT.md) |
| Editor evidence | Selected LFO waveform previews and 68 real-editor screenshots of the merged source | LFO preview fix `ae3958d`; screenshot source `59b1b3c`, run [42334458889](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/42334458889), committed `a07cea6`; [provenance](screenshots/provenance.json) |

The optimization and MSEG reports retain draft-stage qualification notes as
historical evidence. Those changes are now merged; the post-merge status below
is current. The mono/poly MSEG corrections intentionally change affected routed
performances in Legacy and Circuit, while default/no-route playback and PURE
CS-80 remain unchanged.

### Post-merge CI and download checkpoints

Merged code `59b1b3c` passed
[build 42334458843](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/42334458843),
[DSP run 42334458795](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/42334458795)
and [screenshot run 42334458889](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/42334458889).
The README now links four native bundles from that exact source: Linux, Windows,
Intel macOS and Apple Silicon. They include the delay-reader safety fix, resonance
cache, sparse external interpolation and combined mono/poly MSEG corrections.
The artifacts were downloaded and checked: all archive checksums, 54 manifest-listed
regular-file hashes and sizes, source stamps and native architecture headers matched.
They are available and unexpired until February 5, 2027, and require a signed-in
GitHub account. Package inspection does not verify code signatures or notarization.
Automated native CI coverage is not DAW or release qualification.

The documentation screenshots also come from `59b1b3c` and were published by the
screenshot-only commit `a07cea6`. All 68 hashes match their manifest. Their source
matches the downloadable native bundles, rather than later documentation-only
commits.

The earlier [build 42274477424](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/42274477424)
at `228cc8d` remains a historical **failure**: five jobs succeeded and the
[Apple Silicon job 128087142687](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/42274477424/job/128087142687)
was canceled, leaving no ARM bundle or completed ARM qualification for that run.
The newer `59b1b3c` run supplies successful four-platform CI for the newer revision;
it does not change the earlier outcome. Its bundles supersede the older `fe16f0a`
download links.

The still-earlier `f921f54` [build run 42175969425](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/42175969425)
failed its Linux fixed-seed GUI validation step. Later passing runs do not
establish that failure's cause or resolve the intermittent validator issue.
[README downloads](../README.md#downloads) also retain the October 31 installers
as explicitly historical artifacts. No tagged GitHub Release has been published.

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
- **ENGINE: LEGACY (compat)** retains the older synthesis voicing and is used for
  sessions saved before schema 3 and the Legacy Library. Pinned render gates
  preserve their tested cases; MIDI/modern-layer bug fixes can change affected
  performances, including the optional KAS scanner ordering described below.
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
| Idle voices | frozen, reset on reuse | all 16 chains free-run (closed-form idle oscillator, decimated stages, snapshot replay); cold circuits start warm | [workstreams/ws3-keyboard.md](workstreams/ws3-keyboard.md), truth `reference/FreeRunningVoiceTruth.h` |
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
| 0.1 Resonance-only cache and sparse external interpolation | merged numerical optimizations; focused comparisons preserve tested output, whole-instrument CPU target remains open | `465e56c`, `228cc8d`; reports above |
| 0.1 Split, fast math, structure of arrays | partial: bounded-error tan in the nodal stage (`00ceccf`); fast exp2/pow measured no faster than glibc's and dropped; `processInternalSample` split and SoA not done | `00ceccf` |
| 0.2 Parameter smoothing | done (circuit engine), ws2 | `d29f6eb` |
| 0.3 State schema version | done (schema 3 since ws2) | `eb486cb`, `83f8a0e` |
| 0.5 This status page | done | `eb486cb` |
| 1.1 IG00156 filter, first stage | done | `f6b126d` |
| 1.1b IG00156 nodal stage | done with two reported misses (top-octave small signal at 2x/4x; ADJ p11 HP point) | `c9f8cf0`, `cbd2442` |
| 1.2 Oscillator promotion | done; CPU over budget | `a6a574f` |
| 1.3 RC envelopes | done | `131f72b` |
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
| 4.2 Memory panels | modelled with a truth model; not driving audio | `617259d` |
| 4.3 Curated library | done: 301 circuit-engine programs + 222-program Legacy Library; recalibrated for the final audio | `9beed5e`, `82b9e14` |
| 4.4 Program Change, ribbon CC | done | `ec4d853`, `7c65aaf` |
| 5.1-5.2 Conformance audit, re-layout | done | ws7 merge `bd524f9` |
| 5.3-5.4 Toolbar, Studio view, interaction | done; modern and tuning pages mounted | ws8 merge `5af3b77`, `230f367` |
| 5.5-5.7 Readability, code health, UI tests | done; goldens re-recorded on the integrated tree | `bd524f9`, `41363ea` |
| 6 Modern layer | done (MSEG, LFO waves, matrix, FX, tuning, MPE, voice modes, MIDI learn, undo, A/B, randomise, host groups) | ws5 `5263172`, ws3, ws6, ws8 |
| 6 Sample-accurate automation | partial: eighteen key VST3 parameters in modern circuit policy; AU and other parameters remain block-rate | `1b3df5a`, `7c54caf`; [scope](HOST_AUTOMATION.md) |
| 7 Release qualification | release workflow, installers, licence AGPLv3 done; signing needs secrets; exit 139 open | `d9b98c6`, `06eb8f3`, `80cbe8c` |

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

## Pinned compatibility gates

- `dsp-measurements.yml` **legacy-compatibility**: default-off audio byte-identical
  to `d56911a`. These pinned audio cases remain a compatibility requirement.
- `dsp-measurements.yml` **source-factory-compatibility**: circuit V1 numerics
  (`22d6a32`) and circuit-engine audio V2, now **`00ceccf`** (the eight-workstream
  integration). Earlier V2 pins, for historical circuit-engine renders:
  `f6b126d` (Phase 1.1 filter), `131f72b` (Phase 1.3 envelopes), `228459c`
  (Phase 1.5 gain staging), `6e5892f` before that.
- `scripts/check-compat-gates.sh --strict-v2` passes at the re-pin commit `ef9bc35`.

These gates cover their defined render cases, not every MIDI performance.
The optional KAS scanner now preserves same-pitch event arrival order, including
across channels. A strike before a release within one scan can change voice
allocation and playback in saved sessions that enable it. See the explicit
compatibility correction in [ws3-keyboard.md](workstreams/ws3-keyboard.md).
Other optional MIDI and modern-layer fixes have the scoped contracts linked above.

### Historical CI on the October 31 integrated tree

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
passed every job. The README retains the installer artifacts as explicitly
historical downloads and links newer native bundles separately.

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

- Intermittent Linux pluginval exit 139 after SUCCESS: code review and a memcheck
  teardown probe found no fault in project code; root cause not established
  ([VALIDATOR_DIAGNOSTICS.md](VALIDATOR_DIAGNOSTICS.md)).
- CPU and idle targets remain open. The resonance cache and sparse external
  interpolation optimizations are merged, but their focused measurements do not
  replace a new full scorecard for the merged source.
- Native four-platform CI passes at `59b1b3c`, including Apple Silicon.
  Actual DAW sessions and release qualification remain separate; the older
  canceled ARM job is retained in the historical CI record above.
- Nodal filter top-octave small-signal accuracy at 2x/4x internal; ADJ p11 HP point.
- Note-off click scores above 1 on some percussive circuit-engine programs.
- MTS-ESP now includes the [pinned 0BSD client](../src/vendor/mts-esp/README.md)
  for channel-independent live-master queries. External-master DAW qualification
  and per-channel master tables remain open.
- 16-voice mode meter and pressure ownership cover the first engine only.
- Memory-panel electrical model not wired into audio; 25 tone-tab cells remain
  hypotheses; tremolo/chorus both-off/both-on states need an engine parameter.
- Context menu and MIDI learn for switches, combo boxes and lid miniatures;
  undo-applied values reach the host without gestures; the voice monitor
  illustrates settings rather than tapping the engine.
- Sample-offset automation is limited to eighteen key VST3 parameters in modern
  circuit policy. AU scheduled ramps, unsupported parameters and DAW-level
  qualification remain open ([HOST_AUTOMATION.md](HOST_AUTOMATION.md)).
- CC121 support resets MIDI sustain and RPN/NRPN selection only; full Reset All
  Controllers behavior is not implemented ([CONTROLLER_RESET.md](CONTROLLER_RESET.md)).
- Signing and notarization need the owner's certificates; real-DAW sessions,
  trademark review and AAX are owner actions
  ([release/RELEASE_QUALIFICATION.md](release/RELEASE_QUALIFICATION.md)).
