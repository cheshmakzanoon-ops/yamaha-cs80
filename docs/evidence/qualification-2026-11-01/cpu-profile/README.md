# Unchanged qualified core: instruction profile and next CPU candidate

This profiles production source at
`d06648c386486cf2630de6e41195ddd5d4360ff0`, before changing another CPU kernel.
Its `src/core` contents are identical to the published 1.5 kHz idle checkpoint
`18400c29a54ac2204564b355c0e52ffd3b0dcb8e`. The complete source hash manifest,
compiler flags, fixture drivers and Callgrind output are retained here.

These are **instrumented instruction counts**, simulated cache observations and
branch observations. They are not native CPU time percentages, RTF measurements,
hardware audio measurements, or a speedup result. User-mode native `perf`
sampling was denied by the environment; the error is preserved and no kernel
policy was changed.

## Method and exact scope

GCC 13.3, C++20, Release `-O3 -DNDEBUG`, plus debug symbols for attribution. No
fast-math, inlining change, altered numerical option or production source probe
was used. The temporary driver includes the stock factory benchmark source and
reuses its exact `makeFrame()` and note array. It prepares the original engine,
warms for the stock one-second interval, and profiles 128 callbacks at 48 kHz
with 256 frames. Held selector 0 has the stock eight notes; idle has no notes.
The Standard circuit oversampling rate is 96 kHz.

Callgrind instrumentation starts during the final 32 warmup callbacks while
collection remains disabled. Each measured callback collects only
`setControlFrame`, controller events and `render`; fixture arithmetic, audio
checks and reporting stay outside collection. Voice count, policy, source route
and finite output are checked. The retained driver and source hashes identify
which driver variant belongs to each capture.

The held-case selection was fixed from the three previously published 1.5 kHz
idle checkpoint CSVs: selector 0 plus the minimum/maximum mean of their three
per-case held medians, with lower-index tie breaking. Selector 0 was also the
minimum; selector 10 was the maximum. Once selector 0 identified the two large
sounding kernels, work was reprioritized to the unresolved idle question.
Selector 10 was interrupted and the planned moving profile was not run. The
partial selector-10 files are retained separately and are **not qualified
profile results**. No completed observation was discarded or relabelled.

The stock quiet idle case is Pure CS-80. Pure forces the analog-noise overlay to
zero, so a nominally noisy Pure fixture would be vacuous. The additional noisy
idle case therefore uses Source Performance, the same stock frame generator,
`analogNoise=0.12`, no voice notes, and disabled modern effects. It is explicitly
a supplemental circuit case. Comparing its total with Pure is not an isolated
noise-on/off experiment because their policy work also differs.

## Completed results

Counts in the rows below are inclusive within the named function or debug source
location. Nested rows overlap and must not be added as disjoint time shares.
Cross-header inline arithmetic can be attributed to its header location rather
than a separate runtime function.

| Scope | Held selector 0 | Pure quiet idle | Source Performance noisy idle |
|---|---:|---:|---:|
| Total recorded instructions | 2,877,600,053 | 535,065,489 | 581,809,343 |
| `tickRanks` | 82.53% | 11.99% | 11.05% |
| WSC `tick` | 27.32% | — | — |
| IG00156 corrected `process` | 24.89% | — | — |
| Complete filter `clockStage` path | 29.92% | 2.68% | 2.46% |
| `tickGlobals` | 7.49% | 43.72% | 43.64% |
| `updateSourceReadouts` | 3.03% | 20.03% | 18.42% |
| `mixBus` | 4.49% | 14.79% | 18.19% |
| Decimator code in `Dsp.h` | 4.17% | 22.40% | 20.60% |
| BBD including helper calls | 2.11% | 6.37% | 10.44% |
| PRA including helper calls | 1.14% | 1.84% | 1.69% |
| Noise-source inline call paths in `NoiseSource.h` | 0.62% | 3.32% | 3.05% |

In quiet idle, the retained FIR's three loop/arithmetic source lines account
for 117,112,832 instructions, 21.89% of the total. The 22.40% family above also
includes its history writes and output setup. That identifies substantial work
which an exact-zero-history shortcut can avoid when its complete admission
conditions hold; it is not a promised 21.89% native speedup.

In the held capture, the WSC next-branch search is 1.19% inclusive, while the
whole oscillator tick is 27.32%. The published event cache removed the repeated
search as the dominant remaining oscillator task. The corrected filter sample
path and oscillator together remain major sounding costs. Filter column-layout
or other local speedups must retain the frozen scalar state/FP contract, and
cannot by themselves establish the requested whole-engine 0.15–0.20 RTF.

## Decisions

The next candidate is the already-preregistered exact zero-history FIR shortcut,
including its runtime standard-rounding and independent MXCSR guards. This
preserves the original ordered convolution whenever history, zero signs or the
arithmetic environment do not establish the shortcut. Its full continuation,
subnormal ownership, mutation, compatibility and stock timing gates still have
to pass before adoption.

The already-preregistered dormant Gaussian state continuation remains deferred.
Its retained component package is substantial, but it intentionally changes the
post-dormancy seeded realization and adds conditional state machinery. The fresh
profile identifies a much smaller instruction share than the FIR. The active
hiss case also remains a real consumer that may not be suppressed. This is a
priority decision from the measured code path, not a claim that the deferred
candidate is qualified or cannot improve native timing.

The source-readout clocks account for another material idle share. They must
continue the common-bus envelope and touch state correctly; the existing cache
already avoids solving unchanged memoryless networks. Further optimization
needs an exact continuation/work contract that retains those clocked states.
No control clock, nonzero tail, signed-zero case or tolerance was removed here.

## Reproduction and retained evidence

Build the unchanged source with debug symbols added to Release, link the driver
against that exact `cs80_core`, and use the captured command JSON. For example,
from the repository root:

```sh
cmake -S . -B build-profile -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_FLAGS=-g -DCS80_BUILD_PLUGIN=OFF -DCS80_BUILD_TESTS=ON
cmake --build build-profile --target cs80_core
c++ -std=c++20 -O3 -g -DNDEBUG -I . -I src \
  docs/evidence/qualification-2026-11-01/cpu-profile/engine_profile_idle.cpp \
  build-profile/libcs80_core.a -o /tmp/cs80_idle_profile
valgrind --tool=callgrind --instr-atstart=no --collect-atstart=no \
  --cache-sim=yes --branch-sim=yes --callgrind-out-file=/tmp/cs80_idle.callgrind \
  /tmp/cs80_idle_profile idle_quiet 128
```

The `.callgrind.1.gz` files contain the completed client-request capture. The
small unsuffixed `.callgrind.gz` files contain the subsequent exit dump. Text
annotations and logs are also retained. `sha256.json` seals this evidence set.
The CPU scorecard remains the separate stock native timing record; neither
held nor idle target is closed by this profile.
