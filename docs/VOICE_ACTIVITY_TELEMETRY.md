# Eight- and sixteen-voice activity telemetry

## Scope

Implementation `6e279c2`, prepared against main `3527987` on 2026-11-09.
This is read-only editor telemetry, not voice allocation, audio synthesis,
pressure ownership, a new parameter, or a saved-state change.

Previously the processor published only its first instrument's eight activity
bits/count. The toolbar and monitor also stopped at eight, so an extension-only
performance could look silent. The old separate count/mask could describe
adjacent callbacks, and prepare/release did not clear old voice telemetry.

`VoiceActivity.h` publishes one lock-free 32-bit value: sixteen physical-slot
bits plus an applied-extension flag. One UI read yields both activity and the
8/16 denominator. Slots 1–8 are primary; 9–16 are extension. A disabled extension
cannot leak stale bits, and PURE CS-80 reports eight even if sixteen remains the
stored request. Prepare/release clear the reading; panic publishes the resulting
post-callback state. Zero-frame callbacks publish accepted events too.

This is the latest completed callback, not a sample-accurate event stream. Very
short notes between UI polls can be missed. “Active” includes release tails and
does not guarantee audible energy: a muted active voice is still active. Counts
are physical voice slots, not necessarily the number of keys held.

## Display

The original eight-voice lamp geometry is retained. Sixteen-voice mode adds a
lower extension row in the toolbar and voice monitor; the accessible count and
monitor denominator become 16. Capacity follows what audio has applied, not an
unacknowledged parameter request. Each widget uses a coherent reading; separate
widgets may poll adjacent callbacks.

A related accessibility correction ensures a toolbar paint before its timer
still notifies the count label. Previously paint could consume the new snapshot
silently, leaving the label waiting for another change. The paint path updates
without recursively invalidating itself.

The monitor description now distinguishes its settings-based envelope/filter
illustrations from its real voice-activity telemetry. No new DSP taps or
circuit-response accuracy are claimed.

## Cost and thread ownership

Audio performs at most sixteen slot reads and one atomic store per completed
callback. Preparation/release clear the same atomic with audio stopped under
the existing host lifecycle contract. The UI never reads mutable Engine state
or `extensionActive` directly. No allocation, lock, retry loop, additional DSP
transcendental, or per-sample work is introduced. Lock-free 32-bit atomics are a
compile-time requirement. This is source-level boundedness, not a measured
whole-instrument CPU budget or cross-platform qualification.

## Verification

Linux, GCC 14.2, official JUCE 8.0.11, exact implementation above:

- `cs80_voice_activity_tests`: 131,332 checks, including all 65,536 sixteen-slot
  masks, eight-slot masking, bounds, lifecycle reset, silent capacity and
  concurrent publication. Release CTest and ASan/UBSan passed (LeakSanitizer
  disabled for this sandbox). An independent reviewer reran UBSan with explicit
  float-cast-overflow instrumentation and library assertions successfully.
- A temporary-header negative control discarded extension bits. The same core
  test failed 130,561 checks. No repository header was modified for that run.
- Native `CS80Snapshots` compiled and linked all processor/editor sources.
  `cs80_voice_activity_host_tests` passed its 48 assertions: Legacy/Circuit,
  zero/32-frame callbacks, sixteen active slots, extension-only activity,
  release tails, panic, eight-mode/PURE transitions, re-prepare and release.
  The existing `cs80_keyboard_handoff_host_tests` also passed unchanged.
- `cs80_voice_activity_ui_tests` is registered separately and included in the
  full UI smoke suite. It checks full/minimum sizes, accessible counts,
  paint-before-timer, actual lower-row pixels rather than changed text, and
  parameter-state preservation. Its source compiles, but **local UI execution
  did not run**: the virtual-display wrapper failed at `xlsatoms: unable to open
  display`, before invoking the plugin. This sandbox separately rejects
  `socket(AF_UNIX, SOCK_STREAM)` with `EPERM`. No visual pass, updated screenshot
  provenance, native-peer pass, DAW pass or release qualification is claimed.

Core-only reproduction:

```
cmake -S . -B build-core -DCS80_BUILD_PLUGIN=OFF -DCS80_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-core --target cs80_voice_activity_tests
ctest --test-dir build-core -R '^cs80_voice_activity_tests$' --output-on-failure
```

With the native JUCE dependencies and a usable desktop/virtual display:

```
cmake --build build --target CS80Snapshots cs80_voice_activity_tests
ctest --test-dir build -R '^(cs80_voice_activity_tests|cs80_voice_activity_host_tests|cs80_voice_activity_ui_tests|cs80_keyboard_handoff_host_tests)$' --output-on-failure
```

The processor-only `CS80Snapshots --voice-activity-tests` entry does not create
an editor/native peer. It is not a substitute for the separate UI gate.
