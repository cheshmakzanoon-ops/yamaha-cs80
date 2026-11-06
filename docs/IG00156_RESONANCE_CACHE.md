# IG00156 resonance-only coefficient cache

## Scope

This is a CPU optimization of the existing circuit-engine filter equations,
not a new circuit law or hardware measurement. The baseline is main
`b5e0f1f271f0d189750233f7fde6c6cbf30b71d5`.

`Ig00156Stage::set()` already returns immediately when every control is
unchanged. Previously, changing only resonance Q rebuilt the cutoff calibration,
prewarped capacitor steps and linear network coefficients. Those calculations
include a square root, a tangent approximation and multiple divisions, despite
their independence from Q. Modern matrix HP/LP resonance routes can change Q
every internal sample while cutoff remains fixed.

The added cache retains cutoff-dependent intermediates. A Q-only change updates
Q conductance and the original `xFromBp` expression. Cutoff, sample rate or
coupling-clock changes still take the full original calculation path. The
Newton solve, saturation, filter histories and audio output arithmetic are
unchanged. No allocation, lock or shared mutable cache is introduced.

The first call and `restoreMemory()` invalidate the partial cache. Copies keep
their own cache, and ordinary `reset()` continues to clear audio history without
changing coefficients. The shortcut requires a finite positive sample rate.
This preserves the old full-update behavior for exceptional clocks, including
the different signs of the unwarped half steps at positive and negative zero.

## Reproduction

The standalone test has no JUCE dependency:

```sh
cmake -S . -B build-core -DCMAKE_BUILD_TYPE=Release \
  -DCS80_BUILD_PLUGIN=OFF -DCS80_BUILD_TESTS=ON
cmake --build build-core --target cs80_ig00156_cache_tests
ctest --test-dir build-core -R '^cs80_ig00156_cache_tests$' --output-on-failure
build-core/cs80_ig00156_cache_tests --benchmark
```

Sanitizer reproduction (leak checking is disabled for the restricted runner):

```sh
c++ -O1 -g -std=c++20 -D_GLIBCXX_ASSERTIONS -fsanitize=address,undefined \
  -Isrc tests/Ig00156CacheTests.cpp -o /tmp/q-cache-sanitized
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 /tmp/q-cache-sanitized
```

The frozen class in `tests/Ig00156StageBaseline.h` is copied from the baseline
with only its class name changed. It shares the unchanged network definitions,
cutoff helper and math utilities. The comparison covers default, HP, LP and
custom networks; held, Q-only, cutoff-only and simultaneous controls; rate and
coupling-clock changes; clamps and adjacent Float32 controls; reset, copies,
assignment, `setState()`, `reReference()` and `restoreMemory()`; separate
instances and recovery from exceptional controls.

For normal scenarios, every output, public state, node, memory field, half step
and Q conductance is compared by floating-point bit pattern. Outputs must also
be finite and nontrivial. Exceptional-control cases compare finite values and
infinities exactly, but separately count both-NaN results without demanding a
NaN payload or sign. Floating-point exception flags and changes to the active
rounding mode are outside this cache comparison contract.

The regression uses symmetric, test-only non-inlined call wrappers. With native
FMA instructions, GCC can optimize adjacent paired calls differently even
when both classes are the unchanged baseline. That baseline-versus-baseline
control reproduced the original harness differences. The wrappers remove
that asymmetric optimization context without changing production arithmetic
or compiler flags. Benchmark calls remain directly inlined.

An additional whole-core witness, `tests/Ig00156QCacheEngineProbe.cpp`, writes
audio and public filter histories for the same-toolchain comparison. Compile
the same source against each revision's own headers and core library. This is
important because the optimization changes the internal class layout.

```sh
# Build baseline and candidate cs80_core libraries separately first.
c++ -O3 -std=c++20 -I../baseline/src tests/Ig00156QCacheEngineProbe.cpp \
  ../baseline/build-core/libcs80_core.a -o /tmp/q-cache-baseline
c++ -O3 -std=c++20 -Isrc tests/Ig00156QCacheEngineProbe.cpp \
  build-core/libcs80_core.a -o /tmp/q-cache-candidate
/tmp/q-cache-baseline /tmp/q-cache-baseline.bin
/tmp/q-cache-candidate /tmp/q-cache-candidate.bin
cmp /tmp/q-cache-baseline.bin /tmp/q-cache-candidate.bin
```

The witness spans 44.1, 48, 96 and 192 kHz host rates, all three quality settings
and Legacy, circuit and PURE CS-80 policies. It exercises changing HP/LP
resonance routes, cutoff/resonance edits, variable host blocks, idle circuits,
re-entry, quality changes, reset and release. It requires finite, non-silent
audio and observes live resonance modulation in the circuit-policy cases.

## Qualification limits

The benchmark measures one filter's `set()` plus `process()` calls, including
output checksumming, on deterministic precomputed inputs. It alternates old/new
execution order, reports seven-trial medians and verifies every trial checksum.
Timing is advisory, never a pass/fail gate. This is not a whole-plugin CPU
measurement; simultaneous cutoff modulation reduces cache reuse, and unchanged
controls already use the original early return.

## Recorded results, 2026-11-06

- GCC 14.2, `-O2` and `-O3 -march=native`: 20,364,865 exact scalar
  comparisons over 385,223 samples, plus 96,654 separately counted
  exceptional both-NaN checks, all passing.
- ASan/UBSan with library assertions passes, with leak detection disabled.
  Independent review repeated native, baseline-versus-baseline and sanitizer
  runs and found no remaining blocking issue.
- The Engine witness passes all 36 rate/quality/policy combinations, including
  12 observed circuit-policy resonance-modulation cases. Its 331,992 stereo
  frames and public filter histories match the baseline byte-for-byte in both
  Release and native-CPU builds, compared within each matching toolchain.
- The full JUCE-free target build and 20 selected core/filter/modulation CTest
  suites pass. The strict compatibility gates pass: Legacy default/tails match
  `d56911a`, V1 circuit numerics match `22d6a32`, and V2 circuit-engine audio
  matches `00ceccf`, all byte-for-byte.
- On the shared AMD EPYC 9V74 runner, one seven-trial native benchmark measured
  Q-only `set()+process()` at 97.39 ns before versus 63.34 ns after, about
  **35% less model time**. Updating cutoff every 2, 4 or 16 samples while Q
  changes every sample reduced median time by about 16%, 28% and 33%.
  Fixed controls, cutoff-only changes and both-controls-per-sample were
  approximately unchanged to 1.5% slower. Every trial's output hash matched.
  A separate ordinary Release run measured Q-only at 126.25 versus 78.88 ns
  (37.5% lower), but cutoff-only at 121.11 versus 128.09 ns (5.8% slower).
  Fixed controls and simultaneous changes were approximately unchanged in
  that run. Cache misses add bookkeeping, and timing is noisy on this shared
  runner. These observations are workload- and machine-specific, not
  guaranteed speedups or whole-instrument CPU figures.

No local plugin, GUI or DAW qualification is claimed. Keep the change in draft
while required CI and the existing Linux validation discrepancy remain
unresolved. This optimization does not diagnose or waive that discrepancy.
