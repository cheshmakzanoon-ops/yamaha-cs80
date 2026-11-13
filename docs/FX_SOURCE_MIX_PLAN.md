# Fixed classic FX mix automation with asymmetric factory/manual sources

Test-only gate on the existing 115-lane module. All 5,373 tracked base files stay
unchanged. Two fixed source layouts: source0 is rank 0 factory selector 0 / rank 1
manual selector 13; source1 is rank 0 manual 13 / rank 1 factory 0. These are
hardware-source selectors, not host program numbers. `SourceFactoryTones.h`
addresses map rank 0 selector 0 to String1 and rank 1 selector 0 to String2.
These are the existing reconstructed source addresses, not measured hardware
parity. No other factory preset, MEMORY selector or live source switch is used.

The existing `Instance` constructor supplies `hardware_source_1` and
`hardware_source_2` in the ordinary initial parameter flush as selector/13.0.
Every `renderSession` explicitly passes both fixed source selectors. Source
selection is not serialized, patched or inferred from a filename. `Engine.cpp`
applies the three global FX slots in series after rank summation and the shared
chorus path, rather than treating FX mix as a selected rank panel control. This
gate tests ordinary global FX mix histories in the two selected layouts; it does
not prove masking of panel queues, per-rank isolation or all-source authority.

## Effects, histories and unchanged bounds

Classic slot types 1 chorus / 2 phaser / 5 drive, separately and all together.
Modern models 0; optional gain compensation off; oversampling 2; no live type,
model, bypass or source switch. Selected mix stays above the bypass threshold.
Six configurations: 44.1/48/96 kHz, realtime/offline Linux VST3 float32.
Each layout uses the original four FX115 scenarios: 16,384 frames, notes 48/49
on 17/18, off 14336/14337. Global modulation is neutral (macros 0.5, matrix
amounts 0, global drift/dispersion/analog noise 0). Panel parameter settings do
not redefine factory-selected rank controls; existing factory semantics remain.

Mix starts 0.10 through frame 1023 then 0.35/0.15/0.45/0.20/0.10 at
2047/4095/8191/12287/16383. Combined interior knots offset by 37 times slot index.
Each layout has 63 captures: four scenarios times 14 plus seven combined
controls. Two layouts times six configurations = 756 retained finite stereo PCM
files, each 131,072 bytes, with unique source0-/source1- names.

192 scalar/block comparisons (7/64/256/1024) retain the historical 2e-6 tolerance.
48 reversed-queue checks and 48 initial 1,024-frame holds must be byte-identical.
48 overall and 36 combined per-lane motion witnesses must exceed the unchanged
1e-5 threshold in frames 1024..14335. Empty/null, Legacy final-point flattening
and PURE moving/constant controls are compared within the same fixed layout.
Each scenario also renders the same active scalar curves in a fresh manual/manual
instance (explicit selectors 13/13), retained as `source-manual.pcm`. All 48
selected-layout versus manual/manual witnesses must exceed a separately declared
1e-5 threshold in frames 1024..14335. This confirms an output contribution from
the selected fixed factory/manual layout, not correctness of all factory tone
laws or panel-queue masking. No cross-layout identity, state-readback or new
cross-build compatibility claim.

## Execution and preservation

Independent source review before freeze/build/runtime. Pin four new sources and
known qualified module SHA256:
`8c05a7273d12ff0fc7f6de8ae33cb8467cc7459a65290de03994d4147f291667`.
Eight exclusive stages: helper build, six explicit configuration runs, collection.
Verify every tracked base path remains unchanged and source, module, helper and
completed-stage evidence hashes before/after each stage. Collector independently
recomputes all active/control comparisons and witnesses from every retained PCM.
Do not overwrite or retry an incomplete stage; preserve failures and investigate.

No production, DSP, protected UI, metadata, state, routing or authority change;
no broad suite, malformed/parser/state reproduction, scanner, GUI, benchmark or
privilege workaround. No all-factory/source-mask/range qualification, MEMORY
use, source switching, native DAW, AU, cross-platform, CPU or hardware claim.
