# MTS-ESP client integration

Date: 2026-11-05. Integration baseline: `5d848ce`.

The shipping adapter now builds the pinned ODDSound client rather than silently
disabling MTS-ESP when untracked dependency files are absent. The source,
header and 0BSD licence are byte-exact upstream files; their source pin and
SHA-256 values are in [the vendor README](../src/vendor/mts-esp/README.md).
No master library is installed or distributed by this change.

## Safety and compatibility boundary

The adapter stages all 128 pitches and mapping flags before publishing them.
A non-finite, non-positive or out-of-range mapped frequency rejects the entire
update without changing either caller array. Valid frequencies are bounded to
the float domain, with 16 octaves of upper headroom for downstream multipliers.
An invalid frequency on an unmapped note receives a finite equal-tempered
fallback, because an earlier sounding voice may still use that note's pitch.

A missing master, failed registration or disconnect still observed at the end
of polling preserves the current table. This all-or-nothing caller update is
**not** an atomic snapshot of the external master: the public API supplies no
tuning revision or snapshot mechanism. A rapid disconnect/reconnect or
concurrent master edits may span one polling pass.

The existing processor polls channel `-1` once per block in MTS mode and sends
the result to each active Engine. Equal temperament and PURE CS-80 remain
unaffected. Local MIDI Tuning Standard SysEx continues through the existing
validated `Engine::handleSysEx` path at its sample position. The adapter no
longer forwards it into a second, unused upstream parser. No core tuning parser,
Scala behavior, DSP formula, validation script or tolerance file changes.

## Local verification

GCC 14.2, Linux, Release:

- Four new CTests pass: real vendored-client lifecycle, deterministic master
  fixture, explicit no-client build, and Windows max-macro compile regression.
- The real client completes 32 register/deregister lifecycles with no master
  present. The deterministic fixture passes 98 checks, covering all 128 pitch
  and mapping entries, failed registration, exact table preservation after
  invalid values at first/middle/last notes, finite filtered fallbacks,
  observed disconnects, reconnect updates, positive frequency-boundary
  controls, finite boundary renders in both engines, and default/PURE isolation.
- The existing `cs80_tuning_midi_tests` and `cs80_core_tests` also pass.
- All four new tests pass with AddressSanitizer and UndefinedBehaviorSanitizer,
  using `-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer` and
  `ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1`. This is not a
  leak-check result.
- The same final fixture linked against the baseline adapter implementation
  fails 62 of its 98 checks. For that comparison, the baseline's optional
  client branch was enabled and the upstream declarations force-included;
  its tuning implementation was unchanged. The native availability check also
  fails against the original no-client build.

Reproduce the ordinary focused suite:

```sh
cmake -S . -B build-mts -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCS80_BUILD_PLUGIN=OFF -DCS80_BUILD_TESTS=ON
cmake --build build-mts --parallel 2 --target \
  cs80_mts_esp_native_tests cs80_mts_esp_integration_tests \
  cs80_mts_esp_fallback_tests cs80_mts_esp_windows_macro_tests \
  cs80_tuning_midi_tests cs80_tests
ctest --test-dir build-mts --output-on-failure \
  -R '^(cs80_mts_esp_.*|cs80_tuning_midi_tests|cs80_core_tests)$'
```

## Remaining qualification

This work does not claim a live external-master DAW test, native Windows or
macOS qualification, whole-project aggregate gates,
or measured CS-80 fidelity. A later [channel-master integration](MTS_ESP_CHANNEL_MASTER.md) adds per-channel tables; [scale-name display](MTS_ESP_SCALE_NAME.md) is now available through a bounded message-thread query. Simultaneous live-master and local SysEx precedence needs
host-level qualification; the existing block-start-master/sample-position-SysEx
ordering is unchanged.

## Combined Linux build follow-up

The independently reviewed candidate `ac44b760` was built on the unchanged
`5d848ce` main baseline with `CS80_BUILD_PLUGIN=ON`, using GCC 14.2 in Release.
The complete VST3, Standalone and `CS80Snapshots` targets compile and link.
All six focused/core CTests listed above also pass from this fresh plugin build.
The 98-check fixture and 32 real-client lifecycle checks remain green.

The reused JUCE 8.0.11 source tree was compared with its pinned
`c352e2489000ffeb9e397051377d75eef2efe641` archive: all 4,377 files match, with
none missing or changed. The three MTS vendor files were also rechecked
byte-for-byte against the official upstream commit. The linked Linux VST3
exports no `MTS_` API symbols, avoiding accidental outward exposure of the
embedded client.

No local GUI/socket retry or external validator run was performed. Native UI
runtime validation remains for ordinary CI; a successful compile and focused
suite do not constitute release qualification or a live-master DAW test.
