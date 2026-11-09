# Exact pitch-conversion memo

Baseline: main `36581cabdb69a377ae9b02f05349371e088cdb52`. This change is
independent of the separate zero-EI VCA proposal (#91); their timings are not
additive and a combined build needs its own qualification.

## Scope

The oscillator frequency path evaluates `tuningRef * pow(2.0f, (pitch - 69)/12)`
for each rank. Many successive float pitches are identical, even while the
underlying double-precision drift state evolves. `PitchPowMemo` retains the
exact `std::pow` result for the exact exponent. It does **not** replace `pow`
with `exp2`, approximate or quantize pitches, change any circuit law, or move
the tuning-reference multiplication. NaN exponents always recompute; both
signed-zero exponents yield one. Every rank owns its own two-float memo.
No allocations, locks or shared mutable cache are added. Rate, policy, reset,
copy and route changes need no invalidation because the cached result depends
only on the exponent, which is checked on each memo use.

An ungated experiment improved held-pitch timing but regressed the active-LFO
control by about 1.3% median. The final version bypasses the memo for active
pitch LFO, SUB-VCO/aftertouch depth, active mod-wheel-to-SUB depth, or nonzero
matrix pitch offset. The shared gate is computed once per internal sample;
the matrix check remains per rank. This is a conservative workload heuristic,
not an exhaustive detector of changing pitch: glide, initial bend, controller
smoothing and drift can still change the argument. Both branches calculate the
same function, so gate changes and zero crossings do not change the sound.

## Exact-output checks

`PitchPowMemoTests.cpp` compares the original `midiToHz` and direct `std::pow`
against the memo, including repeated/random pitches, changing tuning reference,
signed zeros, subnormals, adjacent overflow/underflow boundaries, arbitrary float
bit patterns, NaNs/infinities, copies and independent instances. GCC 14.2.0 UBSan
and the registered CMake test passed **1,133,091 bitwise checks**. Independent
review separately repeated the unit and boundary checks.

```sh
c++ -O2 -std=c++20 -fsanitize=undefined -fno-sanitize-recover=all \
  -D_GLIBCXX_ASSERTIONS -Isrc tests/PitchPowMemoTests.cpp -o /tmp/pitch-memo
/tmp/pitch-memo
```

The public, JUCE-free `PitchPowEngineProbe.cpp` is compiled against the baseline
and candidate with identical `-O3` options. It covers all three policies, four
rates, three qualities, quality transitions, note lifecycle, external input,
master tuning/reference, ribbon, bend, portamento-parameter changes and active
pitch LFO. It toggles SUB, aftertouch-SUB, mod-wheel-SUB and matrix pitch routes,
exercising the final direct/memo gate transitions.

145,584 stereo frames were finite and byte-identical on the x86-64 runner:
`fe256fa9a3fc8fe9c355f116620b4ee4b03c9bf3cf995af04a58b0cefc53bfc1`.
This is a run-specific hash, not a portable golden file.

```sh
# Run in each source tree, keeping distinct executable/output filenames.
c++ -O3 -std=c++20 -Isrc tests/diagnostics/PitchPowEngineProbe.cpp \
  src/core/Engine.cpp src/core/FactoryPresets.cpp -o /tmp/pitch-probe
/tmp/pitch-probe /tmp/pitch-output.bin
# cmp BASELINE_OUTPUT CANDIDATE_OUTPUT
```

No processor, GUI, validator or DAW runtime was exercised in this focused work.
Normal platform CI is a separate gate, and no tolerance is relaxed.

## Bounded performance evidence

`PitchPowBenchmark.cpp` renders 65,536 frames per timed round after 8,192 warm-up
frames, 48 kHz, 256-frame blocks, eight held voices, Standard quality. Arguments:
policy (0 Legacy, 1 Circuit, 2 PURE), voice count, SUB-VCA depth, pitch-LFO depth.
The benchmark explicitly selects LFO destination **1 (pitch)**, not 0 (off).
It uses a 20 Hz LFO for the active-modulation control.

GCC 14.2.0 `-O3`, shared x86-64 Linux runner, CPU affinity 2, no concurrent
compilation. Circuit rows use B/A/A/B/A/B/B/A process order with nine rounds each
(36 timings per revision). Legacy/PURE use B/A/A/B (18 each). Raw timings are
under `artifacts/pitch-pow-memo/`.

| Policy / pitch modulation | Baseline median seconds | Candidate median seconds | Observed change |
| --- | ---: | ---: | ---: |
| Circuit / held pitch | 0.699441 | 0.667644 | -4.55% |
| Circuit / LFO depth 0.7 at 20 Hz | 0.915116 | 0.915745 | +0.07% |
| Legacy / held pitch | 0.267245 | 0.245517 | -8.13% |
| PURE CS-80 / held pitch | 0.724998 | 0.716697 | -1.14% |

The active-LFO difference is small relative to run variation, not proof of zero
cost. Timings vary substantially on this shared machine; these observations
are not a universal speed guarantee, DAW qualification, or proof that the
0.15–0.20x realtime / idle CPU targets are met. The exact-output checks are the
stronger compatibility evidence. The earlier mistaken destination-0 timing
trial is excluded from this table and its active-modulation claim was withdrawn.
