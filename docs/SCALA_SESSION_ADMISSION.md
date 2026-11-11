# Reject malformed embedded tuning before session restore

Session restore previously ignored `publishScala` errors. It could restore the
incoming sound, metadata and studio state while leaving a previous session's Scala
table in place. The resulting combination was not the saved session.

A present `WS3_TUNING` child must now parse and build successfully before any other
session restore side effect. Invalid SCL or KBM rejects that restore and leaves the
previous session intact. The existing transactional tuning parser does not publish
on failure. Valid embedded tuning still restores normally; an absent tuning child
still clears tuning. MIDI-learn/editor-state restoration now follows tuning
admission instead of preceding it.

The native fixture tests malformed SCL and KBM, with and without a prior scale.
It compares complete serialized session snapshots and observes metadata, hardware
source selection and tuning presence. The previous UI/MIDI-learn state deliberately
differs from the incoming session. Valid tuning and absent-tuning controls remain.

The final exact fixture reported **27 checks / 8 failures** against the prior
implementation, then **27 checks / zero failures** after the fix. Nine rebuilt
native registrations passed **9/9 in 47.13 seconds**: this fixture, physical
parameter admission, deterministic MTS processor, and six actual-module VST3
rate/mode automation/SysEx tests, with existing tolerances unchanged.

During fixture development, a hard-coded metadata expectation was corrected:
selecting a hardware source legitimately renames the preset. Final tests capture
the actual pre-restore metadata, and both baseline and fixed runs above use that
corrected fixture.

The host's void state-restore interface still provides no new error dialog. This
change does not claim concurrent session-restore atomicity, live MTS service,
external DAW/GUI, full native suite, sanitizer, performance or hardware qualification.
