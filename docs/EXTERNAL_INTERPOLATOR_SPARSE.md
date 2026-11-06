# Sparse external-modulation reconstruction

## Scope

This is a numerical performance change to the host-rate mono external modulation
input, not a new circuit model or a sound change. It does useful work only while
that input is connected. The coefficient design, input sanitisation, gain,
phase order, fractional reconstruction delay and Engine output latency stay
unchanged. No output decimator, modern FX, modulation routing or host wrapper
is changed.

The previous implementation at `b5e0f1f` inserted `N - 1` zero samples after each
host input and evaluated the full `128N - 1` tap FIR at every internal sample.
At 8x that requires 4,096 coefficient products per host frame, even though most
history entries are known zeros. The new implementation stores 128 host samples
in a mirrored fixed-size ring and visits only contributing symmetric tap pairs.
It has 128, 384 and 896 potentially nonzero coefficient products at 2x, 4x and
8x respectively (plus the zero center initialization in other phases).
There are no render-thread allocations.

## Arithmetic and boundaries

For phase `p`, delayed internal tap `t` contains an input only if `t % N == p`.
Its host lookback is `(t - p) / N`. A symmetric pair at `i` and `128N - 2 - i`
therefore contributes at residues `p` or `(N - 2 - p) % N`. Their union is visited
in ascending `i` order, with coinciding residues visited once. Lookbacks are
always 0 through 127, including ring wraps. The center tap contributes only at
phase `N - 1`, with host lookback 63.

The sum still starts with the center and adds the original symmetric pairs in
order. This matters: reordering a conventional polyphase dot product could
change rounding. Only guaranteed-zero products are omitted. A final double
addition of positive zero retains the full FIR's exact-zero sign without
changing a negative nonzero result that rounds to float negative zero.

Prepare uses the unchanged `OversamplingDecimator` coefficient generator. Reset,
connection transitions, repeated connection calls, invalid-factor fallback to
2x, and default-before-prepare behavior retain their prior contracts. The first
`getFactor()` frame entries contain outputs; unused entries remain positive zero.

## Verification

`tests/ExternalModulationInterpolatorTests.cpp` contains the frozen old
zero-insertion implementation and compares complete frames by float bit pattern.
It checks 1,331,712 frames across 44.1/48/96/192 kHz, all three factors, impulses,
steps, chirps, random signals and float bit patterns, clamps, nonfinite inputs,
signed zeros, subnormals, history wraps, lifecycle changes and separate instances.

On Linux x86-64, GCC 14.2:

- Release: 337 checks passed. The `-march=native` build also passed all 337.
- AddressSanitizer + UndefinedBehaviorSanitizer + `_GLIBCXX_ASSERTIONS`: all 337
  passed, with leak detection disabled. Independent review repeated the
  sanitizer run and independently checked every phase's indices and tap order.
- The full JUCE-free target build passed. Broader core checks and the pinned
  compatibility gates are recorded in the PR results.

The separate `tests/diagnostics/ExternalInterpolationEngineProbe.cpp` uses the
public core Engine, not a plugin processor. Compiling it against `b5e0f1f` and
this revision gives byte-identical output: 145,584 finite stereo frames across
36 rate/policy/quality combinations, with variable block sizes, notes, external
input, disconnect/reconnect, input clamps and live factor changes. Both files
have SHA-256 `388cd626f7a1d30eb7a5dc6d8079e161e72f6e50e86046b2a45f75327df08d0d`.
Compile against each revision's matching headers and Release core library:

```sh
c++ -std=c++20 -O2 -I "$tree/src" \
  tests/diagnostics/ExternalInterpolationEngineProbe.cpp \
  "$build/libcs80_core.a" -o "$probe"
"$probe" "$capture"
# Compare the two captures with cmp.
```

## Reproduce the standalone check and measurement

```sh
cmake -S . -B build-core -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCS80_BUILD_PLUGIN=OFF -DCS80_BUILD_TESTS=ON
cmake --build build-core --target cs80_external_interpolator_tests
ctest --test-dir build-core -R '^cs80_external_interpolator_tests$' --output-on-failure
./build-core/cs80_external_interpolator_tests --benchmark

c++ -std=c++20 -O1 -g -I src -fsanitize=address,undefined \
  -fno-omit-frame-pointer -D_GLIBCXX_ASSERTIONS \
  tests/ExternalModulationInterpolatorTests.cpp -o /tmp/cs80-ext-sanitized
ASAN_OPTIONS=detect_leaks=0 /tmp/cs80-ext-sanitized
```

The optional benchmark alternates execution order for five trials per factor,
uses 262,144 host frames each time, consumes every output phase and requires
identical checksums. It does not impose a timing pass/fail threshold. An initial
`-O3` run measured these ranges:

| Factor | Full FIR | Sparse FIR | Speedup |
| --- | --- | --- | --- |
| 2x | 54.8–59.5 ms | 43.9–46.7 ms | 1.25–1.31x |
| 4x | 222.6–236.9 ms | 99.2–102.2 ms | 2.18–2.38x |
| 8x | 913.3–997.8 ms | 199.2–261.1 ms | 3.81–4.90x |

These are interpolation-model measurements on one shared Linux machine, not
whole-plugin CPU savings or guarantees on other CPUs. Plugin, GUI, DAW and
release qualification are separate. This change does not resolve or waive the
existing Linux validation discrepancy; the PR must remain draft and unmerged
while required validation is unresolved.
