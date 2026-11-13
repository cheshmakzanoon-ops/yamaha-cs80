# Existing 115-lane FX mixes with asymmetric factory/manual sources

Test-only gate on the qualified FX115 module. All 5,373 base tracked files remain
unchanged. No production, DSP, protected UI, metadata, state format, routing or
source-authority change; no added lane or cross-build compatibility claim.
Independent source review preceded freezing, helper build and native execution.

## Recorded results

Eight stages passed once without interruption or retry: narrow helper build,
six configuration runs, collection. Four frozen source identities, fixed module,
helper and previous-stage evidence/capture hashes were checked throughout.
Fixed module SHA256:
`8c05a7273d12ff0fc7f6de8ae33cb8467cc7459a65290de03994d4147f291667`.

- Linux VST3 float32, 44.1/48/96 kHz, realtime/offline.
- Two fixed source layouts: factory selector 0/manual 13 and manual 13/factory 0.
  Four classic FX scenarios per layout/configuration: chorus/phaser/drive slots
  separately and together, fixed types 1/2/5, models 0, outer matching off.
- 756 finite retained stereo PCM captures, 126/configuration, 16,384 frames each.
- 192 scalar/block comparisons (7/64/256/1024): maximum error 0.
- 48 reversed-queue checks and 48 initial 1,024-frame holds: exact bytes.
- 48 overall mix-motion effects: 0.008732296526432037–0.07085740566253662.
- 36 combined per-lane effects: 0.010633677244186401–0.05993552505970001.
- 48 selected-layout versus manual/manual scalar differences:
  0.09908725880086422–0.3040674068033695.
- Empty/null, Legacy final-point flattening and PURE moving/constant controls
  are exact within each layout.

All contribution and source-layout witnesses exceed their predeclared 0.00001
bounds over frames 1024–14335. The historical 2e-6 scalar tolerance is unchanged.
The collector recomputed every active/control comparison and witness from the
retained PCM, rather than relying only on runtime log assertions.

## Exact source scope

Factory selector 0 maps to reconstructed String1 for rank 0 and String2 for rank
1. These are hardware-source addresses, not host program 0 or measured hardware
claims. Every render passes both selectors explicitly to the existing initial
setup flush. Each source-manual control is a separate fresh instance with both
selectors 13 and identical active scalar curves. No live source switches or
MEMORY selections occur. The output difference proves the fixed selected layout
contributes in these histories, not correctness of every factory-tone law or
masking of panel automation.

Global FX slots remain after rank summation and shared chorus. Existing selected
rank semantics remain unchanged; panel settings do not redefine factory controls.
Two notes, fixed FX types and conservative mix histories; global modulation
neutral, no global drift/dispersion/analog noise. See the
[frozen plan](../../FX_SOURCE_MIX_PLAN.md) for exact histories and source grounding.
Combined lane witnesses do not isolate internal processing components.

No all-preset/source-mask/range coverage, state readback, native DAW, AU,
cross-platform, CPU-target or hardware-fidelity claim. No historical native suite
rerun, broad suite, malformed/parser/state reproduction, scanner, GUI, benchmark
or privilege workaround was used.

Git retains source, plan, freeze, logs, stage markers and hashes. Actual PCM and
helper remain in build-fx-source-mix; the module remains in the prior FX115 build.
Historical qualification artifacts and contracts were not replaced or relaxed.
