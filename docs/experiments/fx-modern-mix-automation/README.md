# Existing115-lane fixed modern ensemble/Drive wet-dry qualification

Test-only extension using the prior qualified FX115 module. All5,373base tracked
paths remain unchanged; no production, DSP, protectedUI, metadata, routing,
source-authority or state-format modification. No added automation lanes or new
cross-build compatibility claim. Independent source review preceded execution.

## Verified recorded results

Eight stages completed once without interruption or retry: narrow helper build,
six configurations, and collection. Four frozen source identities, known-qualified
module, helper and prior stage/evidence/capture hashes were verified throughout.
Fixed module SHA256:
`8c05a7273d12ff0fc7f6de8ae33cb8467cc7459a65290de03994d4147f291667`.

- Linux VST3 float32 at44.1/48/96kHz, realtime/offline.
- Slot1 BBDensemble(model1),slot2 Drive(model5),slot3 BBDensemble(model1),
  separately and together:24scenarios,354finite retained PCM captures.
- 96scalar/block comparisons(7/64/256/1024): maximum error0.
- 24combined reversed-queue checks and24initial1024-frame holds: exact bytes.
- 24overall held-motion effects:0.0013744556345045567–0.022106478456407785.
- 18combined per-lane effects:0.0007683783769607544–0.011219243053346872.
- All effects exceed the predeclared1e-5 gate. Existing2e-6 host tolerance unchanged.
- Empty/null, Legacy final-point flattening and PURE moving/constant controls exact.

All354PCM captures were retained and all active comparisons/witnesses recomputed
from them by the collector, rather than relying only on runtime log assertions.

## Scope and limits

Classic types are0 while selected modern models are nonzero: the existing model
selection path overrides the classic selector. Models remain fixed during each
render. A/B remain.5; oversampling2; optional outer gain compensation isoff.
Drive's intrinsic wet loudness matcher still runs as defined by existing DSP.
Ensemble and Drive keep their existing linear wet/dry laws. Each slot owns its
own effect state; combined witnesses demonstrate the selected lane's contribution
in this history, not isolation of every internal processing component.

Manual/manual sources, two notes, neutral macros/matrix, no drift/dispersion/noise;
16384frames per capture. See the [frozen plan](../../FX_MODERN_MIX_PLAN.md) for
exact conservative mix histories. No model/type/bypass switches, modelA/B queues,
modern spring/plate/tape qualification, allmodel/allrange coverage, state readback,
nativeDAW, AU, cross-platform, CPU-target or hardware-fidelity claim.

Git retains source, plan, freeze, logs, stage markers and capture hashes. Actual
PCM/helper remain in build-fx-modern-mix; the module remains in the earlier FX115
build. Historical native suites were not rerun; no broad/malformed-state/parser,
scanner,GUI,benchmark or privilege workaround was used.
