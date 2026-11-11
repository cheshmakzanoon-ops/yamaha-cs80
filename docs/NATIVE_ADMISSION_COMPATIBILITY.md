# Native compatibility of processor admission fixes

## Inputs and strict gate

Independently built Linux VST3 modules were compared at these exact source commits:

- Baseline: `d5ffae4c46c7652ea825d9a2d7576f8406020442` (published tree equivalent `ace89dfd0eab21531cbff27d145053de223ce67c`).
- Candidate: `39a94c4812a8b94912f761f0c2a622a9b3bc58ec`, including physical-value, patch-recall, Scala-session and UI-mailbox admission fixes.

Both tracked checkouts were clean during the run. The baseline used a separate
fresh build (60 build steps); the candidate module was rebuilt with its changes.
Both were Release, JUCE8.0.11, AVX2 filter batching OFF. The same compiled
`cs80_vst3_main_compatibility_capture` executable processed both actual modules.
The existing comparison rejects a self-comparison and incomplete captures.

The existing `main-vst3-compatibility.json` numeric contract and selections were
retained: 336 cases, 4096 float audio samples per case, zero permitted differing
bytes. A separate local contract copy changed only `baseline_commit` to identify
the actual baseline above; the historical repository contract was not overwritten.

## Result

Both complete captures contain **336 cases**, **15,105,216 bytes**, and **2,378
ordered normalized controller parameters per case**. They are byte-identical:

`ab4665270d2447ab30f7cecf8a7917f7b98f64f4e448ca70233339b01de8dbce`

The capture parser also checks finite audio, valid normalized parameters, unique
parameter IDs, complete case identities and no trailing data. Both captures exited0.

Tested module SHA-256:

- Baseline: `e0227d1bf7806cc10fc1178f4fbc3e9fa0d44ee8d10bcc074f87e2056644e420`
- Candidate: `3373cdf366458bc17f4e2d3da00370543949fdadb3b16c3cd24ff99e86cc7400`
- Shared capture executable: `9ec059a97d7497a93f44f4fa1b93bfa501b39f0636df85634461b4530d10f801`

These are artifact identities, not reproducible-build/signing claims.

## What this establishes

The selected existing native workflows preserve complete captured audio and
ordered normalized parameter state across the admission fixes. The matrix covers
Modern/Legacy/PURE policies, 8/16 voices, block sizes64/256/1024, no-queue and
selected automation queues, and program-recall combinations. State is observed
through native component/controller transfer, not inferred from APVTS XML.

The run is 48kHz, float32, realtime, 2048 stereo frames per case. It does not capture
all serialized metadata or every valid workflow. Modern supported-key trajectories
are explicitly excluded by the existing contract. Malformed-input behavior is
intentionally different and covered by separate regressions. No GUI, external DAW,
AU, live-service, sanitizer, CPU-budget or full native qualification is implied.
