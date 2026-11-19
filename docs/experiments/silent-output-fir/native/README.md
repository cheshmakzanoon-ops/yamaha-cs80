# Native sound and state: shipping cutoff vs shipping cutoff + signed-zero FIR

The exact combined Linux Release plug-in was tested against its **real current-route + UniformSourceCutoff** control, not against a route-only binary or a FIR prototype. A real VST3 SDK host observed **65,024 callbacks, 4,161,536 stereo frames and 76 public component-state snapshots**. The two instruments matched across **37,760,474 bytes** of raw audio, callback records, state and scene receipts. All six additional native automation cells passed for both modules and matched **216 more files / 7,077,888 bytes**. No CPU timers, active sampling or performance measurements were involved.

| Factory scene, 48 kHz/64 frames | All callbacks, including preparation | PCM frames |
|---|---:|---:|
| Fresh silent instance | 17,408 | 1,114,112 |
| Eight held notes | 5,120 | 327,680 |
| Full release after eight note-offs; 8,192 uncounted tail callbacks before region | 25,600 | 1,638,400 |
| Sixteen quiet instances woken with eight notes each | 16,896 | 1,081,344 |

The [period-zero host](https://github.com/cheshmakzanoon-ops/yamaha-cs80/blob/9d4dcb7e4088cd2af2980d2e7c21ecc4ad1849ce/tools/native-vst3-profile/host.cpp) plus [this observer-only patch](public-probe.patch) records every sample of both channels (33,292,288 bytes), the SDK result/event/host-silence flags and pre/post MXCSR, x87 control/status and errno for every call (3,381,248 bytes), and four `IComponent::getState` snapshots per instance placement. Those snapshots are ordinary publicly serialized parameters/state, not private oscillator/filter histories. The quiet windows remain quiet; held and wake produce the expected sound. Separate ring automation ran natively at 44.1, 48, and 96 kHz, both real-time and offline, with 36 matching PCM capture files per cell. These finite tests are not a full pluginval/DAW or cross-platform qualification, and do not establish a CPU saving.

## What was actually built

- The stock VST3 `0ed90e446f1faa6153b0e2f76f074b7da62201be49f76fc9183375e9ff20edf2` contains the exact [9d4dcb7](https://github.com/cheshmakzanoon-ops/yamaha-cs80/commit/9d4dcb7e4088cd2af2980d2e7c21ecc4ad1849ce) core TU/UniformSourceCutoff.h in the normal, previously verified unchanged-layout VST3 wrapper.
- The combined VST3 `74f2507db4a7042a23a96b9a8c2249cc2f76d1f7f417ebb4838498d7f0578895` contains that same cutoff and the final v3.1b Linux-only FIR, normal compile options, no eligibility counters. Its complete production tree was compared with the base: only `Engine.cpp`, `Engine.h`, `EngineOutputFir.h` and `EngineOutputFir.cpp.inc` differ. The exact current source Git blob IDs, CMake closure, compiler and dependency identities are in [the receipt](recorded.json.gz).
- Layout changed from stock, so the candidate used a **layout-coherent VST3 wrapper build**: an initial clean native Release build, then all 21 prior/new layout-header readers recompiled when the FIR moved into the engine-owned wrapper. Going from that verified layout to the final combined source/guard left its headers/layout unchanged. Only the `Engine.cpp` TU depends on the added cutoff header/final owner implementation and it was rebuilt, the core archive and complete VST3 relinked; the other 74 native objects matched the coherent predecessor byte-for-byte. The actual final TU object SHA256 was `869909c4efc1afdfd42fb8accee2b3432b07077d1596410fb7035af8d636a4e0`. Matching normalized 75-command production build plans; Debian GCC 14.2.0, JUCE 8.0.11; no FMA/native tuning or experimental reach instrumentation.

## Replay (no plug-in execution) or rerun on already-built modules

`python3 check.py replay` checks the stock/candidate hashes for each native observation/cell and the fixed coverage. The compact receipts contain digests, not all 44 MB of raw sample/state observations. The checked source/blob and artifact hashes are provenance, not a signature. For a fresh full public comparison on Linux x86-64:

```sh
python3 check.py compare --source /path/to/yamaha-cs80 \
  --sdk /path/to/juce/modules/juce_audio_processors_headless/format_types/VST3_SDK \
  --stock '/path/to/stock.vst3/Contents/x86_64-linux/CS-80 Reference.so' \
  --candidate '/path/to/combined.vst3/Contents/x86_64-linux/CS-80 Reference.so' \
  --output /tmp/new-native-fir-run
```

The host requires its pinned four-scene source. The run compiles the test host from the supplied checkout, uses fresh instance homes, emits public sample/state files and compares the new runs. It does not build the two plug-ins. To repeat the automation matrix, build the repository's `cs80_vst3_ring_automation_tests` and separately run all six real-time/offline cells against both actual module files; compare each output file byte-for-byte.
