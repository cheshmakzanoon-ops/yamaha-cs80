# Fixed modern Spring/Plate/Tape wet-dry tail qualification

Test-only pinned115 module. All5,373base tracked paths unchanged. Fixed slot1
Spring(model2),slot2Plate(model3),slot3Tape(model4),separately and together.
Classic selectors0; A/B.5; optional outer gain compensationoff;oversampling2.
Existing nonzero modern model selection overrides classic type0. All three
models use existing equal-power wet/dry laws. No DSP/calibration/authority changes.
No model/type/bypass switching; only selected mixes move,always above0.001.

48kHz realtime/offline.65,536frames,notes48/49on17/18,off8192/8193.
VCA release.005,key-release/master-sustain0. Initial mix.10 through1023, then
.35/.15/.45/.20/.40/.20/.10 at2047/4095/8191/12287/32767/49151/65535.
Combined interior offsets37×slot index. Neutral modulation/manual-manual as prior
FX fixture. Tail window16384..65535 follows dry release. Tape's fixed A.5 gives
nominal .06*sqrt(20)seconds (~.2683s) with existing wow/flutter; no time-control
motion or measured decay-time claim. Dry residual must stay<=1e-6. Scalar-vs-dry
and scalar-vs-constant tail differences must each exceed1e-5.

4scenarios×14captures +7combined =63/config,126retainedPCM (524288bytes each).
32scalar/block comparisons(7/64/256/1024) under frozen2e-6 host bound;
8exact queue-order checks,8exact initial1024frame holds;8overall and6combined
per-lane witnesses>1e-5. Empty/null,Legacy flatten,PURE controls exact.
No cross-build compatibility claim; fixed existing115module. Collector recomputes
all active/control/tail metrics from every retained finite PCM.

Independent source audit before helper build. Four exclusive stages build,run
realtime,run offline,collect; never overwrite/retry incomplete stages. Freeze four
new sources and known qualified module; check all old tracked paths,helper,
module/evidence hashes before and after stages. No production/UI edit,broad suite,
malformed/parser/state repro,scanner,GUI,benchmark or privilege workaround.
Scope excludes allslots/allmodels/allranges,type switching,state readback,native
DAW,AU,crossplatform,CPU target,hardware fidelity and decay-time measurement.
