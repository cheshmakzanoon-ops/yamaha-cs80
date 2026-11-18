# Untimed native mirror lifecycle evidence

**PASS for this fixed history, hybrid prototypes only.** Four genuine native processes were captured: two independent cold stock runs, one ordinary V1 candidate and one separate counted V1 candidate. All 30 ordinary files, 474,170 bytes per process, match exactly: stereo PCM, caller/next-operation FP observations, serialized public states and event records. Audio entered the genuine VST3 `IAudioProcessor::process`; side state used `IComponent::getState/setState`. No processor cast or replacement renderer was used.

Both candidate modules retain 19 original header-dependent objects. One native wrapper TU was rebuilt and one helper TU added with the original release options; original stock archives and wrapper-object bytes were authenticated. Header changes add declarations/friendship without fields. This is a **genuine loaded hybrid release-module prototype**, not a full clean candidate product build. Original unchanged bundle metadata was copied. The normal module has no observer hooks.

## Fixed 80-callback history

64 frames/callback; Source Performance, factory selectors 0/0, volume .15, drift .37, dispersion .18, drift model 0, analog noise 0, modulation amount .5 and MSEG rate 2. Mode values below are the public 8/16 selector, not observed private voice counts.

| Block | Action before real audio callback |
|---|---|
| Before 0 | Initial 48 kHz setup, then public state edit variant 0 |
| 0 | Mode 8; eight note-ons 36–43 at offsets 0–7 |
| 8 | Config variant 1 while notes held |
| 16 | First mode 16 activation; eight new notes 60–67 |
| 24 | Config variant 2; transport becomes playing, 97 BPM, PPQ 8.5 |
| 32 | Mode 8 |
| 40 | Config variant 3 while extension dormant |
| 48 | Mode 16 reactivation, explicitly no new note event |
| 49 | Eight new notes 76–83 |
| 56 | Stop/deactivate, prepare 96 kHz/64, reactivate; sixteen notes 36–51 |
| 64 | Config variant 4 while mode 16 |
| 72 | Mode 8; continue through block 79 |

The five state edits each replace only `MODERN_LAYER`: all three MSEG shapes and all 16 slots. Their before/input/after triplets retain the wrapper private trailer. Trigger/sync, shape level and slot source/destination/target change deterministically with variant. The exact host is `host.cpp`, using the repository's genuine SDK test scaffold; its hash and source identity are in `evidence.json`. These are quiescent component-state calls, not a GUI/DAW concurrency test.

`callbacks.csv` contains **all 80 callback observations** with per-block PCM hashes, full before/after/next-probe FP and errno fields, mode/rate and count boundaries. There are **12 public serialized state snapshots**, plus 15 edit-triplet state files, not 80 private Engine-state snapshots. Each process produced 5,120 finite, nonzero stereo frames and 40 note-on inputs.

## Separate observer witness

`observer.patch` contains every diagnostic source change, including the integer counter and host delta. It observes the actual guarded helper entry, successful exit after rebuild, and unconditional transport return. Exactly 40 entries/accepts/transport returns occurred, one in each mode-16 callback; all 40 mode-8 callbacks had zero. Blocks 16, 48 and 56 each have one accept. No fallback occurred in this ordinary guarded history. Counted PCM/FP/state bytes equal the normal module's bytes. These counts are from the explicitly instrumented module only, not normal VST telemetry; private active voice counts and raw ModernLayer state were not observed.

## Offline verification

Run `python3 verify.py` and `python3 test_verify.py`. They only read this folder. The verifier reconstructs the 80 CSV rows from binary records, validates all config edits/public snapshots, finite PCM, control modes, next-operation FP, actual count deltas and four independent recorded digest rosters. Twelve tests include synthetic corruptions of PCM, FP controls, mode, first16/zero8 counts, shape/trailer, CSV and identity records.

`captures.tar.gz` preserves all 30 common original files and the observer count log losslessly. Identical copies are stored once, with each process's original file digests in `evidence.json`; no captured PCM/FP/state bytes were removed. Private command paths, runtime identifiers/addresses, build products and module binaries are omitted. Hash manifests detect later changes but are not cryptographic proof that execution occurred.

No native timing, CPU saving, real-time factor, clean-build admission, unsupported-FP fallback coverage, or full product/DAW qualification follows from this evidence. See the adjacent direct-core tests for their distinct guard/trap scope.
