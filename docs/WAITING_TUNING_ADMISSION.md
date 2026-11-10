# Waiting circuit keys obey the current tuning map

Base: published `58aabe6`.

A circuit-engine key waiting behind eight held circuits was promoted without
checking whether the current tuning table still mapped it. A table change could
therefore cause an excluded key to sound as soon as another circuit became free.

Promotion now selects the lowest currently mapped waiting key before choosing a
circuit. Excluded held keys remain in the bounded waiting list: key-up can cancel
them, and they can become eligible on a later reconsideration after remapping.
They do not block higher mapped waiting keys or advance the allocation cursor.
The read-only scanner routing projection follows the same selection policy.

This does not make loading a tuning table itself trigger promotion. Existing
note-off, pedal release and channel-mode reconsideration points remain unchanged.
Already sounding notes retain existing table-update semantics. The MTS case uses
the direct table API's mapped flags, not SysEx exclusion support.

Regression covers Scala/MTS table modes, immediate/scanned source-engine dispatch,
and retained-versus-released excluded waiting keys. The baseline expanded suite
reports 22,385 checks with 16 failures: excluded key40 sounds and prevents mapped
key41 from receiving the newly freed circuit. It also checks later remapping and
key-up cancellation. Existing 1,152 scanner routing comparisons remain in suite.

The older scanner qualification explicitly preserved the no-remapping behavior.
This change intentionally replaces that compatibility assertion: an excluded
waiter must leave the circuit available, while a mapped queued strike must fill
it and match actual dispatch. This is a bounded admission policy correction,
not merely a projection optimization. Existing held-key storage capacity and
initial-touch-zero promotion remain unchanged.

Final fresh Release validation passes both targeted CTests (controller reset and
tuning/MIDI), including the corrected compatibility and projection assertions.
This is scoped core verification, not a full suite or native host qualification.
