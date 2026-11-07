# Exact control caching for modern Tape echo

Date: 2026-11-06. Baseline: `b5e0f1f271f0d189750233f7fde6c6cbf30b71d5`.
This is a modern software-effect performance change, not a hardware-model change.

## Scope

`TapeEchoFx::process` evaluated `delaySeconds(a)` and `tailSeconds(b)` on every
internal sample. Both are double-precision powers whose only input is the
corresponding float control. Their outputs are now retained while that exact
input is unchanged. At 96 kHz internal rate, one active tape slot avoids
192,000 power calls per second with held controls.

Each model owns its two keys and double results. NaN keys force the first
evaluation and are restored by reset/reprepare; there is no allocation or
shared cache. New controls take effect on that same sample. The original
mapping functions, clamp, division and floating-point expressions are retained.
The moving-time feedback-gain power still runs every sample, including during
the tape-speed glide. Delay memory, glide, wow/flutter, feedback, trim, output
filters, mix, and classic effects are unchanged. No parameter or state format
changes are made.

## Exact-output checks

`tests/TapeEchoUncachedReference.h` freezes the original TapeEchoFx class, with
only its class name and namespace changed. The Eq and DelayLine primitives are
shared; neither primitive is changed here. The comparison checks both float
bit patterns for every stereo frame, and separately requires finite and
non-silent output so startup silence cannot pass the suite by itself.

`cs80_tape_echo_control_cache_tests` passes 231 assertions over 21,845,986
stereo frames:

- Internal rates 88.2, 96, 176.4, 192, 384 and 768 kHz.
- Held controls, independent Time/Repeats steps, both controls changing every
  sample, adjacent Float32 values, single-sample excursions and a 5-by-5 held
  control grid.
- Clamp inputs including an initial -1, signed zero and infinities. NaN Time
  is not a supported delay-line input and is not passed through this test.
- Each primary case runs beyond the maximum 1.2-second delay and at least one
  circular-buffer wrap. A separate ten-second sequence covers long upward and
  downward glides, feedback and a silent-input tail across repeated wraps.
- Reset/reprepare with held controls, trim changes and independent instances.

The suite passes Release, native-CPU Release, and AddressSanitizer plus
UndefinedBehaviorSanitizer with standard-library assertions (leak detection
disabled). Independent review confirmed the frozen class matches the baseline
and repeated the optimized suite successfully. Lifecycle equivalence protects
the audio result; because the two mappings are rate-independent, it does not
by itself prove that reset invalidates a cache. The reset code does explicitly
invalidate both keys.

The full JUCE-free target build and 17 selected core/modulation/FX CTest suites
also pass, including the existing Tape echo time/glide and modern FX suites.
Strict compatibility passes: Legacy default/tails match `d56911a`, V1 circuit
numerics match `22d6a32`, and V2 circuit audio matches `00ceccf` byte-for-byte.

## Model-only timing

The opt-in `--benchmark` path uses 524,288 runtime-input stereo frames per
trial, five trials with alternating original/cached order, a no-inline process
boundary to prevent constant-control hoisting, and preparation/warmup outside
the measured interval. Both outputs contribute to the checksum; every trial
checks its checksum against the original. Timing is not a pass/fail gate.

Observed on this shared x86-64 container with GCC 14.2.0 Release:

| Internal rate | First run, held controls | Independent run, held controls |
| --- | --- | --- |
| 96 kHz | 91.761 to 79.536 ms, 13.3% saved | 12.3% saved |
| 384 kHz | 93.542 to 77.863 ms, 16.8% saved | 14.8% saved |

Fully sample-varying controls showed about 0–1.5% overhead/noise across the two
runs. Stepped controls generally benefited, but one independent stepped-Time
measurement was 5.9% slower, illustrating shared-host variability. These are
isolated TapeEchoFx results, not whole-plugin CPU measurements or a guarantee
that every automation pattern is faster. There is no benefit while Tape echo
is inactive. All benchmark trials retained matching output checksums.

## Reproduction

```sh
cmake -S . -B build-core -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCS80_BUILD_PLUGIN=OFF -DCS80_BUILD_TESTS=ON
cmake --build build-core --parallel
ctest --test-dir build-core --output-on-failure \
  -R '^cs80_(tape_echo_control_cache|tape_echo_time|modern_fx)_tests$'
build-core/cs80_tape_echo_control_cache_tests --benchmark

c++ -std=c++20 -O1 -g -fsanitize=address,undefined \
  -fno-omit-frame-pointer -D_GLIBCXX_ASSERTIONS -Isrc \
  tests/TapeEchoControlCacheTests.cpp -o /tmp/cs80-tape-cache-sanitized
ASAN_OPTIONS=detect_leaks=0 /tmp/cs80-tape-cache-sanitized
scripts/check-compat-gates.sh --strict-v2
```

No local plugin, GUI or DAW qualification is claimed. The existing unresolved
Linux validation discrepancy remains separate; this optimization neither
resolves nor waives it. Keep publication in draft pending required validation.
