# ANALOG DRIFT118 source review plan

Source only; no build/native qualification claim. Base qualified local NOISE117
84ac43a0452734dd1d71a872119fe959964980f1. Append existing P_drift at index 117
while freezing literal 117 prefix. No DSP/UI/MEMORY/metadata/ID/normalization
changes. The noise fixture gets only a minimum 117/literal-prefix forward guard;
its historical evidence remains unchanged and is not requested to rerun.

## Existing semantics

ANALOG DRIFT is a live 0–1 amount included in existing per-internal-sample
smoothing. Model 0 multiplies seeded per-rank thermal drift cents by amount / .25;
model 1 multiplies the existing .031Hz voice-offset sine by amount × .9 semitones.
Both are existing laws, not a new calibration. Thermal state ticks for all ranks
even silent/amount 0, so amount changes do not alter its RNG schedule. PURE masks
drift to zero. Fresh Engine instances align deterministic thermal/sine histories;
do not rely on reset/reprepare to reset sine phase. Models are fixed per render,
never queued. No stationary distribution, warm-up duration or PSD claim.

## Ordinary native fixture and predeclared gates

Four scenarios: thermal drift alone / with volume, worn-sine drift alone / with volume.
Each uses fresh instances, 32768 stereo float32 frames, manual/manual sources 13/13,
source-performance policy, ARP/scanner off, neutral modulation, dispersion/noise 0,
fixed bypassed effects. Notes 48/52/56 on 17/18/19, off 28672/28673/28674.
Drift .10 through 1023,then .70/.40/.25/.60/.10 at 4095/8191/16383/24575/32767.
Volume in combined scenarios .60 through 1023,then .75/.50/.70/.55/.60 at 4132/
8228/16420/24612/32767. Existing normalized interpolation is unchanged.

Per scenario, five frozen117-compatible captures (no new drift queue): noqueue,
null, preceding117,legacy-prior,pure-prior. Prior curves include volume/masterTune/
tuningRef/mod16Amount/fx1Mix/arpRate/analogNoise; only preceding117 enables fixed
ascending ARP to exercise its old lane. Classic drive remains fixed at type 5,
initial mix 0,activated only by the prior mix queue. No model/type automation.
Candidate adds Legacy/flatten pair, PURE/drift-held pair, scalar/constant and four
blocks 7/64/256/1024. Combined adds four reversed queues and two held-lane renders.
Therefore 15+21+15+21=72 candidate and 20 baseline per configuration; six configurations 
44.1/48/96kHz × realtime/offline give 432+120=552 retained PCM and 120 actual pairs.

Gates: 96 scalar/block comparisons <=1e-8 (stricter than existing host 2e-6);
48 reversed-queue comparisons and 24 initial 1024-frame holds byte-exact;
24 overall +24 combined individual-lane effects >1e-6 over frames 1024..28671.
Twelve same-configuration thermal/worn scalar pairs must differ >1e-6 in that
window, demonstrating fixed model setup matters. 72 exact controls (empty/null,
Legacy flatten,PURE masking); PURE holds only drift and retains volume motion.
All PCM finite/exactsize. No no-note mask assumption or output silence claim.

## Execution boundary

Freeze source/plan/runner and baseline identities after independent review.
Use only exact cs80_vst3_drift_automation_tests build, frozen117 helper build,
six named run stages and collection, fresh directories, retained logs/PCM. No retries,
overwrites, threshold relaxation, broad suites, malformed queue/state/parser,
security workarounds or benchmarks. Copied qualified117 module,not a rebuilt baseline.
No historical fixture reruns, DAW/AU/cross-platform,CPU,state-readback,hardware,
spectral/full-range or release claim. Publication is tracked separately.
