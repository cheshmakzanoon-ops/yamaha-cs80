# Existing115-lane classic delay/reverb tail qualification

Test-only extension on the prior FX115 module. All tracked source and evidence
from base2305421d remain unchanged. No production, DSP, UI, metadata, routing,
state-format or source-authority change; no new capability lane or cross-build
compatibility claim. Independent source review preceded execution.

## Recorded results

Four stages completed once without interruption/retry: helper build, realtime,
offline, collection. Fixed115 module SHA256:
`8c05a7273d12ff0fc7f6de8ae33cb8467cc7459a65290de03994d4147f291667`.
The module and helper identities, four-source freeze and stage evidence hashes
were verified throughout. Only the new narrow helper was compiled.

- 48kHz Linux VST3 float32, realtime/offline; three scenarios in each mode.
- Slot1 classic tape delay(type3), slot2 classic plate reverb(type4), separately
  and together. Slot3off; models0, gain compensationoff; fixed types throughout.
- 96 finite stereo PCM captures retained locally,48/mode,65536frames each.
- 24 scalar/block comparisons(7/64/256/1024): maximum error0.
- 8 reversed-queue-order comparisons and6initial1024-frame holds: exact bytes.
- 6 overall effects:0.0032468903809785843–0.006014822982251644.
- 4 combined per-lane effects:0.002431372180581093–0.0029767388477921486.
- Empty/null, Legacy final-point flattening and PURE moving/constant controls exact.

Two notes release at8192/8193frames after starting17/18; short VCA release.005,
key-release and master-sustain0. The tail window begins16384frames and ends65536,
beyond the nominal.34s tape tap including wow and the dry release. Maximum dry
residual amplitude was5.945803422946483e-7–9.64835749073245e-7, below the declared
1e-6 bound, not exactly zero. Effect-versus-dry differences in this window were
0.0006158955192461235–0.005362069507782508; moving-versus-constant mix differences
were0.00042081573337782174–0.0034496564185246825. Both exceed the fixed1e-5 gate.
The collector recomputed every metric from retained PCM, not only runtime logs.

## Limits and retention

See [frozen plan](../../FX_TAIL_AUTOMATION_PLAN.md) for exact histories and windows.
This establishes supported fixed-effect mix automation and released-tail response
at48kHz, not measured reverb decay times, tape accuracy, all slots/models/ranges,
type/bypass switching, individual internal-effect isolation, state readback,
native DAW, AU, cross-platform, CPU-target or hardware-fidelity qualification.
No historical native suite was rerun; no broad suite, malformed-state/parser,
scanner,GUI,benchmark or privilege workaround was used.

Git retains plan, source freeze, logs, stage markers and hashes. Actual PCM and
compiled helper remain in local build-fx-tail; the fixed module remains in the
previous FX115 build. The collector manifest identifies all96captures. Old
historical artifacts and contracts were neither replaced nor relaxed.
