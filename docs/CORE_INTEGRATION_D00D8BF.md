# Core integration checkpoint: d00d8bf

Tested source: `d00d8bf039a8fb658784dabb6a7271d8c8c535e0`, November 10, 2026.
This follows the recent MTS, ARP, MPE panic, controller-channel, and note-event
admission changes. Documentation changes recording this run do not alter the
tested runtime or tests.

A fresh Ninja Release build with `CS80_BUILD_PLUGIN=OFF` and
`CS80_BUILD_TESTS=ON` completed all **259 build steps**. One uninterrupted CTest
invocation then ran all **145 registered tests serially**:

- **145/145 passed, zero failures**, 884.83 seconds total elapsed time.
- No registered test was skipped or replaced by a later retry.
- Core synthesis, scanner ordering/overflow, controller state, tuning/MIDI,
  MPE, release ownership, modern modulation/FX, library, offline experiments,
  and packaging preflights passed in the same invocation.

Reproduction from the pinned source with CMake, Ninja and the documented build
dependencies installed:

```sh
cmake -S . -B build-core -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCS80_BUILD_PLUGIN=OFF -DCS80_BUILD_TESTS=ON
cmake --build build-core -j2
ctest --test-dir build-core --output-on-failure
```

This is local core integration evidence, not native plugin/DAW validation,
sanitizer/leak qualification, installer/signing readiness, hardware equivalence,
or proof that the project's realtime CPU target is met. CTest durations are
not audio-performance measurements. The earlier [215b900 checkpoint](CORE_INTEGRATION_2026_11_10.md)
remains dated evidence for that source.
