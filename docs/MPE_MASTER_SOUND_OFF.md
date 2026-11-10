# MPE master-channel All Sound Off

## Scope

CC120 (All Sound Off) received on an enabled MPE zone's master channel now
silences that master and the member channels in the **current negotiated zone**.
This implements the zone-level All Sound Off route described in MIDI Manufacturers
Association RP-053, *MIDI Polyphonic Expression*, version 1.0 (March 12, 2018),
section 2.3.1 and Table 1.

Previously the command affected only the master channel, leaving member notes
sounding. The fix applies one affected-channel predicate to active voices,
scanner events, unassigned held keys, and arpeggiator source keys. All affected
keys are removed before the single final waiting-key promotion pass, so a muted
zone cannot immediately repopulate from its own queued or waiting notes.

## Deliberately unchanged

- CC120 on a member or conventional channel remains channel-local.
- MPE disabled, or a master with no active zone, retains channel-local behavior.
- The other zone and channels outside the addressed zone are preserved.
- Sustain pedal and other controller state are not reset; a new note can still
  be sustained by a pedal held before All Sound Off.
- CC123 (All Notes Off) and the public `allNotesOff` API retain their existing
  routing. This change is not a claim of complete MPE channel-mode conformance.
- Scope uses the current channel layout, not a voice's captured historical zone.
  Zone reconfiguration and sounding-note ownership policy are not changed.
- Shared effects tails are not flushed by a channel-scoped panic.

## Regression coverage

Controller-reset tests cover both lower and upper zones in both engine modes,
scanner-pending notes, source-engine waiting keys, other-zone waiting promotion,
master-pedal preservation for future notes, member-only panic, conventional and
disabled-master routing, and removal of arpeggiator source notes. The existing
controller-reset and channel-mode matrix remains in place.

Verification uses the controller-reset, core, scanner-ordering, and tuning-MIDI
CTest targets. This is bounded engine verification, not native-host qualification,
a full-suite rerun, or a performance claim.

### Recorded result

The final controller-reset test source linked against the preceding unchanged
core library reproduced **18 failures out of 3,819 checks**. Against the fixed
library it completed **3,819 checks with zero failures**. The four targeted
CTest targets above passed together (4/4, 24.66 seconds). Timing is test-run
elapsed time only, not an audio-performance benchmark.
