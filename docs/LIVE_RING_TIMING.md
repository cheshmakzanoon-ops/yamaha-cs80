# Live ring-envelope timing

2026-11-07. This change connects current RING ATTACK and RING DECAY controls to the running envelope in Source Performance and PURE CS-80. It is an **engineering-inferred live-control policy**, not a measured PRA circuit correction or a new calibrated envelope law.

## Reproduced behavior

At baseline `185e04c`, the engine updates the ring settings when parameters change but passes attack/decay to `ringAdsr` only when the first note of a phrase starts. A later control edit has no effect until another phrase. For example, at a 48 kHz host, changing attack from 2 s to 0.01 s after 100 ms produces exactly the same envelope as leaving it at 2 s: both read 0.118639 after another 50 ms in either source policy.

The earlier [rank timing change](LIVE_ENVELOPE_TIMING.md) explicitly excluded the ring envelope. Its M41 rank timing-wire evidence is not evidence of the separate PRA ring circuit.

## Evidence and inference

The [ring service constraints](HARDWARE_REFERENCES.md#ring-calibration-unusually-useful-constraints) establish attack/decay controls, a 31 ms attack target at A = +5 V, and an EO-zero target at minimum attack/decay. The [existing ring model](workstreams/ws4-circuits.md#step-2-ring-modulator) leaves the control-voltage taper unresolved and already consumes host timing values in seconds. Those sources do **not** establish exact mid-stage responses to moving a control.

The declared choice here is to apply the current timing coefficient to the retained envelope state, consistent with an analog state that survives control movement and with the project's existing RC-envelope approximation. Alternatives were to keep note-on latching, or to reset/retrigger on edits. Latching ignores live control moves; retriggering discards the current state and changes phrase behavior. Neither is adopted for the source policies. Exact PRA timing topology, taper, transient response and hardware measurements remain open; no service anchor is newly claimed as matched.

## Bounded implementation

- Within `usesSourceModels`, `Engine::applyParams` calls `ringAdsr.setTimes(global.ringAttack, global.ringDecay, 0.35f)`.
- The setter changes timing values only. It preserves stage, precise state, elapsed time, phrase trigger and carrier phase. It does not call note-on, note-off or reset.
- The existing zero sustain, fixed 0.35-second release, host timing clamps, and source-policy RC equations remain unchanged. Rank envelope scaling and tolerance factors are not added to the ring envelope.
- A change during attack can also select the decay time used when attack ends. Release, completed held sustain, and idle do not restart. A new phrase still takes the latest controls.
- Legacy keeps its historical note-on latching. Static source-policy audio is unchanged; intentional audio differences are confined to live ring timing edits in those policies.
- No host parameter, serialization, preset, UI, plugin or reference-circuit implementation changes.

## Verification

The standalone `cs80_live_ring_timing_tests` checks both source policies and legacy, Eco/HQ at 44.1/48/96 kHz, and lifecycle/block partitioning through 192 kHz:

- Faster and slower edits in attack and decay; setter-boundary stage/value/elapsed and unrelated voice/rank-state continuity.
- Independent recurrence checks at the next samples, stage transition and following sample. The oracle does not call production `Adsr` or its coefficient cache; agreement establishes the existing numerical law and new routing, not hardware fidelity.
- A nonzero decay sample after selecting decay during attack, legato and next-phrase behavior, fixed release, zero held sustain, idle, repeated unchanged settings and bit-identical block partitioning.
- Final Release regression: **1,331 checks passed**. The same final test linked against the baseline core fails **170 checks**, rejecting the missing routing.

Five focused Release CTest suites passed: core, ring circuit, live rank timing, controller reset and live ring timing. This is a focused core-only result, not a full repository or host qualification. The final expanded regression also passed GCC `-O3 -march=native -ffp-contract=fast` and ASan+UBSan; leak detection was disabled, so this is not a leak-clean claim. Three targeted mutations are rejected: retriggering on a settings update (194 failed checks), replacing the fixed release with decay (24), and enabling live routing in legacy (24). Both new test/probe sources pass GCC `-Wall -Wextra -Wpedantic` syntax checks.

A separate same-toolchain raw-float probe, [LiveRingCompatibilityProbe.cpp](../tests/diagnostics/LiveRingCompatibilityProbe.cpp), covers all **523 shipped programs** at 44.1/48/96 kHz plus 18 policy/quality ring scenarios including legacy live edits. Baseline and candidate produced **1,587 identical rows / 53,182,464 identical bytes**. Both corpus SHA-256 hashes are `35bebd5c4589dab6932940408e2789c89f6ea0e69f5c037a809b6eed4793dcd6`. This is one GCC 14.2 Linux comparison, not a cross-platform hash promise, exhaustive musical coverage or hardware comparison.

To run the registered regression without the plugin:

```sh
cmake -S . -B build-core -DCS80_BUILD_PLUGIN=OFF -DCS80_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-core --target cs80_live_ring_timing_tests
ctest --test-dir build-core -R '^cs80_live_ring_timing_tests$' --output-on-failure
```

The supplemental probe is compiled from the same source against each core static library and invoked with separate raw-output filenames, which are compared byte-for-byte. No local processor/GUI tests were run. Native-platform CI remains separate from this local evidence.
