# Resumed exact control/DSP pass

This composes the preceding AVX2 filter change with three changes in the original render path. It compares the full Linux x86-64 VST3 module directly against `9208eb6323207b5054055dd2e0549e45324c539a`, not against one component or an estimated sum. Both were built with strict GCC 14.2.0, Release/PIC, and JUCE 8.0.11 at `c352e2489000ffeb9e397051377d75eef2efe641`.

- The already qualified masked-nearest held oscillator uses the held comparator's identical finite value and constant and exact +0 slope. It omits the zero terms, still adds the live residual and consumes it in the original order. The canonical path is unchanged. The fixed regression covers finite, signed-zero, subnormal and nonfinite residual words.
- The BBD history mirrors three samples and their stamps beyond the logical ring. Reads retain the original cursor, stamp comparisons and interpolation, with fewer address wraps. A test covers the boundary, copying while active and reset. The isolated proof includes actual traps and exact continuation on GCC 13.3 and 14.2.
- The qualified strict GNU path does not build a per-voice input record for an inactive Modern modulation matrix. Global transport advances normally. Live routes see the same prior-rank envelopes; active, releasing and decimated voices retain their clocks and eligibility. Nonqualified builds retain the original dispatch.

## Matched real VST3 work

At 48 kHz, the 16-, 32- and 64-frame versions each count the same 256 stereo frames per arm (16, 8 or 4 genuine callbacks). Captures include a duplicate selected window and an exact one-callback increment/additivity control. Translated guest instructions, reads and writes are not physical hardware counters, DAW timing or CPU usage.

| State | 16 frames | 32 frames | 64 frames |
|---|---:|---:|---:|
| Fresh quiet | −0.921% | −0.986% | −1.022% |
| Eight varied held notes | −1.821% | −1.847% | −1.861% |
| Eight equal held notes | — | — | −1.869% |
| Varied wake | — | — | −1.342% |

These are guest instruction changes. All three metrics fell in every measured case. The native comparison was byte-exact across 27 histories, 82,944 callbacks, 3,286,656 stereo frames and 216 state snapshots per arm. It included 44.1/48/96 kHz, variable 1–64 frame blocks, factory quiet/held, automation, external input, effects, pulse, and MPE event/metadata traffic. Standard and Legacy still report 67 and 63 samples respectively; neither is a DAW round-trip measurement. Absolute CPU targets on physical machines are unverified.

Six additional connected MIDI-routing histories separately excited master bend, member bend, channel pressure and the modulation wheel. Neutral-stream controls changed the PCM; the contender and baseline matched all audio, state and event data. The final build including the BBD regression's test-only friend was byte-identical to the qualified ELF. Focused oscillator, BBD, Modern effects/LFO and MPE integration tests passed.
