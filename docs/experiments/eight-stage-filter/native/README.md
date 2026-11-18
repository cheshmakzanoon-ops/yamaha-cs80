# Untimed native factory-eight held gate

**PASS for this fixed native history; hybrid prototypes only.** Two cold authenticated stock VST3 processes, one normal V2 process and one separate k8-observer V2 process produced exactly the same eleven ordinary files: **933,293 bytes per process**, including every stereo PCM sample, every caller/next-operation FP observation, all declared controls, native metadata, note-input receipts and five public serialized states.

Every audio callback entered the genuine `IAudioProcessor::process`. This is neither an Engine runner nor a processor cast. No cost clocks, CPU estimate, native speedup, clean-product admission or DAW/concurrency qualification is established.

## Workload and public observations

- 48,000 Hz, maximum block size 64, 64 frames/callback, realtime sample32; inactive mono external input and active stereo output.
- 1,024 warm callbacks, **16 post-warm callbacks**, one continuation: 1,041 callbacks and 66,624 finite, nonzero stereo frames per process. The named window is not a timing sample.
- Factory selectors **0/0**, Source Performance 1, Pure 0, voice mode 0; drift .37, dispersion .18, drift model 0; Standard oversampling parameter 1, volume .12, mix .5; dry, MPE/ARP/noise/modulation depths disabled.
- Exactly eight Note Ons, all at block 0/offset 0 on SDK channel 0: **36,43,48,55,60,64,67,72**, unique note IDs 0–7. Event velocity is float `102/127`, decoded MIDI7 102. The SDK event-list boundary recorded eight reads and no subsequent events. No releases, transitions or configuration edits occur.
- All 192 ordinary controls/selectors are declared once. No parameter automation is resent in positive callbacks; the real plugin reapplies its control frame before MIDI. Native metadata contains 2,378 records, with all 192 required IDs present. Factory selectors have step count 13 and normalized value 0.
- Factory selection makes the real processor replace its 26 programmer controls per rank each callback. The host explicitly retains live feet/pan/HPF/LPF controls. Input controls, actual metadata and serialized states are preserved, not inferred from the program name.
- The public parameters' 23 float projection differences are recorded. Together with native MIDI quantization, these mean this fixture is **not claimed to have bit-identical direct-core inputs**.

`callbacks.bin` has all 1,041 public callback records, not private Engine states. Public serialized snapshots are initial, block 0, 1023, 1039 and 1040. Each 72-byte little-endian callback record is `<IIdiQ` (block, public mode, rate, return, silence mask), three `<IHHi` FP records (MXCSR, x87 control/status, errno: before/after/next probe), and one double probe result. PCM is interleaved little-endian float stereo. The FP environment and errno are restored after host observations.

## Narrow, separate attribution

Only the diagnostic module calls the integer observer. It counts actual returned eight-stage kernel calls, successful returns, and subsequent cached-quartet consumptions. It never dereferences the stage pointers.

| Phase | Callbacks | Returned k8 calls | Accepted | Cached quartet consumed |
|---|---:|---:|---:|---:|
| Warm | 1,024 | 281,894 | 276,208 | 276,208 |
| Post-warm window | 16 | 8,207 | 8,090 | 8,090 |
| Continuation | 1 | 512 | 502 | 502 |
| Total | 1,041 | 290,613 | 284,800 | 284,800 |

The normal module has no observer hooks. Counted and normal PCM/FP/public-state bytes are exact. These are not private active-voice or source-route observations from normal VST3. Broader arithmetic, fallback, FP/trap and Engine gates have distinct scopes outside this folder.

## Build identity and limitations

The normal native prototype uses the frozen V2 core archive `ae34d6a4899a03fdbddf204520ea01644b77f379a38e611b84bdaedfcd689005`, authenticated original shared-code/VST3 wrapper objects, and the original native link recipe. One original core TU (`Engine.cpp`) changed, with two new TUs (`EngineFilterEight.cpp`, `Ig00156Eight.cpp`). Header dependency closure is **21 original TUs plus two new TUs**. Twenty header-dependent originals remain unchanged in these modules. No fields/layout changed, but this is still a **genuine loaded hybrid release-module prototype, not a full clean candidate product build**. Original unchanged bundle metadata was copied.

`host.cpp` is the exact ordinary host. Apply `scaffold.patch` to the pinned repository `tests/Vst3AutomationTests.cpp`, writing `sdk_scaffold.cpp` beside it; the only scaffold change is maximum block size 4096→64. The host uses the official local JUCE 8.0.11 VST3 SDK IID/support sources and `-ldl`, not a linked renderer. `observer.patch` contains every reporting change, including the exact counter source and host delta. Source, object, archive, module and receipt hashes are in `evidence.json`.

## Offline audit

Run `python3 verify.py` and `python3 test_verify.py`. They read only this folder, validate the complete saved workload/public state and FP records, recompute all attribution phases, and check every arm's original per-file digests. Eight tests include synthetic corruption of PCM, callback/FP records, events, counters, aggregates and arm identities.

`captures.tar.gz` stores one lossless copy of the eleven identical captures plus the full observer log. Duplicate copies are omitted, while all four original digest rosters remain. No retained PCM/FP/state bytes are redacted. Private host paths, runtime identifiers and all native module/build binaries are excluded. A hash manifest detects subsequent edits; it is not cryptographic proof that execution occurred.
