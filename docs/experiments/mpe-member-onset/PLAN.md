# MPE member onset retention: bounded contract

Base: `eebc94e` (published main). This plan precedes implementation.

## Defect and source

A finite member-channel pitch bend, channel pressure or CC74 before Note On
is discarded by the current voice initializer. The identical events after
allocation set the expected targets. MPE v1.1 (M1-100-UM, 14 April 2022),
sections 2.2.6–2.2.8 and 2.4, explicitly requires retaining these messages
without sounding notes and using them for a new note's initial state.
The official PDF linked from https://midi.org/mpe-midi-polyphonic-expression
has SHA-256 `c5f6727af065e454fe9bc4ff80f66414e8b25ffb6c1019ce7795ab4a89da1f1e`.

## Selected increment

- Store the latest finite, clamped raw bend/pressure/CC74 for each currently
  assigned MPE member channel. Defaults remain the existing zero targets;
  do not invent a new no-message timbre default.
- At actual accepted voice allocation, seed the member targets from this
  state, using the current effective member range and the current pressure
  curve/threshold exactly once. Existing active-voice event paths, their
  arithmetic and controller smoothing remain unchanged.
- Apply to Legacy and Circuit with MPE enabled. No retention for conventional,
  PURE, gap or manager channels; no retained polyphonic pressure.
- Clear retained values on Engine reset and an MPE enabled/disabled edge.
  A channel changing its role loses its retained member state. Same-role
  reconfiguration retains raw values and future onsets use the new range.
- CC121 reaches the addressed member bend/pressure cache through its existing
  reset routes and retains CC74. Manager resets do not rewrite independent
  member caches. No saved state, host parameter or UI change.
- Queued KAS notes, waiting keys and arpeggiator steps use the latest state at
  accepted allocation, not a new snapshot captured at input arrival.
- Both instruments receive events only while already active in sixteen-voice
  mode. Do not add a partial controller handoff on extension activation.

## Required gates

1. Finite bit-pattern equality of audio, exposed target state and routed timbre
   against ordinary controller events immediately after the same accepted
   allocation, with no render gap. The reference must use existing event
   handlers, not the new cache helper. Include both engines, all member channels
   in lower/upper/dual zones, independently addressed channels, both ranks,
   rates 44.1/48/96/192 kHz and HQ in a bounded representative subset.
2. Explicit expected bend/pressure/timbre targets; subsequent event smoothing,
   reuse/unison, KAS/waiters/arpeggiator, manager addition, CC121, mode/role/reset
   lifecycle, same-role ranges, malformed events, and PURE/conventional/gap
   preservation. Partitioned rendering must retain exact results.
3. The unchanged base must fail the new behavior tests normally. Focused fault
   controls must expose omitted fields, reset/role invalidation and accidental
   scope expansion. Build/crash/truncation is not a successful negative.
4. GCC and Clang generic/native builds, full-core sanitizers, neighboring core
   regressions and existing strict compatibility gates. Report every stage's
   actual scope and results. No performance claim is required or inferred.

## Deliberately not completed

This is not full MPE conformance. Existing manager pressure/timbre combination,
member changes reaching released tails, MCM note termination, eight-to-sixteen
activation state transfer, live active-note range retargeting and real controller/
DAW/plugin-wrapper qualification remain separate. No filter/memory activation,
CPU optimization, allocator ownership change or circuit-fidelity claim.
