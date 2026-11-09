# Sixteen-voice on-screen note and pressure ownership

## Defect

The plugin's optional sixteen-voice mode routes notes to a second eight-voice
instrument when the first is full. The on-screen keyboard's before/after
ownership snapshots previously inspected only the first instrument.

An accepted UI onset in the second instrument was therefore mistaken for a
refused onset. Its UI-held bit was not recorded, so the later UI release could
be discarded, leaving that extension voice held. Its pressure gesture was also
unattributable. In the opposite direction, a host-origin pitch alias in the
second instrument could be invisible to the primary UI pressure safety check.

## Correction and limits

The processor now snapshots all sixteen physical slots, leaving the second
eight empty when the extension is inactive. The existing allocation proof,
release tracking and conservative same-pitch host-alias exclusion use that
same combined snapshot for UI and host events. Token storage is two fixed
sixteen-slot arrays, bounded by the existing 1,024-byte compile-time limit.
No audio allocation, lock, new parameter or state field is added.

No Engine oscillator, envelope, controller law or voice-routing policy changes.
The correction intentionally changes affected on-screen performances in Legacy
and Circuit sixteen-voice mode. PURE CS-80 still uses the original eight voices.
The ownership correction originally left the meter primary-only. The later
[activity telemetry correction](VOICE_ACTIVITY_TELEMETRY.md) covers both
instruments separately; its visual qualification limits remain explicit.

The existing [handoff limits](KEY_PRESSURE_HANDOFF.md) remain: deferred KAS
onsets have conditional release ownership but no immediate pressure proof;
arp pressure is disabled; host pitch aliases suppress UI pressure; simultaneous
UI/host ownership of one MIDI channel/pitch is not independent per-voice
source-targeted note-off support. This is a software correctness correction,
not evidence of analog hardware equivalence or DAW qualification.

## Regression coverage

- `KeyboardPressureAuditTests.cpp` covers all sixteen slots, both-direction
  pitch aliases, extension release/host reuse/reset, and the fixed storage
  bound. All 235 checks pass locally. The extended tests against the untouched
  production helper report 18 failures. AddressSanitizer and UndefinedBehavior-
  Sanitizer pass with leak detection disabled; LeakSanitizer itself cannot run
  under this execution environment's ptrace supervision.
- `SixteenVoiceKeyboardHostTests.h`, included by the existing native pressure
  host suite, drives the actual processor keyboard FIFO. It covers all eight
  extension notes, same-batch short notes, release and late pressure, refused
  seventeenth onset, aliases in both instruments, unchanged host aftertouch,
  overflow recovery and eight/sixteen mode reactivation, in Legacy/Circuit
  with zero- and 32-frame callbacks.
- `CS80Snapshots --keyboard-handoff-tests` and CTest
  `cs80_keyboard_handoff_host_tests` run the processor-only ownership suite
  without opening an editor. The unchanged full `cs80_ui_smoke` also runs it
  alongside native pointer/editor checks. Native execution is recorded
  separately; syntax-only compilation is not a runtime pass.
