# Rejected prepared oscillator profile cache

**Decision: do not integrate the cache.** This is an offline negative result,
not a shipping optimization or a new CPU/idle scorecard. The prototype, exact
comparison harness, paired cost probe and raw evidence remain under
[`experiments/oscillator-profile-cache`](../experiments/oscillator-profile-cache/).
Production `src/`, registered `tests/` and `CMakeLists.txt` are restored exactly
to screenshot parent `35f3431400b28d95d41ef6c6a863db0516edfa01`, whose source
is `87405eed4b11779cc52816599d5c093601fcac2e`. Screenshots are unchanged.

## Why this experiment

The [October 31 scorecard](../STATUS.md#scorecard-2026-10-31-00ceccf) has unmet
active CPU and idle goals. Its instruction profile identified the reconstructed
oscillator event solver as a large cost. That dated profile motivated a small
prepared-value experiment; it is not evidence of the current cost distribution.
The cache did not change the nominal circuit model or approximate a changing
control, event time, exponential, filter or residual.

The [acceptance plan](../experiments/oscillator-profile-cache/preregistered.md)
was committed before timing, originally at `60e8cbe5015ae8b4536c90ff6406e97781f06fae`
and rebased onto the screenshot child at `3dbe589`. Candidate source and tests
were frozen at `c95dafc76639706c0904a24c130dedb625aafe9a` before either timing run.
The original plan remains historical text, including its then-current fixture
path and the wider proposed cache. All archived copies retain their tested bytes.

## Correctness narrowed the candidate first

The broad aggregate and flat-field caches failed the native GCC release
bit-pattern comparison: **318,101 failures in 328,508 checks**. The first observed
continuation difference was `inputBaselineVolts` at nominal profile / 8 kHz:
`0x1.ef7c066325cp-9` instead of `0x1.ef7c06632a8p-9`. An identical-header A/B
passed. Disabling contraction made the aggregate pass, but compiler flags were
**not** relaxed to admit it. This observation does not identify the exact
contraction/inlining instruction or establish a compiler defect.

Isolated groups were tried before timing. Combining the geometry fields failed
again; only the three quotient-related values were selected for the cost test:

- maximum frequency, `min(.45 * rate, .95 / resetSeconds)`;
- triangle gain, `triangleSpan / rampHalf`;
- reset rate, `1 / resetSeconds`.

The selected per-instance cache refreshed only after successful basis acquisition
and before reset, copied with its profile, and initialized from the original
default members even when construction could not acquire a basis. A reused
rate/RC basis did not suppress profile refresh. It added **24 bytes**, from
5,984 to 6,008 bytes per oscillator on this ABI, or 384/768 bytes for 16/32
oscillators. Setup/copy costs were not timed. No dynamic division was replaced
by multiplication, and geometry, event search and state scheduling stayed intact.

The frozen narrow header passed **328,508 checks / 161,787 compared ticks** in
each GCC 14 generic/native run. Independent GCC 14 and Clang 19 generic/native
runs reproduced those counts. These are explicit bit-pattern comparisons of
all returned pins, continuation/residual fields and all diagnostic/event fields,
including signed zero, not padding-dependent structure comparisons. The corpus
covers eight rates from 8 kHz to 1.536 MHz, nine profiles, moving/invalid controls,
clamp/comparator boundaries, copy/reset/restore/idle transitions, same-basis
profile changes, invalid prepare and real registry exhaustion, including failed
constructors. The callback/idle allocation counter observed zero allocations.
This is a fixed floating-point-environment result, not a proof over every input,
compiler, changing rounding mode or sticky exception flags.

## Equal-workload timing, including the failures

Each GCC profile used `-std=c++20 -O3 -DNDEBUG`; native additionally used
`-march=native`. The 16/32-oscillator cases used identical precomputed inputs and
warm-up at 96 kHz internal rate. There were nine alternating-order pairs per
case. Each active measurement processed 32,768 frames; idle used 2,048 calls
advancing 16 internal samples each. Setup, input generation and final full-state
hashing were outside the clock. All three returned pins were consumed and a
GNU/Clang read-only memory observer exposed each complete oscillator object after
each call. No global memory clobber forced artificial profile reloads.

Both versions ran in the same process with the same workload. The child reported
CPU affinity `{8}` and every measured start/end CPU was 8. **All 432 rows / 216
pairs** are retained, with matching output/full-state checksums and zero reported
runtime failures. This was a shared machine with visible variation, not isolated
hardware, a deadline/WCET test or real audio-driver execution. Pinned affinity
does not eliminate shared-resource or scheduling interference.

The following values are the **median of the nine paired percentage reductions**,
`100 * (1 - candidate_ns / baseline_ns)`. Negative means slower. They are not the
ratio of separately selected timing medians. Every pair, including outliers, is
available in the raw CSVs and [validated summary](../experiments/oscillator-profile-cache/evidence/summary.json).

| Workload | Oscillators | GCC generic | GCC native |
| --- | ---: | ---: | ---: |
| Held low | 16 | −0.512% | −0.864% |
| Held middle | 16 | +0.255% | +4.940% |
| Held high | 16 | +1.396% | +2.679% |
| Moving pitch/PWM | 16 | −0.851% | −1.320% |
| Dense controls | 16 | −1.308% | +0.813% |
| Idle advance | 16 | +7.014% | +4.835% |
| Held low | 32 | +2.624% | **−3.015%** |
| Held middle | 32 | +1.287% | −2.958% |
| Held high | 32 | +1.801% | +4.374% |
| Moving pitch/PWM | 32 | −2.993% | +0.591% |
| Dense controls | 32 | +2.673% | +2.531% |
| Idle advance | 32 | +6.528% | +0.819% |

The preregistered primary gate required at least **3% median reduction across
all 45 active 16-oscillator pairs in each GCC profile**. The results were
**−0.004576% generic** and **+0.813107% native**: both failed. The native held-low
32-oscillator median also missed the secondary maximum-3%-regression gate.
The full range includes a −63.207% generic held-low pair and a −41.025% native
held-low/32 pair; neither was discarded. These noisy measurements are insufficient
to prove a universal slowdown, but equally do not meet the declared useful-cost
evidence requirement. An idle-only improvement cannot rescue the active gate.

No longer or selectively filtered rerun was used to change the verdict. The
larger archived prototypes were rejected on correctness before any timing.

## Qualification boundary and next step

Because the cost gate failed, this candidate did **not** proceed to the unchanged
oscillator numerical/promotion/continuity suites, pinned whole-Engine audio
compatibility, sanitizer/fault-injection qualification, or paired eight-voice
Engine timing. Those planned gates remain unrun for this candidate and must not
be inferred from the component corpus or older project evidence. There is no
shipping activation, new realtime qualification, measured whole-instrument gain,
or completion of the active/idle CPU targets.

A better next experiment is a fresh profile of the unchanged current solver,
then a bounded way to avoid repeated event-search work. That requires its own
continuation and changing-control contract; this result does not authorize a
segment cache, a looser numerical tolerance or altered floating-point flags.

## Reproduction and provenance

The [reproduction script](../experiments/oscillator-profile-cache/reproduce.sh)
compiles only archived headers and writes to a new output directory. It never
replaces shipping files:

```sh
docs/experiments/oscillator-profile-cache/reproduce.sh /tmp/osc-cache-check
# Optional fresh complete timing matrix on a permitted CPU, without deleting old rows:
docs/experiments/oscillator-profile-cache/reproduce.sh /tmp/osc-cache-timing --timing 8
python3 docs/experiments/oscillator-profile-cache/summarize.py
```

The summary script independently validates ordering, coverage, evaluation counts,
CPU observations, status/checksum agreement and both decision gates for the
committed historical rows. Fresh optional timings are separate observations, not
a rewrite of this decision. [The manifest](../experiments/oscillator-profile-cache/manifest.json)
pins the tested source, archived prototypes, tools, logs and results. GNU/Clang
are required for the offline complete-object timing observer. No offline probe
or archived candidate is part of the normal build.
