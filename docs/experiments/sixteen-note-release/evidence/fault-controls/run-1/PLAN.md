# Immediate sixteen-voice repeated-note release

Base: published `e27c34f39320057e09a34005ad2470d1231b4f87`.

## Failure and bounded scope

The processor broadcasts one Note Off to both instruments. Each independently
releases its newest matching held group. Filling primary with notes 60–67 and
restriking 60 in the extension therefore releases both strikes with one off.
The real-processor baseline probe reproduces this in Legacy/Circuit and 0/32
frame callbacks. The existing software contract is newest held onset, not FIFO.

This increment will coordinate **immediate, non-arpeggiated** releases only.
Both scanners must be empty and scan timing disabled. Candidates produced by
deferred scanner dispatch or arpeggiator execution remain unqualified even after
the feature is disabled. Ineligible cases retain the existing broadcast path;
queued/ARP cross-engine release remains unfinished. No claim of complete
sixteen-voice release correctness is made.

## Intended contract

- Use a shared processor-owned allocation-order source, separate from DSP age
  and local unison release-group identity. It labels direct accepted allocation
  in all modes, including primary groups surviving unison-to-sixteen changes.
- Preserve each engine's existing local candidate selection: matching waiter
  first, otherwise newest held voice by local allocation age. Across engines,
  cancel one matching waiter before releasing any sounding group; otherwise
  release one newest comparable group. Whole unison groups stay together.
- Waiter duplicates preserve the first retained key. An immediate promotion is
  a new accepted allocation with zero initial touch, as today. A promotion from
  a deferred callback remains unqualified.
- UI waiter filtering remains source-aware. A selected host-owned held group
  blocks a UI release rather than falling through to another engine. Ordinary
  host releases keep existing source-agnostic semantics.
- Link the shared order source once, on construction, with a lifetime longer
  than both engines. Engine reset invalidates owned metadata but does not reset
  the shared sequence. Exhaustion disables qualification without wrapping.
- Eight-voice dispatch, controls, audio arithmetic, local ages, scan scheduling,
  arpeggiator rules, saved state and parameter identifiers remain unchanged.

A scanner preview is deliberately insufficient: later lower-pitch events in the
same scan can change allocation before an already queued off executes. Complete
support needs a separately qualified scheduling/ownership policy.

## Gates fixed before implementation

- Reproduce the real processor failure, then prove first/second offs release one
  onset at a time in both directions with deliberately skewed local ages.
- Core/router and real-processor tests: channels, repeated strikes, zero-velocity
  Note On, CC64, matching waiters/duplicates/promotions, UI aliases, unison and
  partial groups, invalid input, reset/panic and 8/16 transitions.
- Prove fallback for active/pending scans, prior deferred candidates and ARP;
  compare those paths with the old two-engine broadcast behavior rather than
  silently claiming the bug is fixed there.
- Test distinct/unlinked sources, denied rebinding and order exhaustion.
- Preserve eight-voice/ordinary unchanged-path raw audio and existing ownership,
  scanner, tuning/routing-oracle, controller and keyboard gates. Run the complete
  core suite and focused native neighbors; qualify compiler/sanitizer results
  explicitly. Behavioral omissions must fail normally, not compile/crash.
- Independent source, test and evidence review before publication. No push from
  this worktree without parent coordination. No CPU, external DAW/controller,
  GUI, circuit-fidelity or complete MIDI conformance claim.
