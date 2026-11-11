# Direct processor callback allocation regression

## Purpose and result

The Linux-only `cs80_audio_callback_allocation_tests` target adds an automated
heap-activity regression around the real `Cs80AudioProcessor::processBlock`, not
a model callback. On the production source at
`39a94c4812a8b94912f761f0c2a622a9b3bc58ec`, the strengthened fixture passed **193 checks,
0 failures**; its registered CTest passed in 0.74 seconds (0.75 seconds total). No production defect
was reproduced, and no production source was changed.

The test arms thread-local counters only during each synchronous callback,
asserting zero intercepted allocation calls and zero intercepted non-null frees.
Construction, parameter setters, prepareToPlay, audio/MIDI-buffer construction,
SysEx creation and output reporting occur outside that scope. This includes the
first callback after preparation, rather than warming it away.

## Coverage

The matrix uses 44.1, 48 and 96 kHz; initial Legacy and Circuit policies; and
initial eight- and sixteen-voice modes, for 12 configurations. Each configuration
executes 13 measured callback scopes:

1. First prepared block.
2. Eight or sixteen note onsets.
3. Held voices.
4. Sustain, pitch bend and channel pressure.
5. Valid addressed real-time MTS scale/octave SysEx at offset 64.
6. Malformed MTS SysEx.
7. MIDI program change.
8. Eight-to-sixteen or sixteen-to-eight voice-mode transition.
9. Legacy-to-Circuit or Circuit-to-Legacy policy transition.
10. Offline transition.
11. Return to real-time mode.
12. MIDI all-sound-off.
13. A zero-frame callback.

Ordinary blocks have 256 frames. Extra checks verify that the valid MTS message
actually changes the selected channel's stored tuning and the malformed message
preserves that table. The policy target is chosen opposite the current effective
mode after program recall, and an assertion confirms the engine policy really
changes. The earlier 181-check fixture passed, but selected its target from the
initial policy and did not establish this post-recall transition explicitly.
Program and other transition cases exercise their callback paths;
this fixture does not replace their separate detailed semantic tests.

## Instrumentation and limits

The test executable replaces C++ allocation/deallocation overloads, including
array, sized, aligned and nothrow variants. GNU linker wrapping instruments linked
references to `malloc`, `calloc`, `realloc`, `aligned_alloc`, `posix_memalign`, and
`free`. A positive probe through volatile allocator function pointers and C++
operators checks seven allocation calls and six deallocations before the matrix.
The counters themselves do not allocate, lock or print while armed.

This is **not exhaustive process-wide heap interception**. GNU wrapping does not
rewrite calls internal to shared libraries or libc; realloc's internal release
is not a separate intercepted free. Calls on other threads are outside the
thread-local scope. The custom operators belong only to this test executable,
not the shipping plugin. The target is Linux-only and links the native processor
library, not the dynamically loaded VST3 wrapper.

The result therefore does not qualify wrapper/DAW allocations, live MTS-ESP,
concurrent session restoration, every control or modulation configuration,
preparation/release outside processBlock, GUI work, lock-freedom, sanitizers or
CPU/deadline targets. Zero intercepted heap activity is a bounded regression gate,
not proof of unrestricted real-time safety.

Run with the native dependency directory configured for the build:

```sh
ctest --test-dir build -R '^cs80_audio_callback_allocation_tests$' --output-on-failure
```
