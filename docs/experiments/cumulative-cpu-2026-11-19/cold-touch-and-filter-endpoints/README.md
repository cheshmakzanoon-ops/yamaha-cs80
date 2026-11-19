# Exact cold touch bank and saturated filter endpoints

A never-acquired keyboard is shortcut only when the physical touch state is
bitwise reset, all voices are inactive and unheld, and the existing masked,
nearest source-rendering FP gate allows it. The shortcut still publishes the
negative-zero C readback and does not carry across a prior strike, altered
capacitor state, or unqualified compiler/environment. The packed filter
scheduler already requires sounding ranks and does not use the cold shortcut.

The exact AVX2 filter solver already computes an ordered strict magnitude
mask for its saturated polynomial endpoints. Drive the original signed ±1
endpoint with that mask, instead of two duplicate FP comparisons. An altered
sign and a non-strict knee comparison were independently rejected by negative
controls. The existing compiler, architecture and floating-point admission
continue to apply.

## Whole plug-in confirmation

The [marginal 64-frame guest receipt](work.json) on the previous main shows
−3.843% instruction work in fresh silence, −0.140–0.141% on eight held notes
and −0.109% on varied wake. All four had equal public audio/state within each
runtime and both repeated/additive controls. Ten real VST3 native histories
also matched PCM, state, events, floating-point observations and latency/tail
across 800 callbacks in each of four processes.

Against the previous pass baseline (product 0832721; docs HEAD d62f3ae), the
[five-change 64-frame receipt](composed-work.json) and real
[16/32-frame receipt](small-buffers.json) show:

| 48 kHz Standard | Fresh quiet guest instructions | Eight varied-held guest instructions |
| --- | ---: | ---: |
| 16 frames | −5.965% | −1.259% |
| 32 frames | −6.138% | −1.230% |
| 64 frames | −6.233% | −1.216% |

The small-buffer host predeclared 2,048 warm frames and a 256-frame measured
window for each cell, with separate duplicate and next-callback additivity
controls. Standard still reports 67 samples algorithmic latency at 48 kHz.
Twelve additional real policy-switch histories retained the original reported
latencies and bit-identical public observations. These are **not** native
CPU time, physical old-machine CPU percentages, or DAW round-trip latency.
