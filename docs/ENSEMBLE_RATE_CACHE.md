# Modern Ensemble rate cache

## Scope

The modern BBD ensemble previously calculated its rate `exp2` and two LFO phase
increments for every stereo sample. The Rate control normally stays fixed for
many samples. The model now reuses those two double-precision increments until
the clamped Rate value changes. The original expressions and operation order are
retained, and a change takes effect on that sample. Both phases still advance,
wrap and drive the same trigonometry on every sample.

`reset()` invalidates the cache; `prepare()` changes the sample rate and then
calls `reset()`. This matters when Rate holds through a quality or sample-rate
change. The implementation adds 16 bytes to an Ensemble instance on the tested
x86-64 build (208 to 224), with no process-time allocation or synchronization.

Depth, delay interpolation, filters, saturation, trim and mixing are unchanged.
No parameter, preset or state-format change is involved. This is a modern-effect
optimization, not a claim about CS-80 circuitry or measured hardware fidelity.

## Exact-output evidence

`tests/EnsembleFxBaseline.h` freezes the Ensemble class from main
`a53c1a8b196d494dedd9f0438be173c1b0ecd28d`, changing only its class name.
The regression compares both output channels by Float32 bit pattern. It also
requires finite, non-silent output for every main scenario.

- Six rates: 44.1, 48, 88.2, 96, 192 and 384 kHz.
- The full 5 x 5 held Rate/Depth grid at each rate.
- Stepped and per-sample control automation, changing Depth with held Rate,
  adjacent Float32 controls, signed zeros, clamping and infinite controls.
- Long phase wraps and silent-input tails, with changing trim.
- Reset and every one of the 36 directed sample-rate pairs, explicitly keeping
  Rate fixed through each reprepare.
- Independently interleaved instances at different rates and controls.

Final regression: **865 checks, 0 failures; 11,962,739 stereo frames**. This passes
GCC 14.2 Release, `-O3 -march=native`, and ASan/UBSan with standard-library
assertions. Leak detection is disabled because it is unsupported in this
execution environment. An independent review verified the frozen class and
repeated the optimized test; no blocking issue was found. A deliberately broken
copy with cache invalidation removed fails all 30 rate-changing reprepare cases.

The full JUCE-free build and 16 selected core, modulation and FX suites pass,
including existing modern FX, plate, tape and external interpolation tests.
The expanded Ensemble suite was rerun after its final coverage change. Strict
compatibility also passes: Legacy default/tails against `d56911a`, V1 circuit
numerics against `22d6a32`, and V2 circuit-engine audio against `00ceccf` are all
byte-identical.

These comparisons establish identity for the tested builds and inputs, not all
possible compilers or floating-point settings. Processing requires `prepare()`
first, as in the existing model. NaN controls are outside the tested contract.

## Reproduce

```sh
cmake -S . -B build-core -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCS80_BUILD_PLUGIN=OFF -DCS80_BUILD_TESTS=ON
cmake --build build-core --parallel
ctest --test-dir build-core --output-on-failure -R cs80_ensemble_rate_cache_tests
./build-core/cs80_ensemble_rate_cache_tests --benchmark

g++ -std=c++20 -O1 -g -fsanitize=address,undefined \
  -fno-omit-frame-pointer -D_GLIBCXX_ASSERTIONS -Isrc \
  tests/EnsembleRateCacheTests.cpp -o /tmp/ensemble-cache-test
ASAN_OPTIONS=detect_leaks=0 /tmp/ensemble-cache-test

scripts/check-compat-gates.sh --strict-v2
```

## Timing method and qualification

The opt-in benchmark alternates baseline/cached execution order over seven
trials per scenario, after warming each model. It processes 524,288 precomputed
stereo frames per trial at 96 kHz; input generation, allocation and hashing are
outside the timed section. Every trial checks the entire output checksum. Mode
0 holds both controls, mode 1 steps them, mode 2 changes both each sample, and
mode 3 changes Depth each sample while Rate holds. Timing is diagnostic, never
a CI pass/fail condition.

Two seven-trial runs on this x86-64 environment with GCC 14.2 produced these
median times (milliseconds; baseline -> cached):

| Controls | Release | `-O3 -march=native` |
| --- | --- | --- |
| Held Rate and Depth | 70.560 -> 59.915 (-15.1%) | 59.522 -> 52.217 (-12.3%) |
| Stepped controls | 67.135 -> 59.697 (-11.1%) | 59.151 -> 51.946 (-12.2%) |
| Both controls change every sample | 69.310 -> 71.010 (+2.5%) | 61.436 -> 60.759 (-1.1%) |
| Held Rate, per-sample Depth | 68.811 -> 59.130 (-14.1%) | 61.060 -> 53.488 (-12.4%) |

Both benchmark runs finish with 893 checks and zero failures, including all
28 trial checksums. Native and generic builds need not match each other: the
requirement is baseline/cached identity within each build. These timings are
noisy shared-machine observations. The held-Rate saving is consistent, while
fully sample-varying Rate offers no demonstrated speedup and may add overhead.

This does not establish whole-plugin CPU savings, plugin/GUI/DAW qualification,
or native ARM qualification. Required checks must pass for the exact published
commit before merge; this optimization does not resolve or waive existing
validation discrepancies. No workflow or validation policy is changed.
