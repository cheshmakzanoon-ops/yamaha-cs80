# ANALOG NOISE117 source review plan

Source-only plan; no new build, native execution or qualification claimed.
Base is qualified local ARP116 commit b0ecf0dd70ad72f0ed7bb5bc9a9c44c623f3e7b5.
Append only existing P_analogNoise at index 116. Existing IDs, ranges, DSP, UI,
MEMORY, scanner and policy laws stay unchanged. Literal116 prefix is frozen in
Vst3Frozen116Roster.h. The prior ARP fixture changes only its full-mode roster
guard to minimum116 plus literal prefix; historical evidence is unchanged and
that fixture is not requested to rerun.

## Existing source semantics

The existing analog_noise 0–1 control scales stereo mix-bus hiss before chorus,
FX and output gain. Circuit policy smooths it with the existing control bank.
PURE masks it to zero. It is not a rank-noise control, is not gated by ARP or
notes, and is not masked by factory source selection. Circuit hiss has separately
seeded deterministic generators reset in fresh instances; draws continue when
amount is zero. No claim of calibrated PSD or hardware noise amplitude follows.

## Fixed ordinary-input fixture

Vst3NoiseAutomationTests.cpp uses only the existing supported native helper.
Fresh instances/output directories for each operation. 32768 stereo float32
frames, three ordinary notes48/52/56 at17/18/19, off28672/28673/28674, except
the explicit no-note pair. Manual/manual sources13/13, circuit performance on,
PURE off, ARP off, scanner off; neutral modulation and fixed effects bypassed.
The compatibility prior116 render alone enables fixed ascending ARP and moves
preceding volume/tuning/matrix/FX1mix/ARP RATE lanes to exercise old capability.
The classic drive type stays fixed, initially mix0; no type/model queue exists.

Two scenarios: noise amount alone, then noise amount plus volume. Noise starts
.10 through1023; knots .70/.40/.25/.60/.10 at4095/8191/16383/24575/32767.
Combined volume .60 through1023, then .75/.50/.70/.55/.60 at4132/8228/16420/
24612/32767. Queues use existing normalized interpolation, not a new physical law.

Each scenario retains five actual frozen116 compatibility captures: noqueue,
null, preceding116, legacy-prior, pure-prior. None queues the new noise lane.
Candidate adds Legacy/flatten pair, PURE/noise-held pair (same volume motion),
no-notes/no-notes-noise-held pair, scalar/constant, and block7/64/256/1024.
Combined adds reverse7/64/256/1024 and held-lane0/1. Thus17+23=40 candidate
and10 baseline captures/configuration. Six configs44.1/48/96kHz × realtime/offline:
240 candidate +60 baseline =300 PCM;60 actual byte-equal frozen116 pairs.

Predeclared gates:48 scalar/block comparisons <=1e-8 (stricter than existing
host2e-6, because the low-level effect witness floor is1e-7);24 exact combined
queue-order comparisons;12 exact initial1024-frame holds;12 overall and12
combined individual-lane effect witnesses >1e-7 over frames1024..28671. The
threshold detects this low-level bus control, not audibility/hardware accuracy.
Twelve no-note positive noise witnesses also exceed1e-7 with volume unchanged
between each pair. Noqueue/null, Legacy flattening and PURE masking are exact.
No-note comparisons must NOT be classified as inert. All PCM finite, exactsize.

## Execution boundary

Only explicit cs80_vst3_noise_automation_tests candidate build and frozen116
helper build, six named run stages and collector, after independent source/
runner review and frozen source/binary provenance. Retain every stage/log/PCM;
no overwrite, retry, threshold relaxation, broad CTest/native host suite, parser,
malformed queues, security workarounds or CPU benchmarks. Frozen116 baseline
module is the newly qualified ARP116 module, copied without rebuilding it.
No historical fixture rerun, state readback, DAW/AU/cross-platform, full-range,
spectral-calibration, CPU, hardware or release claim.
