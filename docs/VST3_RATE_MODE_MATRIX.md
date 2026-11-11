# VST3 sample-rate and process-mode regression matrix

The actual-module automation host previously hard-coded 48 kHz and real-time
processing. Its optional explicit configuration now covers 44.1, 48 and 96 kHz
in both real-time and offline modes. The original no-extra-arguments invocation
still runs 48 kHz real-time. Invalid configuration strings are rejected before
loading a module.

The chosen rate and mode are used consistently in setupProcessing, initial and
active zero-frame flushes, audio process data/context, and program-boundary
processing. Five additional CTest registrations combine with the original to
exercise the six configurations. Existing tolerances and assertions are unchanged;
there are no production plugin or DSP edits.

## Verification

Against the actual Linux VST3 module built from `d5ffae4c46c7652ea825d9a2d7576f8406020442`
(the published `ace89dfd0eab21531cbff27d145053de223ce67c` tree equivalent), all six
registered tests passed: **6/6, 37.06 seconds**. Every configuration reported zero
maximum partition, sixteen-voice, zero-frame state-boundary and native-reader
observation audio error. Policy and program-boundary assertions also passed.

The independent repeat passed **6/6 in 36.69 seconds**. Its 48 kHz offline
maximum partition error was **3.67756e-07**, within the unchanged existing
tolerance; all other listed top-level errors were zero. Thus the evidence supports
tolerance-bounded partition equivalence, not universally exact offline partition
invariance. The zero values above describe the original run only.

This is Release with JUCE 8.0.11 and AVX2 filter batching OFF. Tests use 32-bit
samples and a maximum configured block size of 4096. Each instance is configured
in one rate/mode; this does not test changing processing mode or rate during an
active instance. It does not qualify 64-bit samples, GUI/display behavior, external
DAWs, pluginval, live MTS-ESP/MIDI, CPU throughput or hardware fidelity. Offline
processing is exercised through the VST3 protocol, not inferred from a direct
processor fixture. Timing above is test runtime, not an audio-performance result.
