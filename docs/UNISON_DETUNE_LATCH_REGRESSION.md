# Unison detune is latched at each accepted onset

Scoped Linux Release/Ninja CMake/CTest run on 2026-11-13 passed **1/1 test,
40/40 assertions**, exit 0. Production is unchanged. This is a core contract
regression, not a new host automation lane or a whole-tree qualification.

Existing `applyNoteOn` computes each circuit's `unisonOffset` from the accepted
`global.unisonDetune`. Later amount edits leave that offset intact. A subsequent
accepted strike uses the new amount; changing to zero does not collapse an
already-playing group's spread. This is intentional software unison behavior,
not a physical CS-80 circuit claim.

Six cases cover fixed Legacy and source-performance policies, each with x2/x4/x8
unison at 48 kHz. Both ranks' public oscillator-Hz diagnostics are converted to
fractional MIDI pitches and compared to the independent symmetric-offset formula
with a fixed 0.0002-semitone ceiling. The initial group uses 10 cents, a later
group uses 40 cents, and a final onset uses zero after prior releases. x2/x4
simultaneous groups preserve distinct onset amounts. x8 explicitly releases its
first group before the next onset. No voice-stealing claim is made. Final circuit
reuse occurs for x4/x8; x2 can still use previously inactive slots.

The fixture fixes neutral macros, disabled scanner/ARP/glide and zero
noise/drift/dispersion. All rendered audio is finite. This checks diagnostic
oscillator frequency, not output pitch estimation, audible stereo width,
full-range behavior, native host queues, PURE, or hardware calibration.

## Evidence and build outcome

[Pre-run plan](experiments/unison-detune-latch/PLAN.md),
[source identities](experiments/unison-detune-latch/source-freeze.json),
[commands and exits](experiments/unison-detune-latch/result.json), and
[CTest output](experiments/unison-detune-latch/test.log) are retained.
Independent source review corrected an overbroad 'reuse' assertion label before
freezing. No runtime threshold was changed.

The first configure attempt failed because Ninja was absent from the current
PATH; it never built or ran tests. Its log/result/launcher are preserved in
`experiments/unison-detune-latch/configure-attempt1/`. A fresh `build-unison-latch-v2`
configuration used the already-established recovery environment, which supplies
the existing official build tools. Plugin was OFF. Only the dedicated target and
linked core library were built. Exact CTest selection was inspected before its
single run. All 87 frozen source/plan identities remained unchanged.
