# All16 matrix amount histories: concurrency and queue permutations

Test-only checks on the frozen108-lane module. All production sources, DSP,
metadata, source authority, UI and routing remain byte-identical. Previous isolated
slot tests establish slot identity. This test adds concurrency and queue-order
coverage for a repeated three-source/destination pattern, not general matrix
routing, source/via or routing-edit coverage.

Two public-API canonical sessions enable all16slots or none. Slot i uses MSEG(i%3)
and corresponding pitch/cutoff/level on both ranks. Shapes remain the proven
unsynced poly-triggered always-loop bipolar shapes. Original serializer bytes are
loaded without raw XML editing or production hooks.

Each amount initially0.02 through1023. Interior target magnitudes stay<=0.045;
slot-dependent variation0.001*(i%4), with breakpoint offsets17*i, makes distinct
histories. The repeated pitch group has6slots, cutoff/level5 each, keeping summed
amounts modest. Rate/source/routing choices do not change during rendering.

Six sample-rate/mode configurations, manual/manual sources, capacity8 with two
notes over16,384frames.36renders each:scalar1, forward/reverse/rotate-by5 at
blocks7/64/256/1024 (12), all-constant1,16 individual slot-motion-held witnesses,
off-route2, PURE2, zero-static amounts with routes/on-off2. Require exact initial
1024-frame hold and existing scalar/block tolerance. Each per-slot held witness
must exceed the predeclared1e-5 threshold. No relaxation on failure.

Planned totals:216finite captures,24scalar/block comparisons,48byte-identical
queue-order comparisons,96per-slot witnesses,12native canonical readbacks and
2canonical roundtrips. Captures are test identities, not baseline audio pairs.
No old/new compatibility comparison is claimed.

Qualifier stages are `build`, `generate`, `run RATE MODE`, `collect`. Each uses a
fresh stage directory; completion markers are written only after checks. Existing
partial stages are never retried/overwritten automatically. Source, retained
binary, helper, canonical state and completed evidence identities are rechecked.
Collect requires all six successful configuration markers. Generator and runner
are standalone builds using verified108 SharedCode linkage; no production rebuild.
New CMake targets are source-reviewed only, not executed by this qualifier.

Independent source/freeze review precedes runtime. Historical contracts/evidence
are untouched and broad/malformed/GUI/display-bypass/denied tests are excluded.
No DAW, cross-platform, full-range, hardware-fidelity or CPU-target claim follows.
