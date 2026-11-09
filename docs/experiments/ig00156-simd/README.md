# Exact IG00156 batching qualification

**Status: opt-in, default OFF.** Numerical compatibility is qualified below;
throughput is workload-dependent, so this is not a default performance promotion.

The later [main-integration report](INTEGRATION.md) records the exact newer
baseline, unchanged screenshot scope and fresh merged-source checks. Results
below remain their original dated qualification, not new timing measurements.

Baseline: `9f89f8a` (current main when this worktree was created). This is an
optimization of the **existing discrete model**, not an improvement in analog
accuracy. It does not change the known continuous-model/top-octave deviations,
OTA assumptions, prewarp, sample rates, filter latency or reconstruction.

## Contract and ownership

`processIg00156Four` reads four actual scalar stage objects, performs the
same bounded Newton iteration, and writes all four histories, four node values
and two cached slopes back before returning. There is no second coefficient
cache, persistent packed state, sample queue or snapshot format. The ordinary
`set`, Q-only cache, reset, restore and re-reference remain authoritative.
`Ig00156Stage` has no layout change. Inputs are copied before updates; repeated
stage pointers use the original sequential scalar loop. Null stage pointers
remain a documented programming error. Partial groups are processed scalar. The build-dependent caller wrapper has
internal linkage, so mixed FMA/non-FMA translation units cannot interpose
inconsistent external inline definitions. Shared type/dispatcher declarations
stay identical, and fallback arithmetic remains in the caller's own context.

Each active lane stops at its original `abs(f) < 1e-7` test or iteration 4.
Completed lanes retain their final BP/HP/tanh values while other lanes continue.
Unused divisions get benign operands, and saturated tanh lanes do not evaluate
an unused overflowing polynomial. Arithmetic association and float conversion
match the original scalar stage. This is not a tolerance-based approximation.

## Conservative dispatch and fallback

There are three separate gates:

1. **Build contract:** x86-64 GCC only, ordinary binary64 evaluation, no baseline
   FMA target, fast-math, reassociation, reciprocal-math, no-signed-zero or
   finite-only assumptions. Clang/AppleClang and other compilers always retain
   scalar processing, even when the CMake option is ON. Clang does not expose
   several independently enabled unsafe-math flags through the GNU guard
   macros, so a nominally strict Clang build cannot be safely distinguished
   here. Both caller and backend fail closed, including mixed-compiler objects.
   This change does not override scalar floating-point flags to make identity
   tests pass. The isolated GCC backend alone disables contraction.
2. **CPU/OS:** setup-time compiler CPU detection selects the isolated AVX2
   function only if the processor and OS support it. There is no global AVX
   target flag, no audio-thread initialization/CPUID, and no allocation or lock.
3. **Current inputs/environment:** nearest MXCSR rounding and masked FP traps;
   finite bounded coefficients, inputs and histories; warped valid audio-rate
   coefficients; nonnegative OTA/feedforward gains and cached slopes in [0,1],
   with `xFromBp >= -1`. These sign conditions keep the Newton denominators
   positive. Unsupported values get the original scalar calculation, **not a
   clamp or replacement value**. FTZ/DAZ controls are unchanged and existing
   sticky flags are never cleared; arithmetic may raise status flags.

Eligibility ranges are deliberately wider than the tested physical stage
range: input magnitude <=64, histories <=1024, coefficient magnitudes <=1e6,
rate 8 kHz to 1.536 MHz. Those bounds also keep intermediates finite in the
positive-denominator regime. They do not claim that arbitrary custom networks
inside the bounds model a real instrument. Exceptional/custom networks outside
the supported regime fall back. Bitwise output/state compatibility is the
qualification target; identical sticky exception flags are **not claimed**.

`CS80_ENABLE_AVX2_FILTER_BATCH=OFF` is the default whole-build scalar path.
Explicit `-DCS80_ENABLE_AVX2_FILTER_BATCH=ON` enables the qualified candidate
only when the compiler and per-translation-unit arithmetic gates also allow it.
Disabled and unsupported builds retain the original scalar function shape,
not the template scheduler. The opt-in build emits separate scalar and packed
schedulers from one shared source body, eliminating the pending queue from
its optimized scalar instantiation. The
setup-only `Engine::setFilterBatchingEnabled(false)` selects the original scalar
scheduling path without resetting any circuit state. Neither is a sound/patch
parameter. A selected kernel does not mean every group is eligible.

## Engine scheduling

Only independent active-rank filters and the subsequent VCA/mix are deferred
within the current internal sample. Oscillators, random draws, controls, filter
and amplitude envelopes, modern-layer evaluation, and voice bookkeeping retain
their original order. Every enabled HP is completed before its own LP. Bypassed
stages remain untouched. The VCA and mix are then evaluated in the original
voice/rank order with unchanged multiplication and accumulation order.

The pending array is fixed at 16 ranks on the stack. It adds no samples of
latency and cannot survive a callback. A pack is attempted only if the number
of active voices and enabled ranks permits at least one complete four-stage
pack. Idle stepping and wake replay continue through the original scalar path.
The shared `prepareStageClock` is only an extraction of the existing scalar
set/re-reference sequence. Snapshot continuation still uses the same objects.

## Required qualification

- Exact finite output bits and every scalar Memory member, including cached
  slopes/half steps, across rate/Q/cutoff and lifecycle changes.
- Forced scalar, duplicate pointers, exceptional input/state/custom-network,
  rounding and FTZ/DAZ cases. Unsupported build contracts must execute no SIMD.
- Whole Engine comparison against the untouched baseline, not just a scalar
  branch of the edited binary; compare output and observable continuation state.
- Independent modern-layer, natural release/idle/wake, bypass, event and block
  partition checks. No fidelity claim is inferred from compatibility checks.
- Actual gather/scatter API and complete Engine timings, including sparse
  occupancy, bypass, moving controls, Legacy and disabled paths. Microbenchmark
  speedups do not establish a callback deadline or hard real-time safety.
- Baseline-ISA dispatcher inspection and executable compiler/non-AVX/non-x86
  validation where available. Unavailable platforms must be named explicitly.

## Results and limits

The ordinary build remains scalar because **a universal throughput improvement
was not established**. The opt-in kernel has no approximation/error allowance:
all finite audio/state comparisons below require equal bits. These checks do
not establish hardware parity or change the existing model's truth-grid misses.

- Stage suite: 1,126,360 checks, zero failures, 15,685 accelerated groups on the
  supported native GCC build. Earlier Clang accelerated-stage evidence is
  historical, before the conservative admission correction. All public Memory/half-step members are
  compared, not just audible output. Whole-build disabled/FMA tests execute
  zero accelerated groups. Two deliberate defects (lost history and ignoring
  per-lane convergence) fail 847,874 and 721,470 checks respectively.
- Independent scheduling suite: 869,465 checks across four Engine variants
  (scalar/SIMD, whole-block/partitioned), 135,454 timeline frames. It exercises
  real Source Performance bypass changes, natural envelope completion,
  idle/wake, cross-rank envelope/MSEG routing and external input.
- Whole Engine: 4,135,998 checks and 285,328 stereo timeline frames per run.
  The same harness linked to untouched main and the candidate emits 22,400,288
  bytes of audio/public continuation state. Same-environment pairs match
  exactly with GCC Release, GCC native/FMA fallback, Clang 19, Nehalem/no-AVX
  emulation, AArch64 emulation, and a whole-build disabled configuration.
  Cross-compiler/cross-libm hashes are not required to agree.
- ASan + UBSan + float-cast-overflow + library assertions pass the stage and
  complete Engine cases. LeakSanitizer is excluded in this execution setup.
  Twelve relevant core/regression CTest targets, including the new independent
  compiler-admission matrix, pass in the GCC opt-in build.
- GCC `-flto` stage qualification also passes. Baseline-ISA disassembly of the
  chooser/environment check contains no AVX, and the baseline-target Engine
  object contains no YMM references. CPU dispatch is tested by actual execution
  under a Nehalem CPU model, not just a forced-null pointer.

### Clang admission correction

Independent review of `bf3bb816` found a build-admission hole: Clang 19 does not
advertise independently enabled associative, reciprocal, or no-signed-zero
assumptions through the GCC macros used by the original guard. In the unsafe
profile the stage suite reported 485,972 mismatches with SIMD versus 110 with
it disabled. Reciprocal and no-signed-zero profiles also entered SIMD, but their
80/90 mismatches were present in the disabled controls too. Those latter counts
are **not** evidence of SIMD-only errors; all profiles still violated the
unsupported-build admission contract. Original results are not retroactively
relabeled as current qualification.

The correction admits GCC only and explicitly excludes Clang/AppleClang and
known GNU-compatible alternative compilers at the caller/backend header gate.
CMake also disables the optional path for any non-GNU compiler ID. No scalar
source flags or arithmetic are changed. This deliberately gives up even strict
Clang SIMD until a separately qualified build contract exists.

`tests/test_filter_batch_admission.py` separately compiles caller and backend
under 11 profiles: strict, isolated/effective association, reciprocal,
no-signed-zero, unsafe, fast, finite-only, Ofast, FMA, and no-trapping-only.
All 11 Clang profiles select no backend and execute no packed callback; all 11
also reject a real strict-GCC backend supplied from another translation unit.
GCC strict and no-trapping-only still accelerate, as does isolated association
which GCC explicitly ignores with a warning. All genuinely incompatible GCC
profiles remain scalar. Removing the Clang exclusion makes the negative control
fail five admission checks, so a silently scalar test cannot pass that control.
These are admission tests, not unsafe-math bitwise-equivalence claims.

An independent full-Engine comparison rebuilt untouched `9f89f8a`, current ON,
and current OFF with Clang 19, once with ordinary strict flags and once with
`-funsafe-math-optimizations`. All six runs pass 4,135,998 checks. Each profile's
three 22,400,288-byte audio/continuation corpora match exactly. Strict and unsafe
profiles have different hashes, as expected; no cross-profile identity is
claimed. The independent build flags confirm that Clang keeps its original
scalar options, without an added `-ffp-contract=off` override. See
`evidence/clang-admission/` for frozen commands, hashes and results.

The ordinary Clang stage gate also exposed a separate fixture defect: two
independently optimized literal `prepare()` paths produced starting coefficients
1–2 ULP apart before processing. A test-only non-inlined setup helper puts both
fixtures through one runtime path. Exact finite-bit assertions are unchanged;
all 1,142,908 Clang stage checks now pass with zero accelerated groups. The
ordinary Clang CI-shaped selection passes 4/4 tests. Extra GCC and Clang stage
runs use test-only `-frounding-math` to qualify the suite's dynamic rounding-mode
cases; passing an ordinary build alone is not proof of a dynamic-fenv compiler
contract. None of these test changes alter production flags or arithmetic.

### Measured CPU, not a universal budget

Measurements use GCC 14.2 Release on the shared x86-64 cloud host, 48 kHz,
Eco/2x internal rate, rotating baseline/scalar/packed order. CPU time and wall
time are retained, with identical checksums required. No timing threshold is
used as a flaky correctness gate. The nominal bypass flags are ineffective in
Isolation by design; evidence records the **effective** HP/LP rank count.

The stateless stage benchmark includes ordinary scalar coefficient updates,
eligibility, dispatch and gather/scatter. Median speedups across HP, LP and
mixed packs are 1.38–1.48x static, 1.26–1.42x moving Q, and 1.08–1.13x moving
cutoff+Q. This is smaller than the earlier vector-resident research prototype;
that prototype did not implement the same ownership/lifecycle API.

The retained timings precede the final linkage-only wrapper correction; they
are historical candidate measurements, not a post-correction budget. The
opt-in Engine's seven-repeat survey measures about 8–14% lower CPU for
eight-voice both-stage Source/Isolation cases. Longer nine-repeat confirmations
(196,608 host frames per timing) find 11.2% lower CPU for the eight-voice Source
case and 4.3% lower for four voices with only one enabled stage per voice. But
the no-op/idle/Legacy trials do **not** establish zero overhead: their medians
range from about +0.6% for one voice to +7.0% for idle in the retained longer
runs, with substantial shared-host scatter. A short survey even shows larger
swings in paths where no SIMD is executed. These results are the reason for
leaving the option **OFF**, not grounds to discard inconvenient samples.

The final disabled/unsupported build retains the original scalar function
rather than the opt-in template, and its numerical corpus is checked again.
There is no native no-AVX/ARM performance budget, physical CPU migration test,
AppleClang/MSVC SIMD qualification, plugin/DAW deadline guarantee, or complete
sticky-FP-exception-flag equivalence claim. Native platform CI and workload
budget qualification are still required before enabling this by default.

## Reproduce

From the repository root, with an installed CMake >=3.22 and C++20 compiler:

```sh
CMAKE=cmake CXX=c++ docs/experiments/ig00156-simd/reproduce.sh /tmp/new-batch-check
python3 docs/experiments/ig00156-simd/mutation-check.py /tmp/new-batch-mutants
python3 tests/test_filter_batch_admission.py --compiler g++
python3 tests/test_filter_batch_admission.py --compiler clang++
```

The first script was verified end-to-end in a fresh output directory. It builds
both original main and the explicitly enabled candidate
from local git history, runs compatibility checks, and prints optional timing
commands. Output directories must not already exist. The mutation check needs
a CPU/build where the backend genuinely executes. The new focused GitHub
workflow tests GCC/Clang with the build option both ON and OFF, including
compiler-admission profiles (Clang remains scalar in both configurations); it has not been
run remotely as part of this local, unpushed work.

See `evidence/` for exact logs, corpus hashes, source hashes, compiler/toolchain
provenance, raw timings and the independent review. Earlier integration-v1
measurements are retained separately where useful and are not mislabeled as
final qualification.
