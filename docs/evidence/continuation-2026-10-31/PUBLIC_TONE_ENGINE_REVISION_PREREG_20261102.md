# Explicit Engine factory-tone revision — preregistration

2026-11-02. Base: completed local `ce307860fc64a0c151991983f6157463d2fd707c`,
ultimately based on `058224fad844b8f51e211e3b820c0ae2d8f25762`. This branch
excludes the subsequent unfinished calibration research. Root authorized
a real **non-default candidate** after the independently challenged public
tone estimate; no production default, saved-state meaning or acceptance
criterion is changed.

## One intentional audio difference

An explicit `FactoryToneRevision::publicAmendedV2` in `ControlFrame` selects
the already tested `compileTonePublicEstimate` for active source-factory
ranks. The 25 choices, version2/hash, exclusions and low-confidence direct
alternatives are fixed by [the public policy preregistration](PUBLIC_TONE_ESTIMATES_PREREG_20261102.md).
The default is `frozenV1`; existing aggregate frames and programmer frames
retain it. The separate `source_factory::modelVersion` stays2. This is a
factory-tone revision, not a rename of every historical Source V2 model.

The following must not change in this increment: any parameter ID/range/
default, any factory program/ID, state serialization/migration, programmer
PANEL/memory semantics, default source tone compiler, oscillator/filter/VCA
equations, HP alignment, detune, fade/steal behavior, audio limits, CPU
workloads or original qualification fixtures. Plugin exposure/persistence
and a default switch are later explicit reviewed steps.

## Test-first transaction and cache contract

Append the typed field first without consuming it, then run the new
Engine tests against this real old V1 execution path. This must fail the
new V2 and invalid-revision expectations, not merely fail to compile.

Implementation must validate revision1/2 before mutating the Engine;
unknown values are atomic transaction failures. Keep **per-rank compiled
revision** in the cache, not just a global requested revision, so a
Legacy excursion cannot leave stale V2 controls when returning to V1 at
the same factory address. Compile only actual source-factory ranks;
repeated identical frames must not recompile. Switching revision preserves
all publicly exposed oscillator/filter/envelope/VCA semantic readouts at command time.
These are not complete authoritative private filter memory or precise ADSR state.

Registered checks:

- All22 factory selections, both source render policies, all three audit
  rates: default/explicitV1 provenance and source buses unchanged; explicit
  V2 uses the already independently tested policy and retained masks.
- The seven complete tones remain bit-identical in actual rendered PCM;
  default and explicitV1 render identically for all22. No object-padding
  comparisons substitute for semantic state comparisons.
- All222 Legacy programs remain bit-identical with V1 versus V2 requested;
  the selector revision is inert in Legacy.
- Actual V2 audio must differ from V1 for a preselected affected Brass
  source under a nonzero note/control fixture, with no gain normalization.
  A difference is expected behavior, never click acceptance.
- V1↔V2, repeated frames, mixed ranks, Legacy and PANEL/programmer trips,
  `setParams`, `prepare` and `reset` retain the declared cache/state rules.
- Invalid revisions0/3/99 reject atomically, including when Legacy is
  requested; output parameters, source identity/buses, compile count and
  semantic signal states remain unchanged.
- Compile real isolated mutants restoring V1 compilation, dropping the
  revision cache key, and deleting revision validation. Ordinary semantic
  failures, not build errors, must detect them; restore positives.

## Candidate audit path, not a replacement baseline

Add `--factory-tone-revision frozen-v1|public-amended-v2` to the existing
audit tool, default `frozen-v1`. Pass it through the actual ControlFrame
used for audio. A V2 run writes a conspicuous `candidate-policy.json`
sidecar identifying revision, policy hash, non-default status and lack of
qualification. Existing default command output/inventory remains unchanged.
Reject V2 with `--suite cpu` before creating output: the original registered
CPU fixture cannot be silently changed into different tones.

The original885 scenario inventory, timing, rates and numeric gates remain
the candidate's comparison contract, but its source tree/revision must be
separately pinned and labeled. A candidate result cannot replace original
V1 qualification. Root/verification schedule the full opt-in885 run after
this bounded implementation and independent review; no default switch
is authorized by passing the tests above.

## Pre-execution coverage extension

Before running the first candidate test, compare every exposed semantic oscillator,
filter, envelope and VCA state in all 16 rank lanes at revision transitions and
invalid-frame rejection (not raw struct padding). Exercise the mixed-rank cache
case where rank I retains an inactive V2 cache while rank II compiles V1, then
rank I reactivates at V1. Test unchanged MEMORY 1, MEMORY 2 and PANEL programmer
controls and compile counts across the factory-only revision switch.

## Audit-wiring tests before CLI implementation

Compile the actual audit source into a test-only renamed-main translation unit,
then exercise its real parser and `render` function. The existing executable must
compile but fail the new flag/sidecar/audio expectations. Default versus explicit
V1 latency CSV and affected-tone PCM must be exact. The candidate must propagate
past the programmer-frame builder and match direct explicit Engine V2 PCM, not
merely print a sidecar. Check the unchanged 222-program/11-tab inventories for
both policies. Reject unknown revision spellings and candidate CPU runs before
creating any output. A source mutant dropping render propagation must fail the
actual-audio assertions. This is a wiring oracle, not independent analog truth
or full885 acceptance. No timing/metric fixture is altered.

The first audit test build found a test-only name collision with the existing
`cs80::reference` namespace. It is retained as a build failure, not counted as
semantic RED; rename that helper and repeat before implementing the CLI.

The isolated source-mutation runner additionally replaces per-rank cache
publication with a global fill of the requested revision. The mixed active/
inactive-rank and Legacy excursions must kill this realistic stale-cache bug,
not only the simpler omission of the revision comparison. Each staged input is
byte-verified, real Engine/audit sources are compiled, compile failures never
count as killed mutants, and original-source hashes plus restored positives are
checked after the batch. All runs use the shared heavy-check lock.

## Independent-review hardening before final qualification

Numeric `vector<float>::operator==` cannot establish the promised bit identity:
+0 and -0 compare equal. Add signed-zero oracle negatives before replacing the
numeric helper with per-float `bit_cast<uint32_t>` equality. Preserve original
PCM fixtures and treat this as test hardening, not an audio change. Explicitly
exercise each MEMORY1/MEMORY2/PANEL return to both factory ranks. DynamicSnapshot
observes the listed public semantic readouts only; it does not access every
private authoritative state field. Same-binary default/explicit V1 equality is
not a pristine-prechange Engine replay; add a separate byte-pinned ce307860
cross-binary default-path replay before claiming that bounded preservation.

Root review also identified buffered sidecar-write failures hidden by checking
stream state before destruction. Extract the existing writer unchanged, then
challenge it with a stream buffer that accepts data but fails on sync. Require
an ordinary failure before adding explicit flush/check and file close/check,
so suite execution cannot continue after a failed candidate identity write.

### Pristine cross-binary preservation scope

Before replay, pin pristine tree `561f6bf37b8dcc46c54b5a35e328c08f0cbec399`
(local historical ce307860; a reauthored commit with the identical tree is valid).
Compile the exact same revision-free fixture against that source and the new
candidate source, with the same compiler/options. Emit little-endian float bits
for all22 Source selections ×both source policies ×three rates (256 frames),
and all222 Legacy programs ×three rates (64 frames): **798 cases**. Repeated
default ControlFrames and finite output checks are included. Compare complete
serialized bytes, not rounded CSV or numeric float equality. Compile a real
candidate-header mutant changing the default to publicV2; it must change those
bytes while still rendering successfully. Retain full source/fixture/compiler/
binary/PCM hashes. This gate is bounded to its pristine pre-change tree; final
885 qualification must use root's PR6-preserving integrated head, not this older
branch as a substitute. Original CPU workloads remain unchanged.

The final audit-only mutation batch also deletes the actual `policy.flush()`
call. The deliberately failing sync must then cause an ordinary assertion
failure, not a compiler error. Preserve the earlier Engine mutant batch (whose
production Engine source is unchanged), and rerun all updated positive tests.
