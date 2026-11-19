# Direct cumulative exact VST3 work

Actual Release VST3 source baseline `db02cd8fa56892c6ea2cf7df084bcf190204968e`
and final comparison source `b88b1018291274123fd45126b47cd07d1bda45f7`.
The original is a clean build; the candidate was linked after rebuilding every
affected shipping unit. GCC 14.2 Release/PIC, pinned JUCE, matched Linux runtime.
This is a new direct original-to-final measurement, not a sum or product of
successive changes.

| 48 kHz Standard | Quiet guest instructions | Eight varied-held guest instructions |
| --- | ---: | ---: |
| 16 frames | −40.380% | −14.103% |
| 32 frames | −40.761% | −13.741% |
| 64 frames | −40.971% | −13.555% |

The [64-frame measurement](64.json) also records −14.249% for eight equal
held notes and −10.714% for varied wake. Guest reads and writes fall in all of
these final-versus-original fixtures. The [16/32-frame receipt](16-32.json)
contains all observed work counts. Both protocols completed 40 real host
processes, with exact repeat and independent prefix-plus-one controls per
window and per image. The rebuilt original reproduced the historical 64-frame
work counters exactly.

Direct original-to-final native VST3 playback and independent dynamic policy
and quality transitions compared bit-identical captured PCM, public state,
floating-point observations, events, and reported latency/tail in the tested
histories. Standard at 48 kHz still reports 67 samples (about 1.40 ms
`67 / 48000`), the algorithmic plugin delay, not audio device/DAW round-trip
latency. The original native clock experiment remains closed failed; no
native callback CPU timing, weak physical target or DAW performance was
established by this software-work study.
