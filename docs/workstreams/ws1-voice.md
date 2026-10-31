# ws1-voice: the voice circuit

Plan steps 1.1b, 1.2, 1.4 and 2.5 of [IMPROVEMENT_PLAN.md](../IMPROVEMENT_PLAN.md),
on top of integration commit `6719ce4`. All changes apply to the circuit engine
(Source Performance and Isolation) only. Legacy audio is byte-identical
(`scripts/check-compat-gates.sh`). Stopped at the owner's wind-down request;
see **Handover** at the end.

## Summary

| Step | State | Model doc |
| --- | --- | --- |
| 1.1b IG00156 nodal model | done, with two reported gate misses (top-octave small-signal accuracy at 2x/4x; ADJ p11 HP point) | [IG00156_NODAL_MODEL.md](../IG00156_NODAL_MODEL.md) |
| 1.2 Oscillator promotion | done for alias, anchors, shared table; **CPU over budget** (stated) | [OSCILLATOR_PROMOTION.md](../OSCILLATOR_PROMOTION.md) |
| 1.4 VCA (IG00151) | partial: model, level-law gate and pan done; retrigger click score 1.26 on one tab row at 44.1 kHz (root cause below); model doc not written | this page |
| 2.5 Tolerances and drift | not started (wind-down) | — |

## 1.1b IG00156 nodal model — done (with reported misses)

Implemented in `src/core/Ig00156Filter.h` (`Ig00156Stage`, HP and LP networks),
truth model `src/reference/IG00156Network.h`, wired in the `Engine.cpp` rank chain.
Four states per stage: series 1 uF input coupling, 1.2 MOhm || 180 pF BP-to-AI
branch, two 1.5 nF integrators with 4.7 MOhm feedforward; ideal amplifiers;
unilateral Q current; in-loop OTA tanh on the gm part; scalar Newton on BP.

Evidence classes (details and alternatives in the model doc):
- source-derived: the M41 graph, component values, HP = 2 AI - LP, inverting
  integrators, 1 uF couplings, 33 kOhm AI shunts.
- declared inference: HP mixing-node source 75 kOhm; LP 150 kOhm series source
  (the ledger reads it as a shunt; a stiff source would leave the LP undamped);
  Q-block gq affine in VQ, calibrated to Q 5 / 0.5 at 0 / 10 V (identical to the
  existing 1/Q maps); cutoff calibration (the documented law is the network's
  natural frequency; closed-form compensation of the 180 pF depression, held
  above 20 kHz); OTA level 2.5; unity-passband stage normalization.
- rejected: exact cubic-factorization Newton (no stable root for low Q above
  5 kHz); per-capacitor prewarps (all worse); ADAA (filter aliasing already at
  the -88 to -93 dB floor, so no measurable gain).

Results (`cs80_ig00156_nodal_tests`, tolerances pre-registered in `b87d560`):
equivalence to the truth trapezoid 1.9e-7 (limit 2e-6); nonlinear agreement
0.13 % (limit 3 %); Q anchors within 0.6 %; cutoff anchors +0.25..+0.53 %
(uncompensated alternative -2.3 / -4.4 %); stable on +/-8 noise at 88.2-384 kHz.
**Not met:** the full-grid small-signal gate at 88.2/96/192 kHz for cutoffs of
4 and 12 kHz (top-octave bilinear warp; e.g. band error 11-14 % at 88.2 kHz); met
for cutoffs <= 1 kHz everywhere and the whole grid at 384 kHz. The ADJ p11 HP
point is a miss (1.69 Vpp vs 4.3 +/- 1); LP is within band (1.65 vs 2.2 +/- 1).
Mutation checks: 5 mutations, all detected (table in the model doc).

## 1.2 Oscillator promotion — done, CPU over budget

`src/experimental/ReconstructedOscillator.h` moved to `src/core/`; drives saw,
pulse and sine in the circuit engine through one fixed inferred adapter
(1/1.5 V); the PWL shaper replaces the 3.5 % third-harmonic sine. The 16 KB
residual basis is shared per (rate, RC) through a fixed reference-counted
static pool (no heap allocation): 5,984 bytes per instance. Per-tick savings:
bit-identical end-node reuse and residual-free event skipping, plus a bounded
skip of sub-1e-6 V frequency corners (<= -127 dB, verified).

| Metric (tool) | 6719ce4 | after 1.1b (c9f8cf0) | after 1.2 |
| --- | --- | --- | --- |
| Worst saw non-harmonic 48 kHz (audit `--suite alias`) | -51.07 dB | -51.02 dB | **-88.3 dB** |
| Worst pulse non-harmonic 48 kHz | -53.55 dB | -53.55 dB | **-88.8 dB** |
| Worst saw 44.1 / 96 kHz | -50.67 / -67.35 | -50.61 / -67.16 | -88.1 / -90.1 |
| Circuit-engine RTF, 8 voices, 48 kHz, 256 frames (benchmark, min of 3 interleaved rounds) | 0.296 | 0.358 | **0.518** |
| Legacy RTF, same | 0.235 | 0.233 | 0.239 |

Oversampling for ws2: 2x -87.4 / -88.8 dB, 4x and 8x -89.8 / -89.2 dB (saw /
pulse, 48 kHz; temporary cascaded-decimator change in a scratch copy, never
committed). **The lowest factor meeting -70 dB is 2x.** The filter's top-octave
accuracy (1.1b) is what improves at 4x/8x.

Pin anchors 3.4997-3.5000 Vpp (pin 7) and 3.0000 Vpp (sine) at 88.2/96/192 kHz.
Third harmonic at the output: legacy 3.53 %, circuit 0.32 %. Mutations: PolyBLEP
restored and private basis, both detected. The baseline CPU in the brief
(0.37 RTF) was measured under different load; all numbers above come from the
same tool, flags and interleaving.

## 1.4 VCA (IG00151) — partial

Implemented in `src/core/Vca.h` (`Ig00151Vca`, `constantPowerPan`), wired in the
rank chain (`circuitVca` branch). Tolerances pre-registered in `0143066`.

| Law | Class |
| --- | --- |
| Distinct EI1 (exponential) and LI2 (linear) inputs | source (service p52) |
| LC31 -> IC5 LI2 with 10 nF; five branches 180k/560k/3 x 270k | source (M41, R39) |
| LI node tau 0.542 ms (ideal-source Thevenin x 10 nF) | inferred |
| SUB VCA modulation on EI1 (IC5 "VCA for SUB-OSC"), +/-1 octave at full depth | inferred; alternative: linear (legacy law) |
| Gain = LI x 2^EI x envelope | inferred |
| Offset 1.5e-3 + CV feedthrough 1e-3 (engine units): pedestal -52 dB at full LI | inferred (typical OTA magnitudes) |
| Next-stage AC coupling 1 uF / 100 k (tau 0.1 s) | inferred by analogy with IC5's documented input |
| Constant-power pan 0.5 sqrt(1 -/+ pan), exactly 0.5 at centre | engineering (modern rank pan) |

Gate `cs80_ig00151_vca_tests`: linear and exponential laws, LI tau 0.5419 ms
(declared 0.5419), pedestal -52.04 dB (limit -50), coupling settles, pan power,
exponential SUB tremolo (circuit 2^(2d) ratio; legacy additive law kept): pass.

Click scores (`cs80_engine_audit --suite phrase --quick`, source policy):

| Build | worst steal | worst retrigger |
| --- | --- | --- |
| 6719ce4 | 1.328 (Funky 1+2, 44.1 k) | 1.054 (tab, 96 k); 1.086 (program, 44.1 k, full run) |
| after 1.2 | 0.992 | 1.212 (Funky 3+4, 44.1 k) |
| after 1.4 (before the AC coupling) | 0.930 | 1.258 (Funky 3+4, 44.1 k), 1.001 (Guitar 1+2, 48 k) |

Root cause of the retrigger outlier: the audit's retrigger note lands on an idle
voice while the previous same-pitch voice is still in a bright release tail
(Funky's IL is above its steady level). The new voice's own onset jump is 0.222
(score 0.87); the two same-pitch waveforms' edges coincide, giving 0.29. It is
not a discontinuity; voice allocation is ws3's. The step-1.4 click criterion
(<= 1) is therefore **not met** on that row.

Mutation checks for the VCA gate: not run yet.

## 2.5 Tolerances and drift — not started

Nothing implemented. The design notes are in the handover.

## Tests added

`cs80_ig00156_nodal_tests`, `cs80_oscillator_promotion_tests`,
`cs80_ig00151_vca_tests` (all read `tests/parity/tolerances-v2/ws1-voice.json`
through `tests/Ws1Tolerances.h`). Existing tests updated for intentional
circuit-engine changes, each with a comment naming the change:
`tests/PraWahIntegrationTests.cpp` and `tests/OutputTapIntegrationTests.cpp`
oracles (reconstructed sine, VCA; helper `tests/CircuitSineSource.h`),
`tests/ReconstructedOscillatorTests.cpp` (include path only).

## Intentional circuit-engine sound changes (V2 re-pin needed)

1.1b: frequency-dependent resonance, 1 uF coupling corners, HP roll-off near
20 kHz, OTA limit on the gm part only, calibrated cutoff law. 1.2: WSC rising
saw, comparator pulse following the coupled ramp, shaped sine, reset plateau,
coupling memory, 8 internal samples of oscillator delay. 1.4: SUB tremolo in
dB, LI smoothing 0.54 ms, -52 dB envelope-shaped pedestal (AC-coupled),
constant-power rank pan (off-centre only).

## Entries for HARDWARE_COVERAGE.md (2026-10-31)

- VCF (IG00156): circuit engine uses the nodal stage (input networks, 180 pF
  branch, 1 uF couplings, declared Q-block and cutoff calibration); see
  IG00156_NODAL_MODEL.md. Status stays A/inferred.
- VCO / WSC (IG00153/IG00158): circuit engine uses the reconstructed
  event-driven oscillator; PWL sine; see OSCILLATOR_PROMOTION.md.
- 41 VCA level / IG00151: linear LI and exponential EI separated (circuit
  engine), LI node RC, declared offset/feedthrough, SUB on EI.
- 11 Sub -> VCA: circuit engine exponential (EI) law, +/-1 octave at full depth.

## New parameters

None.

## Files touched outside the ownership list

- `src/tools/source_factory_benchmark.cpp`: optional `--rate`/`--block` filters
  (default sweep unchanged).
- `tests/PraWahIntegrationTests.cpp`, `tests/OutputTapIntegrationTests.cpp`:
  oracle updates for the circuit-engine oscillator and VCA.
- `src/core/Engine.h`: Rank members (`hpStage`, `lpStage`, `wsc`, `vca`,
  `vcaCold`) and appended `RankAudioState::hpCircuit/lpCircuit`.
- `CMakeLists.txt`: three tests below the ws1 marker.

## Handover

**Done, with commits** (branch `worktree-agent-a05190c06a30e00a1`, base `6719ce4`):
`b87d560` tolerance pre-registration (1.1b, 1.2); `c9f8cf0` IG00156 nodal stage;
`cbd2442` closed-form cutoff calibration and nodal doc; `a6a574f` oscillator
promotion; `0143066` VCA tolerance pre-registration; `5e857f5` IG00151 VCA;
then the VCA AC-coupling commit and this report.

**In progress / how far it got**
- Step 1.4: model, gate and oracles done and green. Missing: the model doc
  (`docs/IG00151_VCA_MODEL.md`, content is the 1.4 section above), mutation
  checks of `cs80_ig00151_vca_tests` (suggested: drop the LI RC, put SUB back on
  the linear input, remove the coupling), and a full (non-quick) audit run.
- The integration-branch merge (`bd524f9`) was attempted, conflicts (include
  lines in the two oracle tests) resolved, then aborted on request. Findings for
  whoever merges: (1) both oracle tests need `#include "core/RingModulatorCircuit.h"`
  and `#include "CircuitSineSource.h"`; (2) ws4's
  `cs80_ring_modulator_circuit_tests` "carrier feedthrough is independent of the
  signal" failed against the VCA pedestal before the AC coupling was added —
  the coupling (tau 0.1 s) was added for exactly that reason and was verified
  only on this branch, not re-run on the merged tree; re-run that test after
  merging.

**Current thinking**
- The retrigger click > 1 is an allocation/phase-coincidence effect (see 1.4),
  not a VCA defect. Options: ask ws3 whether a same-note retrigger should reuse
  the releasing voice (the CS-80 KAS behaviour decides this); or accept and
  document it. Do not tune the VCA to hide it.
- CPU: the oscillator costs about 0.16 RTF for 16 instances; the work is spread
  across `nodes()`, `nextBranchEvent()` and the decay polynomial in
  `src/core/ReconstructedOscillator.h`. The realistic route is the planned SoA
  pass (vectorize across the 16 oscillators), not more scalar tweaks.
- Suspicions: the ADJ p11 HP/LP ratio (documented 1.95x, model 1.03x) suggests
  the setup conditions in `SourceFactoryTones.h::Alignment` differ from the
  manual's; the LP 150 kOhm series reading should be checked on a native M41
  crop of the CP7 -> AI1 path.

**Exact next steps, in order**
1. Write `docs/IG00151_VCA_MODEL.md`; run three mutation checks on
   `tests/Ig00151VcaTests.cpp` (method: the scratch-copy script approach used for
   1.1b/1.2).
2. Run `cs80_engine_audit --suite all --rates 44100,48000,96000` on the final
   tree with a long timeout (it needs more than 30 min on a loaded host) and
   record tab peak, steal/retrigger and alias in this page.
3. Step 2.5 in `src/core/ToleranceModel.h` (new) and the trim/drift code in
   `Engine.cpp` (`Engine::prepare` trim generation, `driftPhase`/`voiceDrift`
   in `processInternalSample`): per-card component draws (5 % R, 10 % C,
   declared truncated-normal, sigma = tol/2), mapped to tune, cutoff (integrator
   C1/C2 -> `Ig00156Stage` rates), Q (C1/C2 mismatch), level (LC divider ->
   `Ig00151Vca` linear gain) and envelope time (timing R x 0.22 uF) trims; read
   `trimCutoff/trimLevel/trimEnv` in the circuit branch only; replace the
   0.031 Hz sine with a seeded 1/f random walk plus warm-up; keep the old
   +/-22 cents sine as a "worn unit" parameter appended below the ws1 marker in
   `src/core/Params.h` (AU hint 8, default off). Pre-register its tolerances in
   `ws1-voice.json` first. Legacy and isolation (zero dispersion) unchanged.
4. Ask the integrator to re-pin `SOURCE_AUDIO_V2`.

**Failing or flaky tests:** none on this branch at the final commit (full core
suite green; see the final report). Timing-based numbers are noisy on the
shared host (load 10-13); use interleaved min-of-rounds or callgrind counts.

**Current measurements:** alias -88.3 / -88.8 dB (saw / pulse, 48 kHz);
circuit-engine CPU 0.518 RTF (baseline 0.296); worst steal 0.930, worst
retrigger 1.258 (Funky 3+4 at 44.1 kHz, quick audit, before the AC coupling);
tab peak -0.26 dBFS (quick audit).
