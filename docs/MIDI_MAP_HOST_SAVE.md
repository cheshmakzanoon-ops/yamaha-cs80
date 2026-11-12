# MIDI-learn map host-save notifications

MIDI-learn assignments are saved in `MIDI_LEARN`, outside the host parameter tree. Previously, assigning or forgetting a mapping did not mark that non-parameter state dirty. Learning can also leave a parameter's value unchanged, so an ordinary parameter notification is not a reliable substitute.

`MidiLearnMap` now has an optional immutable callback/context. A complete persistent mutation invokes it once; the processor callback only sets the existing atomic deferred notification flag. The existing message-thread timer coalesces edits and sends `nonParameterStateChanged`. Audio-thread learning does not call the host directly.

Actual assignment, move, displacement, controller removal, parameter removal, and clear-all notify. Identical assignments, empty removals, invalid assignments, arm/cancel, and clearing an armed-but-empty map do not. Assign still returns whether an assignment is valid, not whether it changed. Negative parameter removal returns zero. Accepted host restore uses explicitly silent clear/assign operations and retains the existing pending-notification discard behavior.

No state schema, parameter IDs, synthesis equations, or UI/fader implementation changed. Default-constructed maps remain callback-free. The callback context must outlive the map, and callbacks must be non-blocking and allocation-free because learning can invoke them on the audio thread.

## Verification

- Before the fix: all four focused processor cases failed (assign, forget controller, forget parameter, clear all).
- Standalone map suite: 55 checks, including callback-time final map visibility, no-op behavior, mapping moves, suppression and audio-style learning.
- Processor suite: 74 checks, including worker-thread CC learning with an unchanged parameter value, message-thread-only deferred notification, coalescing, reentrant state saves/edits, accepted restore silence and PURE blocking new learning.
- Native DAW UI/session-save behavior is not claimed; tests exercise the processor listener contract with normal valid inputs.
