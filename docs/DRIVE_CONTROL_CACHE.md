# Modern Drive control-law cache

## Scope

The modern Drive model previously evaluated two powers and one exponential
on every stereo sample to derive its gain and tone coefficient. These values
depend on the Drive/Tone controls and the prepared sample rate, not the signal
or filter history. Normal held controls need not repeat those evaluations.

`DriveFx` now caches gain and tone independently. Raw control comparisons
retain the existing clamp/expression arithmetic. A changed control takes effect
on its first sample, without a control-rate interval or smoothing change.
`reset()` invalidates both entries, including when called by `prepare()` after
a sample-rate change. Each effect instance owns its own cache.

This changes no shaping, oversampling, DC-blocking, tone-filter, loudness,
mixing or classic-effect law. It is a modern-effect CPU optimization, not a
hardware-fidelity claim or a claim that the whole-instrument CPU target is met.

## Exact-output regression

`tests/DriveControlCacheTests.cpp` freezes the pre-cache Drive implementation
from `b5e0f1f`. Its control laws and sample processing deliberately remain
uncached, independent of the production cache. The test compares both output
channels bit-for-bit, not through a numerical tolerance.

Coverage includes:

- 16, 88.2, 96, 192, 384 and 1536 kHz internal rates;
- all four 1x/2x/4x/8x per-slot oversampling settings and live factor changes;
- held controls, independent Drive/Tone steps, endpoints, neighboring Float32
  values and simultaneous per-sample ramps;
- finite out-of-range controls and signed-zero transitions, retaining the
  public model's existing clamp behavior;
- default-state processing, reset, reprepare with unchanged controls, and
  independent effect instances.

These are direct model tests. They do not instantiate a processor, GUI, host,
socket service or validator.

## Reproducible timing probe

The same executable accepts `--benchmark`. It compares the frozen and cached
models at 96 kHz, with 262144 stereo frames per trial after warmup, seven
alternating-order trials and median wall time. Inputs and control trajectories
are prepared before timing. A no-inline call boundary represents runtime
controls and prevents loop-invariant compiler hoisting from concealing the
original cost. Both outputs contribute to verified equal checksums.

Example:

```sh
cmake -S . -B build-core -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCS80_BUILD_PLUGIN=OFF -DCS80_BUILD_TESTS=ON
cmake --build build-core --target cs80_drive_control_cache_tests
ctest --test-dir build-core -R '^cs80_drive_control_cache_tests$' --output-on-failure
./build-core/cs80_drive_control_cache_tests --benchmark
```

On a shared Linux cloud CPU with GCC 14.2, a separate single-CPU-affinity
benchmark of the same candidate against the pinned original measured held-
control reductions of 53.3%, 11.0%, 6.5% and 4.4% for 1x/2x/4x/8x respectively.
It compared 3,932,160 stereo frames bit-for-bit across five rates and lifecycle
changes. The added regression executable's unpinned run measured 43.3%, 4.6%
and 0.9% at 1x/4x/8x, illustrating why timings are observations rather than a
portable test threshold.

A final single-CPU run of the added executable measured 51.5% and 3.4% savings
at 1x/4x, but a 2.9% slowdown at 8x while other verification work was running.
That high-oversampling result does not support a reliable 8x speedup. The
consistent measured benefit is strongest at 1x and smaller at the default 4x.

When both controls change on every sample, the cache cannot save evaluations:
the separate benchmark measured about 1.8–2.8% overhead at 1x–4x. Normal held
or block-stepped controls are the intended optimization. These timings exclude
the loudness follower and the rest of the synth; they do not establish whole-
plugin savings. Timing is opt-in and is never a pass/fail CI requirement.

## Verification of this change

- All JUCE-free targets built successfully.
- Thirteen selected core/modulation/FX CTest suites passed. The final expanded
  Drive suite separately passed all 396 checks in Release and ASan/UBSan builds
  (leak detection disabled), and all 438 checks with the optional benchmark.
- Strict compatibility retained byte-identical Legacy default/tail audio versus
  `d56911a`, V1 circuit numerics versus `22d6a32`, and V2 audio versus `00ceccf`.
- Independent review found no correctness blocker. Local execution used GCC on
  Linux; no local plugin, GUI, DAW or cross-compiler qualification is claimed.
