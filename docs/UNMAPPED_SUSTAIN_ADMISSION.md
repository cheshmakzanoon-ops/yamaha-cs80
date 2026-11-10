# Unmapped keys preserve Sustain-II release tails

Base: published `19e771b`.

A direct note excluded by the selected microtuning table allocated no voice, but
`applyNoteOn` cancelled Sustain-II release tails before checking that mapping.
Pressing an excluded key therefore silenced the previous sound without producing
a new note. Check direct-note mapping before that lifecycle side effect. The
read-only scanner routing projection uses the same order.

Mapping remains checked at dispatch, not queue admission: changing a table while
an event waits preserves the existing dispatch-time policy. ARP source keys remain
admitted regardless of their root's mapping, because generated octave notes can
be mapped; existing timed-rest behavior is unchanged. Mapped strikes still cancel
Sustain-II tails. No allocator, envelope law, pedal or ARP scheduling change is
introduced. The MTS table case uses the direct table API, not a claim that SysEx
can create unmapped keys.

The expanded tuning test fails 16 checks against the pre-fix core, out of 22,353.
Its eight fixtures cover both engine policies, scan timing off/on, and Scala/MTS
table modes. Each confirms an active release tail, exact continuing audio after
an excluded strike, retained voice activity, and normal tail cancellation when a
mapped key is subsequently played. Existing ARP tuning and 1,152 scanner routing
oracle comparisons also remain in the suite. Scoped core tests do not establish
native-host qualification, full-suite success, hardware fidelity or realtime cost.

A fresh Release build passes both targeted CTests: controller reset and
tuning/MIDI. The fixed tuning suite passes all 22,353 checks; controller reset
retains its 3,705 checks. `git diff --check` passes.
