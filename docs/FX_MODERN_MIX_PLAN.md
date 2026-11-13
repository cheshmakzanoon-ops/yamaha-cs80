# Existing115-lane modern ensemble/drive wet/dry qualification

Test-only existing115 module gate. All5373base tracked paths remain identical.
Fixed slot1 modern BBD ensemble(model1),slot2 Drive(model5),slot3 ensemble(model1)
separately and together. Classic type selectors all0; selected nonzero model
chooses the existing modern effect even when classic type isoff(FxSlot::liveTypeFor).
No model/type switching. A/B both.5,oversampling2,optional outer gain compensationoff;
Drive's intrinsic wet loudness matcher remains active, as source defines. Modern
ensemble/drive use the existing linear mix law(FxModels.h); no calibration/DSP edits.

Six configs44.1/48/96kHz × realtime/offline. Ordinary same two-note16384frame
histories as the original FX115 fixture,manual/manual,neutral macros/matrix,
no drift/dispersion/noise. Mix.10 through1023 then.35/.15/.45/.20/.10 at
2047/4095/8191/12287/16383,interior offsets37×slot index in combined case.
No parameterA/B automation. Selected effects stay above the bypass threshold.

Four scenarios13renders each plus7combined captures =59/config,354retainedPCM.
96scalar/block comparisons(7/64/256/1024),24exact queue-order checks,24exact initial
holds,24overall and18combined per-lane witnesses>1e-5. Existing host tolerance2e-6.
Empty/null,Legacy final-point flattening,PURE moving/constant exact. All PCM kept
and finite,131072bytes each. No cross-build comparison claim; fixed115 module.

Independent source audit before helper build/runtime. Stages:build,six explicit
runs,collect; fresh exclusive directories/no retry or overwrite of incomplete
stages. Freeze four new sources and known-qualified module SHA; verify old tracked
paths,runner/module and previous evidence identities. Collector recomputes all
active metrics and controls directly from retained PCM with frozen thresholds.
No production/protectedUI edits,broad suites,malformed/parser/state repro,scanner,
GUI,benchmark or privilege workaround. This is not modern reverb/echo coverage,
allmodel/allrange,nativeDAW,AU,crossplatform,CPUtarget or hardware fidelity.
