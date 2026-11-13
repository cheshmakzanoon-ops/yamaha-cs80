# VOICE DISPERSION119 source review plan

Source only; no build/native qualification claim. Base qualified local DRIFT118
`803caea8157925da35d492681de30a568e741f2f`. Append existing P_voiceDispersion
at index 118, preserving the literal 118-entry prefix. No DSP/UI/MEMORY/ID or
parameter normalization changes. The drift fixture only gains a minimum118 and
literal-prefix forward guard; historical qualification is unchanged/not rerun.

## Existing semantics

The live smoothed amount scales deterministic per-card cutoff, Q, level, tuning
and envelope-time ratios each internal sample. Changed ratios refresh live
filter/VCA times using existing state-continuous setTimes, not an envelope reset.
PURE masks the amount; factory selection does not globally mask these ratios.
Neutral width macro is required because it otherwise adds .22 × width. Fixed
manual/manual sources, neutral macros, drift/noise zero isolate this overlay.
This is aggregate output evidence, not isolated envelope or hardware calibration.

## Four ordinary histories

32768 stereo float32 frames, fresh instances, notes48/52/56 on17/18/19.
Scenarios0/1 sustain until noteoffs28672/28673/28674, amount alone/withvolume.
Amount .10 through1023, then .70/.40/.25/.60/.10 at4095/8191/16383/24575/32767.
Combined volume .60 through1023, then .75/.50/.70/.55/.60 at4132/8228/16420/
24612/32767. Existing normalized interpolation is unchanged.

Scenarios2/3 release at8192/8193/8194, with fixed rank filter/VCA release .5s.
Moving and held controls are identical through10239, including all noteoffs.
Only afterward amount moves to .70/.25/.60 at16383/24575/32767; combined volume
moves to .75/.50/.70 at16420/24612/32767. Therefore a positive difference in
frames16384..28671 cannot merely inherit differing pre-release trajectories.
The entire initial10240 frames must be byte-identical against constant reference.
This demonstrates live post-release output behavior, not isolated envelope timing.

All use source-performance, manual/manual13/13, scanner/ARP off, neutral modulation,
fixed bypassed FX, default fixed thermal model with drift amount0. Only the
preceding118 compatibility render enables fixed ARP and prior lanes including
volume/tuning/matrix/FX1mix/arpRate/analogNoise/drift. No new dispersion queue in
compatibility mode. Fixed classic drive type5, initiallymix0; no type/model queues.

## Counts and predeclared gates

Five baseline-compatible captures per scenario: noqueue, null, preceding118,
legacy-prior, pure-prior. Candidate adds Legacy/flatten pair, PURE/dispersion-held
pair, scalar/constant and blocks7/64/256/1024. Combined scenarios add four reversed
queues and two individual held-lane witnesses. Thus15+21+15+21=72 candidate and
20 baseline per config; six44.1/48/96kHz × realtime/offline configurations give
432+120=552 retained PCM with120 actual byte-identical frozen118 pairs.

96 scalar/block checks <=1e-8 (stricter than historical host2e-6),48 exact order
checks,24 initial1024-frame holds,12 extended10240-frame release holds,72 exact
controls (empty/null, Legacy flatten, PURE mask). PURE preserves volume motion.
24 overall and24 individual-lane effects >1e-6. For release scenarios, individual
witnesses use only frames16384..28671. Twelve overall post-release differences
also exceed1e-6. All PCM finite/exactsize; no no-note silence or all-source claim.

## Execution boundary

Freeze reviewed source, plan, runner and baseline identities before named builds.
Exact target cs80_vst3_dispersion_automation_tests and frozen118 helper only;
six named runs and collector after independent review. Fresh directories,
retained logs/PCM, no retries/overwrites/threshold relaxation/broad suites or
malformed queue/state/parser tests. No security workarounds or CPU benchmarks.
No DSP/calibration, all-source/full-range, DAW/AU/cross-platform, state-readback,
CPU, hardware or release claim. Publication is tracked separately.
