# Fixed delay/reverb automation tails: source-reviewed test-only gate

Use the existing115-lane module from the FX mix qualification. No production,
metadata, source authority, DSP or protected UI modifications; all tracked base
files remain identical. New standalone fixture derives from the reviewed FX mix
fixture. Fixed slot1 classic tape delay(type3), slot2 classic plate reverb(type4),
separately and together; slot3off. Models0, gain compensationoff. No live type,
model or bypass switching. Mix always above0.001 keeps effect history alive.

48kHz realtime/offline only.65536frames, notes48/49 on17/18, off8192/8193.
Rank VCA release.005, key-release and master-sustain0. Initial mix.10 through1023;
then.35/.15/.45/.20/.40/.20/.10 at2047/4095/8191/12287/32767/49151/65535.
Combined interior knots offset37frames for slot2. Same manual/manual neutral
source/modulation fixture as the existing gate. Classic tape's .34s tap is nominally
16320frames at48kHz, with wow up to±33.6frames. Tail window16384..65535frames extends beyond that tap and
short dry release. A dry control must have maximum amplitude<=1e-6 in that
window; scalar-vs-dry tail and scalar-vs-constant tail-motion both exceed1e-5.
This prevents mistaking wet/dry attenuation of live input for delayed output.

Three scenarios ×14renders plus6combined witnesses/order captures =48/config,
96retained PCM at524288bytes each. Scalar blocks1 versus7/64/256/1024 use existing
2e-6 tolerance (24comparisons).8combined reversed-order checks and6initial holds
exact.6overall effects and4combined per-lane effects exceed1e-5. Ordinary
empty/null, Legacy flatten and PURE controls retained. No new cross-build
compatibility claim: production/module remain the prior115 candidate.

Independent source review precedes helper build/runtime. Stages:build,
run realtime,run offline,collect, each fresh/exclusive. Pin fixture,contract,
plan,script before execution; pin fixed module and compiled runner throughout.
Verify every base tracked path unchanged. Collector independently recomputes
all metrics from all96finite retained PCM; stage markers pin evidence and files.
Never overwrite/retry an incomplete stage; preserve errors and investigate first.
No broad suites, malformed inputs,state/parser tests,GUI,benchmarks or privileged
workarounds. No fullrange/allmodels,DAW,AU,cross-platform,CPU or hardware claim.
