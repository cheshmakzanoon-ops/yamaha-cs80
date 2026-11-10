# Independent unchanged-path compatibility capture

This external witness compares the reviewed baseline `2ade213bc4dec34b04352063a72e97dac4cc5667` with the frozen production core change `c0219bd0de8559d72fce4194a849414de5ac7e61`. No production, test-suite, or CMake files were edited by this witness.

## Scope and test design

`CompatibilityCapture-v2.cpp` is the frozen final fixture. It does **not** reuse the old `mpe-member-onset/CompatibilityCapture.cpp`, whose ordinary member-expression events after accepted release are now intentionally changed behavior.

The complete cross-product is 10 event families × 3 policies (legacy, source performance, PURE/isolation) × 3 host rates (44.1, 48, 96 kHz) × 3 quality selections (Eco, Standard, HQ): **270 scenarios per run**. Each profile/revision is run twice: a whole segment in one render call, and repeated irregular partitions of 1, 7, 31, 3, 64, 2, 127 frames. Stage boundaries and total sample counts are identical. Each render writes fresh NaN-initialized output and checks finiteness, and each sounding segment must contain audio above 1e-8. Zero-frame calls must preserve output sentinels. All samples are serialized, not summarized by RMS/tolerance.

Every family begins with fresh note-on without expression history, active-voice/held assertions, three held member-expression fields, a second held expression value, then accepted release. Event families:

1. **split-held-fresh**: split lower and upper zones, member channels 2 and 15; no later ordinary member expression. Polyphonic pressure remains unchanged and is included during release.
2. **lower-full-endpoint**: lower-only full zone, channels 2 and 16, held expression and release continuation.
3. **upper-full-endpoint**: upper-only full zone, channels 1 and 15, held expression and release continuation.
4. **conventional-tails**: MPE disabled, ordinary expression on released and pedal-held notes, then pedal-up.
5. **manager-gap-tails**: split-zone release tails plus a gap-channel-8 released note; ordinary events on both managers and the gap, with routing assertions.
6. **explicit-cc121**: prior nonzero bend and pressure on released and pedal-held members, explicit CC121 resets, pedal ownership release, and retained CC74 modulation assertions. PURE correctly has no reset assertions because its CC121 sound-control behavior is inert.
7. **mpe-mode-transition**: disable MPE while tails exist, send conventional expression, re-enable, then fresh onset on a separate member channel without post-release ordinary member events.
8. **member-to-gap**: change current lower membership into a gap before ordinary expression updates the old released voice.
9. **member-to-manager**: upper full-zone channel-1 endpoint becomes lower manager before ordinary expression updates that released voice.
10. **lifecycle**: reset, fresh onset, offline mode, fresh onset, realtime restoration and reprepare, fresh onset, and continuation.

PURE is a complete policy row across all families. Zone negotiation and modern expression are inert there; assertions follow effective rather than requested mode. The core's historical direct channel-pressure behavior remains represented. Cases do not assume every MIDI event is inert under PURE.

Fixed deterministic controls disable analog noise, dispersion, drift, detune, chorus and initial bend; release time is long enough to keep tested tails active. Pulse and saw paths remain audible. Timbre-to-pulse-width and per-voice-pressure-to-LPF routes ensure relevant controller data reaches meaningful exposed destinations. Direct routing checks verify expected held/released/pedal allocation, held member bend, fresh neutral controls, pressure recipients, nonzero reset preconditions, and explicit reset outcomes.

## Serialized observations

Each scalar is written explicitly as a fixed eight-byte little-endian field. Float bit patterns occupy the low 32 bits; doubles use all 64 bits. Integer, bool and enum fields are explicit, not object representations. Every floating value must be finite. No struct padding or pointer is written.

- Complete raw interleaved stereo host output at every rendered sample.
- Every exposed `VoiceInfo` member on all eight voice slots.
- Every `RankInfo` and `ModernRankOffsets` member for both ranks of all slots.
- Every `RankAudioState` member: oscillator phase, legacy/SVF filter memories, and all four nodal histories of each HP/LP circuit stage.
- Every member of the complete exposed reconstructed oscillator `State`: charge, reset countdown, coupling baselines, held controls, elapsed time, enabled gates, all three residual arrays, cursor and reset count.
- Every exposed `RankEnvelopeState` field, including both stage enums, values and elapsed times.
- Exposed global modern offsets, per-voice MSEG values and MSEG display positions, engine rates/factor/latency, active count, policy/mode, free-circuit flags, MPE zone/range information, ring/sub diagnostics, listening samples, all exposed PRA wah capacitor state and output-tap state/nodes.
- Stage snapshots immediately following relevant events and after non-silent continuation segments. Autonomous continuation also tests consequences of state not exposed directly.

## Limits and exclusions

This is a **bounded compatibility witness**, not exhaustive proof, complete private-engine-state equality, complete MIDI/MPE conformance, hardware validation, or an audio-quality benchmark. In particular, the public envelope state is a float proxy; private precise envelope values, smoothing internals, allocation ages, scanner queues, ARP state, VCA state, and every other inaccessible private field are not directly captured. Complete *exposed* oscillator state and the declared public voice/rank/filter/envelope fields are captured; that does not imply every private engine variable is captured.

Ordinary Pitch Bend, Channel Pressure and CC74 **after accepted release while the recipient channel is currently an effective MPE member** are excluded from equality claims, including pedal-held tails. Those are the repair itself and belong in functional tests, not unchanged-path comparison. Ordinary post-release events in this fixture are only conventional/PURE, current gap, or current manager events. No claim is made for changed member-cache/future-onset interactions, ARP/KAS release timing, multi-engine/native plugin routing, full 16-voice scheduling, malformed event enumeration, every parameter combination, or enabled AVX2 filter batching. The four archived profiles all define `CS80_DISABLE_AVX2_FILTER_BATCH=1`; native means `-march=native` compilation, not enabled batch-kernel coverage.

The matrix covers all requested quality selections, not every possible host-rate/factor combination. No timed benchmark or performance threshold is evaluated; driver wall time is recorded only as provenance.

## Build and artifact provenance

Each revision compiles the same witness against its own header tree and its own existing static core archive, using the compiler, definitions, optimization, architecture, language and PIC flags extracted from that archive's `Engine.cpp` compile command. There is no baseline object/candidate header mixing. Four matched compiler profiles are GCC generic, GCC native, Clang generic and Clang native. Comparisons are **within matching profiles only**, not cross-compiler/architecture.

Baseline archives were built from `sixteen-note-release-impl`. Both it and the baseline header checkout `sixteen-note-release` are at `2ade213`, clean for core sources, and their complete `src/core` trees were verified byte-identical. Per-profile records retain compiler versions, exact compile/run commands, header-tree hashes, archive hashes, source hash, binary hash, exit codes, byte counts and capture hashes. Candidate core tree is separately verified against the frozen `c0219bd` content.

`source-freeze.sha256` records both source versions. Initial `v1` GCC-generic baseline runs passed; their source, compiler warning, binary, complete raw outputs and logs remain under `CompatibilityCapture-v1/`. `v2` only separates three statements in main to remove `-Wmisleading-indentation`; it has no behavioral changes. There were no failed fixture runs. Final complete evidence is under `CompatibilityCapture-v2/` and the machine-readable paired comparison is `comparison.json`.
