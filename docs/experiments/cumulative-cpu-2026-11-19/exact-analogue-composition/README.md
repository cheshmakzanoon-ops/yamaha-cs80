# Exact analogue readout, pressure and stereo-work reductions

* A source-selection transaction caches only bounded integer descriptor
  provenance. The active solvers still check their per-sample drives and
  do the original calculations. The ordinary entry remains checked.
* The physical touch bus reuses the diode evaluation only when the relevant
  input words agree. Keep the same per-key summation order; multiplying one
  term by eight was rejected by negative tests. Mixed and unqualified inputs
  retain their original ordered evaluation.
* The strict GCC 14.2 stereo wah shares identical optical/rate/profile terms
  within a render. Each channel retains its own capacitor history and signal.
  Directed FP, unmasked faults and unsupported builds use the original scalar
  path; a lost state/proof check or cross-fed capacitor fails regression tests.

Both full dependent local Linux VST3 rebuilds passed native byte comparisons
and the 22-case private interruption/continuation replay, including seven
genuine faults on each compiler. The ten native public histories and four
audible crossover histories matched captured audio, FP, state, events and
latency, including simultaneous aftertouch, live mapped CC11 wah and source
changes. GCC 13 does not admit the stereo wah specialization; its source
descriptor fast path does not admit GCC 14 packed arithmetic.

## Work counts (no elapsed/CPU time)

The [GCC 14 incremental receipt](gcc14-increment.json) clocks no samples.
It counts −1.621% / −0.769% / −1.124% / −0.713% guest instructions in fresh
silence, equal held, varied held and varied wake. Reads and writes decrease
in all four. In the [expressive controls](expressive-work.json),
eight-note common-pressure cases save 3.010–3.332% guest instructions.
Distinct/two-group pressures also improve in the **assembled build**;
the full change is not attributable to pressure alone.

The [GCC 13 cumulative-from-b88 receipt](gcc13-from-fir.json) includes
the queue and grouped FIR work following b88, not just the analogue changes.
All seven cases use fewer guest instructions; held reads rise about
0.92–0.97% and wake reads about 1.21%, with lower writes. An isolated
rapid-selector control attributing the map proof alone increases bounded setup
cost for 32 factory switches with zero rendered frames (about +0.92%/+1.19%
guest instructions on GCC 14/13). That is not an audio callback timing result.

The [live-pedal/dry controls](pedal-work.json) retained a tradeoff:
GCC 14 Source wah and dry used about 1.12% fewer guest instructions, but Legacy
moving-wah and dry used 0.304% and 0.402% more guest instructions and roughly
2.1% more guest writes than the immediate predecessor. These are composition
results, not a per-change attribution or a timing conclusion. The connected
pedal was deliberately verified to produce different wet/fixed control audio,
and each paired build still matched exactly.

## Direct original-to-current results

Actual product baseline db02cd8 vs the exact composed GCC 14 Release ELF
`7e52c1d6b3af9f4061818070335861d7d535f6be540204ac2ff29b5d9a8e9a98`.

| 48 kHz Standard | Fresh quiet Ir | Eight varied held Ir |
| --- | ---: | ---: |
| 16 frames | −41.236% | −15.344% |
| 32 frames | −41.687% | −15.010% |
| 64 frames | −41.935% | −14.839% |

[64-frame](original-64.json) and [16/32-frame](original-16-32.json) direct
receipts retain repeated and independently additive work controls.
Eight equal held notes at 64 improved 15.221% and varied wake 11.584%.
Guest reads and writes are lower than original in all eight tested cells.
Direct native captures matched public bytes between these two builds. At
48 kHz Standard the plugin still reports 67 samples, about 1.40 ms algorithmic
delay, not DAW round-trip. None of these data verifies the absolute physical
CPU-use goals.
