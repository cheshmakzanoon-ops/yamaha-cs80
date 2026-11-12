# Four intrinsic performance-macro automation lanes

The existing macro1–macro4 controls are appended at indices 108–111, bringing the
sample-accurate roster to 112 while retaining the literal 108-lane prefix. The
only production change is the roster append. DSP, metadata, routing, source
authority, serialization and protected UI sources are unchanged.

## Verified results

All nine reviewed stages completed once: candidate/baseline builds, six individual
runtime configurations and collection. No runtime stage was interrupted or retried.
Completion markers bind source, four binary identities and recorded evidence.

- 30 full plus 30 compatibility cases: four isolated macros and their combination
  at 44.1/48/96 kHz in realtime/offline modes.
- 648 finite renders (438 candidate + 210 frozen108).
- 120 actual old/new PCM pairs byte-identical, retained as 240 capture files.
- 120 scalar/block comparisons (7/64/256/1024), maximum error zero.
- 24 combined reversed-queue checks byte-identical.
- 30 exact initial 1,024-frame holds and overall held-note effects
  0.000759895–0.0116455.
- 24 independent combined per-macro witnesses, effects
  0.000739954–0.0115045. All effects exceed the predeclared 0.00001 minimum.
- Empty/null-queue, PURE and Legacy final-point flattening controls pass.

Histories use manual/manual sources, two notes and 16,384 stereo frames, with
macros initially neutral at 0.5. Subsequent values are 0.75/0.25/0.70/0.30/0.5;
combined histories have distinct interior offsets. Motion/combined scenarios
select the existing 18 Hz sine LFO1 filter destination; other LFO destinations
are off. Ring is fixed at 13 Hz and compatibility SUB sine at 8 Hz. Drift and
base dispersion are zero before the width macro's intrinsic mapping is applied.

Brightness, motion, width and space retain their existing compound mappings.
This does not isolate their constituent synthesis effects or redefine them.
Compatibility histories cover noqueue, four representative preceding controls,
Legacy and PURE against the actual frozen108 module using an isolated-header
runner. They do not requalify every prior lane. This fixture uses ordinary
parameter setup, not custom state loading, and makes no state-readback claim.

## Limits

No full-range, source-mask, constituent-effect, native DAW, cross-platform,
CPU-target or hardware-fidelity qualification is claimed. Two historical exact108
guards were made prefix-compatible and source-reviewed, not rerun. Historical
contracts and evidence remain unchanged. Only the new narrow candidate CMake
target/module and isolated baseline runner were built; no broad/malformed-state,
editor/window, scanner, benchmark or display/privilege workaround was run.
Compatibility PCM captures remain in their build directories with stage-recorded
hashes. Active scalar/order/witness renders are not retained; their results are
established by the reviewed fixture and passing logs, not later PCM recomputation.
