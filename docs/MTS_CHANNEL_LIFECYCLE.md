# Per-channel MTS lifecycle regression coverage

This qualification-only follow-up adds tests to the per-channel tuning implementation
published in `ace89dfd0eab21531cbff27d145053de223ce67c`. It changes no production code
and does not claim a newly discovered defect.

The matrix exercises scale/octave messages 08/08 and 08/09, real-time and
non-real-time admission, and Legacy and Circuit engines. Two simultaneous notes
at the same pitch on channels 1 and 2 remain distinct while the message addresses
only channel 2. Each combination covers held, sustain-pedal-held, and released
voices, then admits fresh notes on both channels. Assertions check ownership,
active state and pitch. Non-real-time messages additionally preserve exact audio
against an untouched mixed-channel reference. Circuit released key-voltage latching
remains the expected behavior, not a failed retune to bypass.

A separate scanner fixture proves both onsets are still queued when tuning changes.
Both message policies use the latest per-channel table at dispatch and preserve
both source channels. This covers a tuning change between enqueue and dispatch,
not merely a table loaded before enqueue.

## Result and limits

Fresh Release core build; the registered `cs80_tuning_midi_tests` passed:
**23,725 checks, zero failures**, **1/1 CTest in 15.85 seconds**. The preceding
unexpanded fixture passed 23,697 checks; the final result includes explicit
channel-ownership assertions suggested during independent review.

This is focused core regression coverage, not a new full-suite run, native host,
live MIDI/MTS-ESP, sanitizer, hardware-fidelity or performance qualification.
It does not exercise every sustain policy, MPE zone, ARP pattern or sample rate.
The separate [full core integration checkpoint](CORE_INTEGRATION_ACE89DFD.md)
continues to identify its own exact tested source and scope.
