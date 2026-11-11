# Full core integration after per-channel MTS tuning

## Exact source and configuration

The tested checkout was the reviewed local commit
`d5ffae4c46c7652ea825d9a2d7576f8406020442`. Its complete tree
`bce67b3e747a5dcb73c7d94597bfa284612c23b9` is identical to verified published
commit `ace89dfd0eab21531cbff27d145053de223ce67c`. This distinguishes the actual
checkout used by the run from the equivalent published commit; no rebuild on a
different checkout is implied.

A new build directory was configured with Ninja, Release (`-O3 -DNDEBUG`),
`CS80_BUILD_PLUGIN=OFF`, and `CS80_BUILD_TESTS=ON`. Default timeout scale1 was
retained. No additional compiler/linker flags, relaxed tolerances, exclusions or
rerun selection were applied. Tracked source remained unchanged during compilation and testing.

## Completed result

- Fresh build: **259/259 steps completed**.
- All registered core CTests: **145/145 passed,zero failures**.
- One uninterrupted serial run: **893.08 seconds**.
- Exactly145 Passed records; no skipped or not-run tests.

The sweep includes the expanded channel-aware MTS regression together with the
existing Engine, oscillator, filter, modulation, release/scanner, tuning, memory,
library and tooling checks. It closes the broad core integration gap left by the
focused four-suite MTS validation, including recompilation against the changed
Engine table layout. No runtime code changes were needed after the sweep.

## Reproduction

From the pinned source (or the verified identical tree), use a fresh directory:

```sh
cmake -S . -B build-core-check -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCS80_BUILD_PLUGIN=OFF -DCS80_BUILD_TESTS=ON
cmake --build build-core-check -j2
ctest --test-dir build-core-check --output-on-failure -j1
```

## Qualification limits

This is a local core-only Release test result. It does not build or qualify native
plugin wrappers, external DAWs, installers/signing, display-server behavior,
physical MIDI devices, hardware fidelity, sanitizers or leak detection. The
separate [host MTS transition fixture](MTS_DORMANT_ENGINE_HOST.md) records its own
six-suite native scope; the core sweep does not expand that scope or turn the
source-reviewed live MTS-ESP branch into a runtime-qualified integration.

A test-suite wall time is not an audio throughput score. CPU/idle targets and
structural performance work remain open. Existing bank/program and scale-reference
limits in [per-channel MTS tuning](MTS_CHANNEL_TABLES.md) remain unchanged.
