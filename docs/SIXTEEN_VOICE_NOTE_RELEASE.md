# Immediate sixteen-voice Note Off ownership

The modern sixteen-voice mode contains two complete eight-voice instruments.
Previously, one Note Off was broadcast to both. Each instrument could release
its own newest matching onset, so overlapping same-channel, same-pitch strikes
in different instruments could both stop after a single release.

## Bounded software contract

When both instruments use immediate, non-arpeggiated dispatch with empty scan
queues, one Note Off now selects one comparable ownership candidate across the
pair. A processor-owned sequence labels accepted allocations; it is separate
from each instrument's local voice age and unison release-group identifier.

- Matching waiting keys take priority over sounding held groups. One newest
  comparable waiter is cancelled first, without releasing a sounding alias.
- Otherwise, the newest accepted matching allocation is released. All circuits
  belonging to that onset remain a group, including partial unison groups and
  groups that survive a live change to sixteen-voice mode.
- A waiter promoted by an immediate event receives fresh allocation order and
  retains the existing zero-initial-touch rule. This is accepted-allocation
  order, not a claim to reproduce the chronology of MIDI arrival.
- Normal channel matching and sustain-pedal behavior remain in the selected
  instrument. A release removes physical hold; CC64 may keep its sound alive.
- UI release keeps the existing source guard. Host-owned waiters are skipped
  by a UI query, and a selected host-owned held group blocks release instead of
  falling through to an older UI group. This is not independent host/UI
  per-voice targeting when both own the same MIDI channel and pitch.

The sequence is bound before either instrument allocates a note, outlives both,
and is not restarted by individual Engine reset or extension reactivation.
Reset clears the instrument's ownership metadata. A different source, an
ambiguous copied identity or an exhausted sequence cannot prove global order
and therefore cannot enter the coordinated path. The counter never wraps.
Linked Engine copies retain the same non-owning source pointer; the caller must
keep that source alive. Production does not copy the linked processor engines.

## Deliberate fallback and remaining work

The existing two-engine broadcast is retained whenever either instrument has
active arpeggiation, active KAS scan timing or queued scan events. A matching
candidate originally produced by deferred scan dispatch or an arpeggiator also
remains unqualified after those modes are disabled. It does not acquire an
invented global age merely because one instrument happens to render first.
A newly accepted immediate promotion can establish fresh qualified ownership.

**Same-pitch cross-instrument release is still incomplete on those fallback
paths.** A complete scanner solution needs coordinated allocation/release
scheduling: later lower-pitch events in one scan can change admission before an
already queued off executes. This increment does not preview a partial queue
and treat it as final ownership. It does not change the arpeggiator contract.

Eight-voice dispatch, local allocator ages, oscillator/filter/envelope math,
MPE expression, controller routes, parameter IDs, serialized state, note-on
routing and extension teardown are unchanged. The added fixed-size metadata
and bounded event-time queries have no new heap allocation or lock. No CPU
measurement or realtime-deadline qualification is claimed.

## Evidence

The [pre-implementation plan](experiments/sixteen-note-release/PLAN.md) and
[qualification record](experiments/sixteen-note-release/README.md) distinguish
the genuine old-processor reproduction, new core and native fixtures, compiler
and sanitizer runs, behavioral faults, unchanged-path compatibility and
independent review. Native tests instantiate the actual processor without
opening an editor; they are not external DAW, physical controller, GUI or
plugin-format-host qualification. These are software ownership rules, not a
Yamaha circuit-fidelity or release-readiness claim.
