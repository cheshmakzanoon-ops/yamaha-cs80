# Final twelve-checkpoint serial CPU qualification

2026-11-01, [GitHub Actions run 41292549292](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41292549292),
[complete archived artifact 13079312111](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41292549292/artifacts/13079312111).
Artifact: 1,087,594 bytes; SHA-256:
3ffa33d3d5674ea0f1aed409efb3d8940024a8891f4e51147a1d69b3f37dffed.
Its current Actions retention expires 2027-01-30. Raw timing is a CI
software observation, not a hardware measurement, operating-system CPU
percentage or real-DAW validation.

**Neither requested CPU target passes on final production audio.**
Final audio commit 83b7f7670f000a95c6b44f96ffb0df3894755d14 measures
**0.605709375–0.8885724375** held-note realtime factor (RTF),
versus original same-run main db24a471 at **0.588122625–0.696543**;
the required range is 0.15–0.20. Quiet idle is **0.067570125 RTF
(6.7570125%)** versus original **0.143822625 RTF
(14.3822625%)**: a 53.02% relative idle reduction, still
above the 0.02 (2%) budget. Final held p99 reaches 1.0504269375
RTF and selected held fixtures report 49 deadline misses;
moving-control median/p99 are 0.8495158125/1.0522715625 RTF,
with 109 selected misses. Earlier 9V45-host timings cannot replace
these same-host observations or establish a held-voice speedup.

## Frozen timing and scoring method

All twelve immutable sources were compiled before any of 36 timing rounds.
The rounds ran serially in predeclared round-major order (three per source)
on one Ubuntu 24.04 AMD EPYC 7763 VM, four virtual CPUs, GCC 13.3,
Release optimization. Each round retained the stock 48 kHz/256-frame,
1,024-callback fixture bank, 27 cases, 51 result rows. The unchanged
reporter selects the *minimum callback median* of three rounds for each
fixture and takes p99 and deadline misses from that same round; all raw
rounds are retained. Held ranges encompass eleven paired-tone fixtures.
RTF is callback wall-clock time divided by the 5.333 ms audio-block
budget, not the operating-system CPU percentage. Hypervisor scheduling
remains uncontrolled; other-host absolute results must not be mixed.

| Immutable source | Held median RTF | Held p99 RTF | Selected held misses | Idle RTF | Moving median / p99 / misses |
| --- | ---: | ---: | ---: | ---: | ---: |
| Main before db24a471 | .588123–.696543 | .606453–.806714 | 1 | .143823 | .669017 / .850359 / 0 |
| Inherited 2ee33dcc | .612583–.891827 | .631358–1.050665 | 52 | .102794 | .853611 / 1.047152 / 94 |
| Lazy histories 476c2f1f | .609350–.892868 | .629349–1.057810 | 54 | .099601 | .850860 / 1.055301 / 117 |
| Exact BBD/PRA f79b6f7f | .606169–.890869 | .624286–1.055695 | 51 | .088707 | .853909 / 1.057592 / 110 |
| Stationary controls 8dac7955 | .605763–.890443 | .626573–1.058250 | 54 | .087043 | .851802 / 1.056826 / 116 |
| Unused LFO 193f1a31 | .603611–.886423 | .620412–1.053377 | 57 | .087016 | .849773 / 1.053272 / 111 |
| Silent rank 1.5 kHz 18400c29 | .605175–.890976 | .621646–1.053824 | 54 | .077864 | .852066 / 1.055909 / 120 |
| Pre-FIR native BBD 475063ca | .606996–.891424 | .621695–1.055017 | 55 | .078080 | .852713 / 1.056552 / 109 |
| Zero-history FIR 23a7b8df | .606771–.889068 | .621518–1.049547 | 52 | .068789 | .853143 / 1.063521 / 116 |
| Ring charge 481c7e9f | .604950–.891357 | .622728–1.055202 | 55 | .067476 | .853273 / 1.056318 / 121 |
| Connected VCA b75b4d65 | .607846–.890813 | .625190–1.053859 | 52 | .068353 | .850623 / 1.053824 / 105 |
| Final VCA ownership 83b7f767 | .605709–.888572 | .622811–1.050427 | 49 | .067570 | .849516 / 1.052272 / 109 |

The isolated pre-/post-FIR idle pair changes from .07808025 to
.0687890625 RTF, an 11.8995% reduction in the selected idle
median. The recorded 306 before/after non-timing fields match
exactly. This does not establish a held-note speedup or allow
attributing any timing difference solely to scheduling variance.
The subsequent ring/VCA changes follow their separate V2 sound-change
and strict byte-compatibility sequences.

## Independent integrity verification

The downloaded ZIP's digest matches its Actions artifact digest.
An independent read-only verification reproduced **346/346 internal
SHA-256 entries**, **884/884 source-file hash records**, **36/36
completed rounds**, **1,836/1,836 finite result rows**, and
**12/12 frozen reporter summaries**, including medians, matched
p99s and deadline counts. All twelve immutable source trees and
benchmark identities reconciled; all **27 preregistered source/runner
mutations** were rejected before timing. The deterministic 7,287-byte
compact summary regenerates with SHA-256
0dd8b66bc168dcc03fc7ff9c5b000679ee61afbdac43c44c55604c0a89afd9c5.
These are integrity checks of captured code and timing, not analogue
fidelity evidence.

To reproduce the verification, use the [frozen serial request and
retrieval verifier](./verify-retrieval.py), this original ZIP and the
immutable source SHAs above. This document does not alter source audio,
sample rates, event or numerical bounds, or the three-round selection
rule. Full-lane SIMD or other several-fold work reduction remains open
and requires separate predeclared state, branch, NaN, rounding and
work-equivalence oracles; elapsed electrical circuit state cannot
simply be paused or thinned to meet a percentage.
