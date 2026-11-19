# Uniform source cutoff: native VST3 parity and translated work

**In fresh, unplayed Factory audio, the new cutoff path executed 9.86% fewer simulated guest instructions across real VST3 callbacks. With eight equal-velocity notes shortly after onset the saving was 1.38%. Neither number measures native CPU time.**

The comparator is the 67b9915 product (which already contains route reuse); the only new production TU includes the cutoff helper and replaces its one source-factory cutoff evaluation call. Ordinary Linux x86-64 GCC 14.2.0 -O3 -DNDEBUG, pinned JUCE 8.0.11, real native IAudioProcessor::process. The comparator ELF SHA-256 is 6b32efbf93190e1987c7c5fc0104588eac1c3feefc2f936feac24e90a3c2da30. The actual product candidate, Engine.cpp blob 5b56992676392105f3174ae7c3b996688b714f54 plus UniformSourceCutoff.h blob 5a5713581dc5469540544b3d4fdbaeb32cdaab03, produced ELF 0ed90e446f1faa6153b0e2f76f074b7da62201be49f76fc9183375e9ff20edf2. Its .text is 64 bytes shorter than the earlier research-v2/v2b/v2c binaries. All four longer native lifecycle comparisons and all ten guest-work process slots, plus four native short-history controls, were therefore independently reacquired for this exact product ELF rather than inferred from source equivalence. All measured counters and observed DSP/public outputs reproduced the earlier prototype. See binary-identity.json for the distinct artifacts. Other compilers and processors are not qualified by this experiment.

## Native audio, public state and FP trace

Two freshly launched processes per scene (one comparator and one cutoff). No clocks, sampler or callback timer. All warm, release-wait, observed and wake calls were captured, not just the plotted windows. All per-arm outputs matched byte-for-byte:

| Scene | Genuine callbacks per arm | Stereo frames | Captured public states |
|---|---:|---:|---:|
| Held Factory eight | 5,120 | 327,680 | 3 |
| Never played | 17,408 | 1,114,112 | 3 |
| After eight-note release | 25,600 | 1,638,400 | 4 |
| Sixteen fresh wake placements | 16,896 | 1,081,344 | 48 |
| **Total** | **65,024** | **4,161,536** | **58** |

That is 33,292,288 PCM bytes, 4,161,536 bytes of callback return/silence, consumed event and before/after/next-operation FP records, and 828,472 bytes of length-prefixed public states. See native-lifecycle.json for hashes/scene sizes. This longer capture's raw PCM and FP streams are not redistributed in this receipt; run-lifecycle.sh recreates them and compares the two complete streams and state files directly. No real-DAW or deadline claim is implied.

## Callgrind: dynamic work, not CPU percentage

A separate host with unmodified plugin modules runs a short history (32 warm, five eligible callbacks, one continuation). Each translated process still renders all 38. Measure only first four, all five, or fifth only. Each scene has two identical route-only stock arms, one five-call positive, the independent fifth-only arm, and the four-call cutoff arm. Both same-stock traces matched exactly and five = first four + fifth exactly for instructions, read references and write references. The common host-call stub is excluded by extracting the inclusive real plugin entry edge. All adverse counters, if any, remain in decision.json.

| Scene (four 64-frame, 48 kHz callbacks) | Guest instructions, stock → cutoff | Guest reads | Guest writes |
|---|---:|---:|---:|
| Fresh quiet | 7,351,269 → 6,626,789 (−9.855%) | 1,565,998 → 1,500,974 (−4.152%) | 896,262 → 843,526 (−5.884%) |
| Eight equal-velocity notes, short held | 25,326,447 → 24,975,727 (−1.385%) | 6,722,273 → 6,692,065 (−0.449%) | 3,252,690 → 3,226,066 (−0.819%) |

Guest instructions are not cycles; data references are not bytes, cache misses or time. Each runtime's stock/cutoff PCM, callback/host FP records and four public state snapshots matched throughout. Native and translated PCM and public states also matched. The guest exposes AVX2 but not native AVX-512F; XCR0 7 versus 231. Translated MXCSR 0x1f80 differs from native 0x9fc0 initially then 0x9fe0; no other recorded callback/FP field differed. Guest four-wide backend self instructions were observed for the short-held scene. No ownership of private voice state, native cycle saving, CPU-time change, long-held or arbitrary-preset throughput is inferred.

## Replay and reproduce

To replay all ten named Callgrind data dumps and the native/guest short-history integrity controls without running a plugin:

    mkdir replay
    tar -xzf evidence.tar.gz -C replay
    cp verify.py replay/
    python3 replay/verify.py > replay/decision.json
    diff -u decision.json replay/decision.json

The archive contains the named measured dump, public/FP/PCM files and host receipt for each slot (textual machine paths replaced by labels; counters and binary observations unchanged). Terminal dumps after count reset/instrumentation shutdown are not measured. The SHA-256 manifest and binary-identity.json describe the checked inputs. Reacquiring needs the pinned SDK at JUCE/modules/juce_audio_processors_headless/format_types/VST3_SDK, Linux/x86-64 GCC C++20, two trusted module ELFs, and Valgrind 3.24.0 with client headers. Use bash run.sh PROJECT SDK ROUTE.so CUTOFF.so NEW_DIRECTORY for the short-history counts; for the long native exact check use bash run-lifecycle.sh with the same arguments and a different new directory. For workspace-extracted Valgrind use VALGRIND, VALGRIND_LIB and VALGRIND_INCLUDE_ROOT. These are opt-in, not release gates or installation instructions.
