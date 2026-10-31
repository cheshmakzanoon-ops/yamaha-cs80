# Current status

This is the one page that states where the project is **now**. Other documents
are specifications, dated evidence or history; when they disagree with this page
about the current state, this page wins and the other document is stale.

Update this page whenever a phase step in [IMPROVEMENT_PLAN.md](IMPROVEMENT_PLAN.md)
lands. Record exact commits; do not restate test counts without one.

Last updated: 2026-10-31, continuation branch `codex/cs80-completion` from
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
| 6 Sample-accurate automation | not done | — |
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

## Pinned compatibility gates

- `dsp-measurements.yml` **legacy-compatibility**: default-off audio byte-identical
  to `d56911a`. Legacy must never change.
- `dsp-measurements.yml` **source-factory-compatibility**: circuit V1 numerics
  (`22d6a32`) and circuit-engine audio V2, now
  **`4917708268d8eed177670f4d270dbc50fc08e831`** (full-band nodal phase
  correction). Pin commit `9246dbb2584eb2d87d744b7a52b43661bc7f15ce`.
  Earlier V2 pins, for historical circuit-engine renders: `031a333` (Jacobian
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

- Intermittent Linux pluginval exit 139 after SUCCESS: code review and a memcheck
  teardown probe found no fault in project code; root cause not established
  ([VALIDATOR_DIAGNOSTICS.md](VALIDATOR_DIAGNOSTICS.md)).
- CPU above target (above). Idle above target.
- Nodal filter top-octave small-signal accuracy at 2x/4x internal; ADJ p11 HP point.
- Note-off click scores above 1 on some percussive circuit-engine programs.
- MTS-ESP client library not vendored (shim only); 16-voice mode meter and
  pressure ownership cover the first engine only.
- Memory-panel electrical model not wired into audio; 25 tone-tab cells remain
  hypotheses; tremolo/chorus both-off/both-on states need an engine parameter.
- Context menu and MIDI learn for switches, combo boxes and lid miniatures;
  undo-applied values reach the host without gestures; the voice monitor
  illustrates settings rather than tapping the engine.
- Sample-accurate automation not done.
- Signing and notarization need the owner's certificates; real-DAW sessions,
  trademark review and AAX are owner actions
  ([release/RELEASE_QUALIFICATION.md](release/RELEASE_QUALIFICATION.md)).
