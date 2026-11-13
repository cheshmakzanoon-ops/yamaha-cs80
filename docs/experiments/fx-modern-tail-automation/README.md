# Existing 115-lane modern Spring/Plate/Tape tail qualification

Test-only extension on the qualified FX115 module. All 5,373 base tracked paths
remain unchanged. No production, DSP, protected UI, metadata, routing, source
policy or state-format changes; no additional automation lanes or new cross-build
compatibility claim. Independent source review preceded execution.

## Recorded results

Four stages completed once without interruption or retry: helper build, realtime,
offline and collection. Four frozen source identities, fixed module, helper and
all preceding stage/evidence/capture hashes were verified throughout.
Fixed module SHA256:
`8c05a7273d12ff0fc7f6de8ae33cb8467cc7459a65290de03994d4147f291667`.

- 48 kHz Linux VST3 float32, realtime/offline; four scenarios per mode.
- Slot 1 Spring (model 2), slot 2 Plate (model 3), slot 3 Tape (model 4),
  separately and together; fixed models and classic selectors off.
- 126 finite retained stereo PCM captures: 63 per mode, 65,536 frames each.
- 32 scalar/block comparisons (7/64/256/1024): maximum error 0.
- 8 reversed-queue checks and 8 initial 1,024-frame holds: exact bytes.
- 8 overall effects: 0.007278734818100929–0.02541068848222494.
- 6 combined per-lane effects: 0.007156038656830788–0.01655307225883007.
- Empty/null, Legacy final-point flattening and PURE moving/constant controls exact.

Notes release at frames 8192/8193 with short VCA release and zero key-release/
master-sustain floors. Tail comparisons use frames 16384–65535. Dry residual is
nonzero: maximum amplitude 5.945803422946483e-7–9.64835749073245e-7, below the
predeclared 1e-6 bound. Effect-versus-dry differences are
0.007579963487003738–0.0267647623639391; moving-versus-constant mix differences
are 0.004457446746528149–0.021948158275336027 in that window. All required
witnesses exceed the fixed 1e-5 gate. The existing 2e-6 host tolerance is unchanged.
The collector recomputed every metric/control from all retained PCM.

## Exact scope and limits

A/B remain 0.5, oversampling 2, optional outer gain compensation off. Existing
modern model selection overrides classic type 0. All three models retain their
existing equal-power dry/wet laws. Mix stays above the bypass threshold; no type,
model, A/B or bypass switching occurs. Two notes, manual/manual sources, neutral
macros/matrix and no drift/dispersion/noise. The [frozen plan](../../FX_MODERN_TAIL_PLAN.md)
specifies exact conservative histories. Combined witnesses show lane contribution
in this history, not isolated internal-effect components.

No all-slot/model/range coverage, measured decay times, tape/hardware fidelity,
state readback, native DAW, AU, cross-platform or CPU-target claim. No historical
native suites were rerun. No broad suite, malformed-state/parser, scanner, GUI,
benchmark or privilege workaround was used.

Git retains source, plan, freeze, logs, stage markers and capture hashes. Actual
PCM/helper remain in local build-fx-modern-tail, and the fixed module remains in
the earlier FX115 build. Historical artifacts and contracts were not relaxed.
