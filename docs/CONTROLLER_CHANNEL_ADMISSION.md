# Reject out-of-range controller channels before side effects

`Engine::controlChange`, `pitchBend`, and `channelPressure` now ignore channel
numbers below zero or above 16 before changing any state. Previously the global
expression, ribbon adapter, conventional bend, and matrix channel-pressure
paths could accept these malformed channels even though channel-local paths
would ignore them. A bad channel could therefore mute or retune valid notes.

This is an engine API robustness correction. Ordinary MIDI wire messages encode
channels 1–16, so this primarily protects direct callers and event adapters.
It does not add new channel-specific routing for otherwise global controls.

The existing channel-zero application extension is intentionally preserved,
including channel-mode panic. Valid channels, non-finite-value rejection,
PURE behavior, controller curves, and ordinary controller routing are unchanged.
The explicit poly-pressure broadcast API and note-event admission are outside
this change.

The added controller-reset matrix covers both engine modes, MPE enabled and
disabled, negative and oversized channel numbers including integer extrema,
pitch bend, matrix-routed channel pressure, expression, and a CC74 ribbon
mapping. Each case checks finite bit-identical held-note and subsequent-note
audio against a no-event reference. Existing valid-channel controller tests and
channel-zero channel-mode checks remain in the same executable.

## Verification

The final regression source linked against the preceding unchanged core library
reproduced **112 failures out of 4,331 checks**. The fixed controller-reset target
passed with **4,331 checks and zero failures**. Core, controller-reset, and
tuning-MIDI CTests passed together (3/3, 25.70 seconds). This is targeted engine
verification, not a full-suite, native-host, or performance qualification.
