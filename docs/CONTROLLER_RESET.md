# CC121 sustain and parameter-selection reset

## Defect and scope

The engine previously ignored CC121 (Reset All Controllers). A MIDI pedal
could therefore keep released notes sounding after a reset, and later Data
Entry messages could reuse an RPN selection made before that reset.

This change implements only the sustain and parameter-selection portions:

- A finite CC121 on channels 1 through 16 releases that channel's MIDI
  sustain pedal. Its data value is immaterial.
- Pedal release uses the existing CC64 path, including continuous envelope
  release, Circuit Sustain II policy, ring gate bookkeeping and waiting-key
  reconsideration. Held keys are not released.
- MPE members still inherit their master's pedal, and independent member
  pedals remain active when their master is reset. Other zones and ordinary
  channels are unaffected.
- The addressed channel's RPN/NRPN selector and Data Entry latch are cleared,
  including when MPE is temporarily disabled. Negotiated zone membership and
  bend ranges are retained. Other channels' selections are unchanged.
- Invalid channels and non-finite float events are ignored. PURE CS-80 keeps
  its existing MIDI-pedal isolation.

This is **partial CC121 support**, not complete Reset All Controllers
conformance. Mod wheel, expression, pitch bend, pressure, timbre, ribbon,
high-resolution velocity and other controller policies are unchanged. No host
parameters, saved-state format, note ownership or voice allocation policy are
changed. This MIDI/software behavior is not a physical CS-80 fidelity claim.

## Regression coverage

`ControllerResetTests.cpp` compares CC121 against ordinary pedal-up at 44.1,
48 and 96 kHz in Legacy and Circuit modes, across both panel sustain modes,
both footswitch states, note-off and CC123. It checks both rank envelopes,
exact audio across different block sizes, repeated reset, later notes,
all 16 addressed channels, overlapping MPE pedals, waiting KAS keys, malformed
events, RPN 0/6, NRPN interruption, per-channel selectors, MPE off/on,
independent ribbon mapping and PURE isolation.

The focused CMake target is `cs80_controller_reset_tests`; it is JUCE-free.
All 1,477 checks pass with this change; the same tests produce 566 failures
against the unchanged production sources from `f7d1b755`.
Core, source-performance, smoothing, MPE-master-smoothing, pedal-release,
voice-release, tuning/MIDI, MPE-master-routing and parity suites also cover
the unchanged surrounding behavior. These tests do not qualify a DAW, plugin
wrapper, validator, or a complete Reset All Controllers implementation.
