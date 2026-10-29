# UI keyboard → audio handoff

This boundary replaces the former audio-thread call to
`MidiKeyboardState::processNextMidiBuffer`. It does not change Engine source
ownership, synthesis, presets, or host parameter IDs.

## Thread ownership

* `getKeyboardState()` is **non-audio only**. The stock keyboard component may
  call it from pointer/computer-key handling; other non-audio producers are
  supported. Its listener copies note events into `KeyboardEventQueue<256>`.
* Producers are serialized by a producer-only mutex. A fixed ring uses release /
  acquire publication; only the single audio consumer advances its read index.
  The consumer takes a bounded snapshot, never waits for this mutex, and never
  changes producer indices. Concurrent publication after a snapshot waits for
  the next block. No event is allocated by the ring.
* `serviceKeyboardUI()` runs on the message thread at editor construction and
  each editor tick. It acknowledges requested resets and discards the stock
  state's indirect MIDI buffer using an empty scratch buffer with injection
  disabled. That stock state may lock/allocate on the UI side. Any external
  non-audio producer using the public keyboard state must also arrange this
  message-thread servicing; it must never call it from audio.
* Audio never reads, writes, or locks the stock keyboard state. Host MIDI is
  const-iterated without injection, clearing, or growth. Unsupported system,
  long, malformed, and program-change messages are skipped before constructing
  an owning `MidiMessage`. Supported channel messages retain buffer order.

## Timing and pressure

UI events preserve producer order. Their millisecond timestamps are stretched
across the current block using JUCE 8.0.6's indirect-event mapping: first event
at zero; remaining offsets are rounded from `(event-first) * frames /
(last-first+1)` and clamped to the last frame. Unsigned differences explicitly
handle a 32-bit millisecond wrap within a batch. Producer timestamps sampled
before mutex acquisition are normalized into publication order: backward deltas
(or ambiguous jumps exceeding 2^31 milliseconds) clamp to the previous timestamp.
The mapping clamps in floating point before converting to a sample integer.
A zero-frame block dispatches
at zero without attempting to render or clamping to -1. Host events win every
same-sample tie, as they did when JUCE appended indirect events to its buffer.
The original 7-bit MIDI velocity quantization and zero-velocity note-off
interpretation are retained in the non-audio listener. The UI pressure mailbox
is consumed after **both** host and UI note-ons.

## Lifecycle and recovery

`prepareToPlay` / `releaseResources` require host processing to be stopped.
They request a new epoch and clear audio-owned held-note tracking and host
visualization, without touching the UI keyboard state. UI input is rejected
until the message thread resets its local state and acknowledges that exact
epoch. Stale acknowledgments cannot reopen input. A physical gesture arriving during
reset is deliberately dropped; if it lands between the UI clear and acknowledgment,
its local key can remain visually held without sounding until released and pressed
again. This is fail-closed behavior, not delayed replay. Events published late from
an old epoch are discarded, not reinterpreted as new notes. Ordinary factory,
user-preset and session restores retain the existing held-note policy; they do
not request a keyboard lifecycle reset.

If the ring fills, input fails closed: overflow remains latched even if space
subsequently appears. The next callback releases only channel/note pairs
tracked as accepted UI onsets, discards old queued input, requests a fresh
epoch, and waits for UI reset/ack. It does **not** send global channel panic or
clear unrelated host visualization. Host note-off, CC120, and CC123 clear the
corresponding UI ownership bit; an isolation-mode transition also clears UI
ownership because it resets Engine voices. A delayed old UI release cannot turn off a
later host note after that host release/panic.

**This is not independent source ownership.** The Engine currently identifies
notes by MIDI channel/pitch, not UI/host source token. A simultaneous host and
UI note on the same channel and pitch can therefore collide; UI overflow
recovery can release that colliding host note. This change makes no lossless
recovery claim. An onset rejected by the voice allocator can still be tracked
as a UI-held key, as stock gesture state is not a report of sounding voices.

## Display semantics

Host note-on/off and channel panic update atomic per-note channel masks.
Custom key painting combines these with the stock component's **local physical
held gesture**. Host MIDI is never reflected into the producer keyboard state,
so display updates cannot feed new notes back into the audio queue. A local
physical gesture can remain visually down after host panic until its pointer
or computer key is released, even though its sound was stopped. The display is
not a promise that a voice is sounding (sustain tails and voice rejection also
separate these concepts).

## Evidence and limits

`KeyboardEventQueueTests` covers ordered on/off events, capacity/overflow latch,
reset acknowledgments, discarded epochs, invalid inputs, four serialized
concurrent producers, timestamp wrap, and zero/one-frame mapping. Host/UI
integration tests live in the existing `ui_snapshot` smoke executable and are
run by the JUCE CI build. Pure queue tests can run without JUCE.

The implementation removes the identified shared keyboard lock and indirect
host-buffer growth from the callback. Source inspection and focused tests are
not proof of whole-plugin hard-real-time behavior, TSAN cleanliness, or complete
callback allocation freedom; no such claim is made.

Reference: JUCE 8.0.6
[`juce_MidiKeyboardState.cpp`](https://github.com/juce-framework/JUCE/blob/8.0.6/modules/juce_audio_basics/midi/juce_MidiKeyboardState.cpp),
particularly `processNextMidiBuffer`, the indirect timestamp mapping, and its
unconditional `eventsToAdd.clear()` when injection is disabled.

### Focused mutation check

The pure suite was checked against temporary header copies, with each executable
bounded to 10 seconds and undefined/float-cast-overflow sanitizers enabled. The
unchanged baseline passed. Seven individual mutations failed as expected:
accepting input before acknowledgment; clearing overflow during ordinary drain;
reading the next FIFO slot; omitting timestamp normalization; casting the mapped
sample position before clamping; allowing a late stale acknowledgment to overwrite
the current acknowledgment; and removing the zero-frame guard.

Removing the stale-event epoch filter was **not detected by the ordinary suite**.
The reproducible `tests/test_keyboard_epoch_interleaving.py` fixture generates
temporary instrumented copies of the current header. It pauses a producer after old-epoch
capture but before publication, requested reset on the consumer, then resumed
the producer. That controlled interleaving passed for the baseline and failed
with the epoch filter removed. The pause hook existed only in temporary test
copies, not production code. Run it with
`python3 tests/test_keyboard_epoch_interleaving.py --compiler /path/to/c++`
using GNU/Clang. It asserts unique instrumentation markers, baseline success and
the specific mutant failure, with 60-second compile and 10-second execution
limits; temporary files are cleaned automatically. CTest registration is limited
to non-MSVC builds; Windows retains the native queue suite. No compiler options
are read from environment variables and no subprocess uses a shell.

The distinct fast-UI-ack/discard-boundary race was
resolved and independently reviewed by source inspection, not deterministically
reproduced by this fixture. These checks do not establish exhaustive concurrency
coverage or TSAN cleanliness.

## Verified executable checkpoint

Commit `9b860b1ca6c5cf554261ccc23f7db5ffe4f8771f` passed [all platform builds, host smoke tests and strict validators](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/40953422398), [DSP, sanitizer and raw baseline compatibility](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/40953422266), and [screenshots](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/40953422502). GNU/Clang CTest includes the controlled epoch-interleaving script; MSVC retains the native queue and host tests. This verifies the stated software contract, not a whole-plugin hard-realtime guarantee or hardware equivalence. Historical validator failures remain recorded separately.
