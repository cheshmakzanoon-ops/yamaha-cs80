# Prepared oscillator profile constants: acceptance plan

Base: `87405eed4b11779cc52816599d5c093601fcac2e`.
Frozen baseline header: `tests/fixtures/ReconstructedOscillatorPreProfileCache.h`,
SHA-256 `c037a8970f939bdcd7d98ea56348006f82f2412cc64cd912fbf0f85b5ffa9373`.
This plan is committed before recording the paired timing results. The first
exploratory exact-comparison test already passed 325,913 checks / 160,490 ticks;
it was not a timing or whole-instrument acceptance result.

## Bounded implementation

Cache only profile/rate-only expressions using their original operation order:
maximum frequency, reset rate, triangle gain/half, shaper spacing/slopes, and
fold endpoints. No state/control decision, root search, event time, RC decay,
residual, frequency smoothing, idle catch-up, or dynamic-operand division is
approximated or held. No reciprocal-multiplication substitution is permitted.
The cache is per oscillator, updated after successful basis acquisition and
before reset, copied with its profile, and initialized from the actual default
members even if construction cannot acquire a basis.

## Correctness gates

1. Exact double-bit comparisons against the frozen baseline, including signed
   zero, every returned pin, every public continuation field/residual, and all
   diagnostic/event fields. Cover rates 8 kHz through 1.536 MHz, nine nominal and
   non-nominal profiles, moving/invalid controls, clamp/comparator neighbors,
   copy/assignment/self-assignment, reset/restore/idle transitions, same-basis
   profile changes, invalid prepare, genuine registry exhaustion, and the
   failed-constructor default APIs. No callback/idle allocations.
2. Run the comparison under GCC release generic and native-target flags, plus
   an available Clang release profile and sanitizers. Keep rounding stable at
   round-to-nearest; do not promise changed rounding modes or sticky exception
   flags. Per-profile derived cache values are finite under validProfile.
3. Run the unchanged oscillator numerical/promotion/continuity gates and strict
   Legacy/default-tail/Circuit V1/V2 compatibility. No baseline or tolerance
   changes may be used to admit the cache.
4. Mutation controls must detect omitted prepare refresh and copy propagation,
   including same-rate/RC profile changes. Compiler/crash failures are not
   accepted as behavioral fault detection.

## Equal-workload timing and decision

Use identical precomputed inputs and warm-up, alternating baseline/candidate
order over nine pairs per case. Benchmark 16 and 32 oscillators at 96 kHz
internal: held low/middle/high pitches, smooth pitch/PWM, dense changing
controls, and idle advancement. Both versions expose complete object state to
the compiler after each call, consume all returned pins, and report matching
checksums. Setup and input generation stay outside the timed interval. Archive
all rows, order, source/compiler identities and affinity; shared-machine timing
is not WCET, an audio-driver deadline guarantee, or a full scorecard refresh.

Retain only if all correctness gates pass and there is a useful repeated cost
signal: the median paired reduction across non-idle 16-oscillator cases must
be at least 3% in both GCC profiles (the pooled 45 pair reductions), with no individual case's median paired
regression exceeding 3% (including moving, 32-oscillator and idle cases).
If ambiguous/noisy, report the ambiguity or rejection; do not selectively drop
pairs. A longer rerun, if needed, must retain the original result and repeat the
entire matrix. Confirm the direction with bounded paired eight-voice Engine
workloads; report those separately from the component benchmark. The CPU and
idle scorecard goals remain open even if this increment is accepted.
