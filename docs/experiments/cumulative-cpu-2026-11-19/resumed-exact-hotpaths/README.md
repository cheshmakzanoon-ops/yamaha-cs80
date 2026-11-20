# Resumed exact VST3 hot paths

This phase preserves oscillator settings, Standard/Legacy rates and latency, routing, and every tested audio/state/FP output. It adds four independent kinds of work reduction to the [previous phase](../resumed-control-dsp/):

- **Held oscillator:** a qualifying segment stores the decay and endpoint values it has already calculated. Advance reuses them only if the root solver left the duration unchanged; shortened or otherwise unqualified segments still take the original route. This is a reuse within the current interval, not a cross-sample cache. The canonical renderer remains unchanged. See the [isolated oscillator qualification](../held-span-and-comparator/).
- **Uniform receivers:** two rank records are staged before publishing the duplicated receiver values. Both rank solves must succeed before publication. The existing arithmetic, ordering and qualification gates remain. See the [standalone engine evidence](../../engine-lazy-records/).
- **TRG5 touch:** the qualified pressure problem can reuse evaluations for repeated pressure levels. Nonqualifying inputs retain the original solver; recovery and sum ordering are unchanged. The permanent regression checks heterogeneous pressure. [Pressure fixture and receipts](pressure-replay/).
- **Host I/O:** write completed audio spans through stable planar pointers, and publish the processor's existing effective `ControlFrame` directly. Sample order, span bounds, FP operations and async publication semantics do not change. A separate external-input scratch change is not included.

## Direct comparison with the start of this pass

Real Linux x86-64 Release/PIC VST3, strict GCC 14.2.0 and official JUCE 8.0.11 at `c352e2489000ffeb9e397051377d75eef2efe641`. The baseline product is `9208eb6323207b5054055dd2e0549e45324c539a` (screenshot-only source snapshot `631b7708`). The contender is the complete code in this commit, not an arithmetic sum of lower-level estimates. Module SHA-256: `3a1627c434d8b0d2953796c4d8680421c961f9398e45e4527219128f62e18d22`.

| True host buffer | Fresh quiet | Eight varied held notes |
|---|---:|---:|
| 16 frames | −4.372% | −3.945% |
| 32 frames | −4.672% | −4.001% |
| 64 frames | −4.838% | −4.029% |

At 64 frames, equal held was −4.045% and varied wake was −2.898%. All eight cells used fewer translated guest instructions, reads and writes. Each window counts 256 stereo frames at 48 kHz, with duplicate and next-callback additivity checks for both builds. Against the immediately preceding product, the same 64-frame scenes improved by −3.856%, −2.217%, −2.209%, and −1.576% for quiet, equal held, varied held and wake respectively.

Native parity was byte-exact across 27 histories and 82,944 callbacks per arm (3,286,656 stereo frames and 216 public snapshots per arm). Six additional connected MIDI histories individually excited pitch bend, channel pressure and mod wheel streams and still matched. All 196 runtime rank-selector combinations were confirmed through serialized readback, with identical audio, state, FP traces and metadata. Six separately checked pressure scenes also matched, including their next-window controls. Standard and Legacy still report 67 and 63 samples respectively. Focused WSC endpoint, touch, modulation matrix, MSEG transport and retrigger, MPE, filter batch and scheduling test executables passed in the fully linked contender.

These are Callgrind translated guest work counts and untimed native comparisons, **not** a real-time DAW benchmark or a physical CPU percentage. The physical hardware thresholds remain unverified. [Replay without this workspace](harness/REPRODUCE.md).

## Direct comparison with the original VST3

The original product `db02cd8fa56892c6ea2cf7df084bcf190204968e` was rebuilt with the same strict Release/PIC and JUCE toolchain. The original ELF SHA-256 is `0eb5fc999fdadc8e974f464e31210f39f24ca7c40a3518c1cf2661d2a313c42a`. These are separate measurements, not the sum of reported gains. Original and contender passed the same repeated/additive controls, 27 native histories, six connected MIDI histories and 196 rank-selector combinations exactly.

| True host buffer | Fresh quiet | Eight varied held notes |
|---|---:|---:|
| 16 frames | −44.248% | −18.846% |
| 32 frames | −44.880% | −18.576% |
| 64 frames | −45.226% | −18.436% |

At 64 frames, eight equal held was −18.817% and varied wake was −14.272%. Six separately qualified pressure workloads reduced guest instructions by 15.97–18.77%, with matching PCM, state and FP. The same caveat applies: instruction counts are not CPU percentages or deadline/timing measurements.
