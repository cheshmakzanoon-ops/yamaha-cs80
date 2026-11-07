# CC88 release-prefix consumption

## Defect and scope

The MIDI Association's [CA-031 summary](https://midi.org/high-resolution-velocity-prefix)
defines CC88 as a prefix to the subsequent **Note On / Note Off** message.
Previously, the processor consumed a channel's prefix only on a positive
Note On. A prefix intended for release velocity therefore survived Note Off
and changed the next unprefixed strike's velocity on that channel.

The processor now discards the prefix on Note Off, before routing the release.
The engine still does not use release velocity. The change does not introduce
release-velocity synthesis, change note allocation, or change the existing
positive Note On arithmetic. JUCE's existing `isNoteOff()` classification also
covers zero-velocity Note On; its classification and the production call site
were reviewed in source, not exercised with a processor instance.

## Verification

`tests/HighResolutionVelocityTests.cpp` is standalone, with no JUCE, processor,
GUI, audio device, or MIDI device. It checks every MIDI channel, all 128 prefix
values and positive 7-bit strike velocities, plus cross-channel isolation,
repeated/invalid releases, reset and prefix replacement.

- Optimized C++20 (`-O2 -Wall -Wextra -Wpedantic`): **780,656 checks pass**.
- UBSan (`-O1 -fsanitize=undefined -fno-sanitize-recover=all`): **780,656 checks pass**.
- CMake Release target and focused CTest: **1/1 pass**.
- Mutation replacing the release clear with a no-op: **258,208 failures**.
- Combined AddressSanitizer/UBSan attempt: **not a pass**. LeakSanitizer exits
  with its runtime limitation under ptrace. No tracing or work-around was used.
- Production processor integration: source-reviewed only; no processor/GUI
  runtime test was run in this audit.

The CMake target is `cs80_high_resolution_velocity_tests`.
