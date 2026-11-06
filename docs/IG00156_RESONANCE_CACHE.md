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

The compact cache reuses the existing `kHalf`, `kHalfCoupling`, `gmPart` and
`ffPart` fields. A Q-only change updates Q conductance and runs the original
linear elimination in a common path, while skipping cutoff calibration and
capacitor half-step calculation. Cutoff, sample rate or coupling-clock changes
still take the full original calculation path. The Newton solve, saturation,
filter histories and audio output arithmetic are unchanged. No allocation,
lock or shared mutable cache is introduced. The validity flag fits in the
original trailing padding; the regression asserts unchanged stage size.

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
important even though the compact cache preserves the baseline class size.

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

The standalone `--benchmark` mode measures one filter's `set()` plus `process()` calls, including
output checksumming, on deterministic precomputed inputs. It alternates old/new
execution order, reports seven-trial medians and verifies every trial checksum.
Timing is advisory, never a pass/fail gate. This is not a whole-plugin CPU
measurement; simultaneous cutoff modulation reduces cache reuse, and unchanged
controls already use the original early return.

## Historical initial revision results, 2026-11-06

These measurements qualify the initial four-double cache at
`aeea8da315f78e0005e8f0289efda9843fadb81e`, not the compact follow-up.
That revision is preserved in the PR history. Its isolated gains did not
predict all whole-engine workloads; see the follow-up below.

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

## Compact-cache follow-up

An independent whole-core evaluation reproduced a 3.27% median slowdown for
initial PR #76 alone in PURE CS-80 with external input. That workload uses the
original external sub-oscillator source, with pitch/amplitude modulation but
no filter cutoff modulation. Modern effects and resonance-matrix routes are
masked by PURE policy. Instrumenting only branch counts found 5,242,848 full-key
cache hits, zero partial hits and 32 full updates across warmup plus rendering.
Repeated partial-update arithmetic therefore cannot explain this regression.

The initial cache increased `Ig00156Stage` from 296 to 336 bytes and `Engine`
from 284,488 to 285,768 bytes on the tested x86-64 ABI. GCC also changed its
inlining of unchanged oscillator code: `ReconstructedOscillator::tick()` grew
from 14,842 to 17,523 bytes. The compact cache restores both original object
sizes and the original tick code size with the tested compiler flags. These
are concrete layout/code-generation differences, not proof that a particular
cache miss or inlining choice accounts for the entire timing change. A padded
baseline control and alternate check ordering did not isolate a single cause
reliably on the shared runner.

Two clean, sequential 11-round balanced comparisons on the AMD EPYC 9V74 used
GCC 14.2 with `-O3 -DNDEBUG -std=gnu++20 -fPIC`, pinned to permitted CPU 8.
Each round randomized the order of main, initial PR #76 and compact versions,
then ran the reverse order. Each fresh engine warmed up 16,384 stereo frames
and timed 65,536 more at 48 kHz, in 256-frame blocks. Allocation, preparation,
input generation and output/state checksumming were outside the timed region.
Reported percentages are medians of within-round CPU-time ratios, using the
mean of each version's two symmetric observations per round. Positive means
less CPU time than main.

| Workload | Initial cache, run 1 / run 2 | Compact cache, run 1 / run 2 |
| --- | --- | --- |
| Circuit, 8 voices, HP/LP resonance matrix | +13.26% / +12.45% | +13.41% / +11.67% |
| PURE, 8 voices, external input | -1.69% / -1.82% | +1.06% / +1.33% |

Compact versus initial-cache PURE savings were +1.92% and +2.04%, calculated
directly from each round's mean CPU times, not by subtracting main-relative
median percentages. All measured
versions produced identical audio/state checksums. Maximum wall/CPU ratios in
these two batches were below 1.002. Individual pairs still varied substantially
and sometimes changed sign; small percentages are not a universal speedup or
a claim of zero regression on other CPUs/toolchains. The repeat supports
removing the added state and retaining the Q-heavy benefit on this workload.

A separate clean seven-round control batch measured compact/main paired
medians of +1.89% for default eight-voice rendering, +1.56% with external input
and +0.70% idle. Individual ranges crossed zero, so these controls do not
establish general gains. Every checksum matched.

### Reproduce whole-core comparisons

The committed `tests/FilterCachePerformanceProbe.cpp` preserves the independent
evaluation's workload source. The script compiles it separately against each
checkout, records source/compiler identities, pins every process, alternates
symmetric orders and checks every output/state checksum. Use separate worktrees
for main `b5e0f1f`, initial PR revision `aeea8da` and the compact candidate.
Do not run other builds or benchmarks concurrently. Choose a CPU allowed by
`os.sched_getaffinity(0)` on the current machine.

```sh
python3 scripts/benchmark-filter-cache.py \
  --revision main=../baseline --revision initial=../initial-cache \
  --revision compact=. --cpu 8 --trials 11 --scenario 4 --scenario 9 \
  --output-dir ../filter-cache-measurement-1
# Repeat into a new directory, then test controls without overlap:
python3 scripts/benchmark-filter-cache.py \
  --revision main=../baseline --revision initial=../initial-cache \
  --revision compact=. --cpu 8 --trials 7 \
  --scenario 0 --scenario 2 --scenario 10 \
  --output-dir ../filter-cache-controls
```

The output directory must be new. Raw observations include both wall and CPU
time; inspect their ratio for scheduling interference rather than relying on
a single timing number. No machine-dependent timing threshold is a test gate.
The probe also supports `witness OUTPUT.bin` for a 271,872-stereo-frame lifecycle
capture, including effect order, reset, quality/rate changes and external-input
disconnection/reconnection. Compare the capture files between revisions built
with the same toolchain.

Recorded per-round means, pairwise summaries, source hashes and timing
provenance are in [IG00156_RESONANCE_CACHE_RESULTS.json](IG00156_RESONANCE_CACHE_RESULTS.json).
The measured compact header differs from the final header only in its
explanatory comment; the real-script smoke run rebuilt the final header.

### Compact follow-up correctness

The final compact header passed independent GCC 14.2 `-O2`, `-O3 -march=native`
and ASan/UBSan/library-assertion runs: each completed 20,364,865 exact scalar
comparisons over 385,223 samples and 96,654 exceptional both-NaN checks. The
stage-size assertion passed at 296 bytes for both baseline and candidate.
All 20 selected JUCE-free core/filter/modulation CTest suites passed.

The 36-case, 331,992-stereo-frame Engine witness matches main byte-for-byte in
ordinary Release and native builds, within matching toolchains. The separate
271,872-frame lifecycle witness also matches exactly. Strict Legacy
default/tails, V1 circuit numerics and V2 circuit audio gates remain
byte-identical to their pinned baselines. The final script passed a real
build/run smoke test and deterministic checks of its metadata, paired summary
math and rejection of mismatched, invalid or nonpositive-duration results.

These local core results do not resolve the existing Linux CI validation
discrepancy. The PR remains draft and unmerged pending that qualification.
