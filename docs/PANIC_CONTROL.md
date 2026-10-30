# PANIC: stop stuck notes without changing the patch

The keyboard-focusable **PANIC** button in the software toolbar requests a
global hard stop at the beginning of the next audio callback. It clears existing
voices, held arpeggiator keys and MIDI sustain-pedal latches on all 16 channels.
Queued old on-screen-key events and per-key pressure are discarded; the normal
message-thread keyboard refresh clears the display and acknowledges the new
input epoch before on-screen keys are accepted again.

The patch name, every parameter, programmer memories, physical footswitch
settings, expression position, pitch/mod controls and hardware selectors remain
unchanged. Incoming host notes in the same block are processed **after** the
panic and can sound; PANIC is not a latch that rejects future MIDI. If the host
has suspended processing, the request waits for the next callback. A zero-frame
callback can consume it, and repeated pending clicks coalesce.

**PANIC is not output mute.** Existing effect/filter/output histories and analog
noise are retained, following the engine's existing application-level hard-stop
policy. Effect tails or noise may still be audible. The output indicator, if
present, continues to observe actual output and is not cleared by PANIC. Incoming
MIDI CC120/CC123 retain their existing channel-specific semantics; this button
does not redefine those messages.

## Thread and test contract

The UI writes one statically asserted lock-free atomic flag. The audio thread
consumes it before keyboard-queue draining, calls the existing
`Engine::allNotesOff(false, 0)`, and performs the existing consumer-side keyboard
epoch reset. No UI code mutates the engine, and no new audio-thread allocation,
lock, parameter or serialized-state field is introduced.

`tests/PluginPanicTests.h`, integrated in the existing UI/host smoke harness,
checks all three rendering policies; all-channel voices/sustain; queued,
overflowed and stale UI input; fresh host notes after the boundary; pending
requests before prepare and zero-frame callbacks; repeated requests; arpeggiator
cleanup; retained FX/noise against the existing hard-stop primitive; and actual
button discoverability, layout, help and handoff. Sequential engine inspection
in those tests is an offline oracle, not a UI-thread production access pattern.
Native runtime validation is required for the exact commit; syntax checks alone
are not a host/UI pass or whole-plugin realtime certification.
