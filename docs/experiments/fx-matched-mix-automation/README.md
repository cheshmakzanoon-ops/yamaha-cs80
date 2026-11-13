# Existing 115-lane FX mix automation with outer gain compensation

Test-only gate on the qualified FX115 module; all 5,373 base tracked paths remain
unchanged. No production, DSP, protected UI, metadata, state-format or source
policy edits; no new lane or cross-build compatibility claim. Independent source
review preceded freezing, narrow helper build and native execution.

## Recorded results

Eight stages passed once, without interruption or retry: helper build, six rate/
mode configurations, collection. Four frozen source identities, module/helper
and all preceding stage evidence/capture hashes were checked throughout.
Fixed module SHA256:
`8c05a7273d12ff0fc7f6de8ae33cb8467cc7459a65290de03994d4147f291667`.

- Linux VST3 float32 at 44.1/48/96 kHz, realtime/offline.
- Slot 1 BBD ensemble (model 1), slot 2 Drive (model 5), slot 3 BBD ensemble
  (model 1), separately and together; four scenarios per configuration.
- 378 finite stereo PCM captures retained locally: 63/configuration,
  16,384 frames each.
- 96 scalar/block comparisons (7/64/256/1024): maximum error 0.
- 24 reversed-queue checks and 24 initial 1,024-frame holds: exact bytes.
- 24 overall mix-motion effects: 0.0013731778599321842–0.022829514928162098.
- 18 combined per-lane effects: 0.0007988223806023598–0.011701073497533798.
- 24 matched-ON versus matched-OFF scalar effects:
  0.00003229454159736633–0.0012185629457235336.
- Empty/null, Legacy final-point flattening and PURE moving/constant controls exact.

All contribution and ON/OFF witnesses exceed their predeclared 0.00001 bounds
in frames 1024–14335. The historical 2e-6 scalar tolerance was unchanged.
Each matched-OFF control retains the same active mix curves and other parameter
values, changing only the outer compensation enable flag in a separate render.
The collector recomputed every comparison and witness from all retained PCM,
not solely from runtime logs. This shows the outer compensation changes output
in these histories, not that it meets a loudness calibration standard.

## Scope and limits

Classic selectors 0; fixed modern models, A/B 0.5 and oversampling 2. Optional
outer gain compensation is ON for the main modern scenarios; Drive's intrinsic
wet loudness matcher remains active in both matched-ON and matched-OFF controls.
PURE retains its existing policy disabling FX and optional outer compensation.
Two notes, manual/manual sources, neutral macros/matrix, no drift/dispersion/noise.
See the [frozen plan](../../FX_MATCHED_MIX_PLAN.md) for exact source grounding,
histories and windows. Per-lane witnesses show contribution within the chosen
combined history, not isolation of internal processing components.

No live compensation/type/model/bypass switches, model A/B automation, modern
reverb/echo, all-model/range coverage, convergence/standards compliance, state
readback, native DAW, AU, cross-platform, CPU-target or hardware-fidelity claim.
No historical native suite rerun, broad suite, malformed-state/parser test,
scanner, GUI, benchmark or privilege workaround was used.

Git retains source, plan, freeze, logs, stage markers and hashes. Actual PCM and
helper remain in local build-fx-matched-mix; the fixed module remains in the
prior FX115 build. Historical artifacts and contracts were not relaxed.
