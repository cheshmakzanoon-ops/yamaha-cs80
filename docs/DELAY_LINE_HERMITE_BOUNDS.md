# Minimum-buffer Hermite delay reads

## Scope

`DelayLine::prepare()` permits a four-sample buffer. Before this correction,
`readHermite()` passed `[2, size - 3]`, or `[2, 1]` for that buffer, to
`std::clamp`. Reversed bounds violate the standard-library precondition.
An assertion-enabled libstdc++ build aborted on a prepared zero-duration
delay's first Hermite read.

The upper bound is now `max(2, size - 3)`. A four-sample buffer uses the
two-sample tap, where all four Hermite neighbours fit. Buffer capacities,
linear reads, integer taps, and Hermite arithmetic for capacities of five
or more stay unchanged. Callers must still prepare the delay before use.

This is utility boundary hardening. No shipping FX caller has been identified
that requests such a small buffer, and this is not a claim of an audible
normal-playback correction or improved hardware fidelity.

## Regression evidence

- A minimal four-sample probe against base `b5e0f1f` aborted with exit 134
  under `_GLIBCXX_ASSERTIONS`. The corrected probe passed.
- The expanded `DelayLineTests.cpp` failed 16 of 112 checks against the
  unchanged base implementation without assertions, then passed all 112
  with the correction.
- Coverage includes four-to-six-sample capacities, finite clamp endpoints,
  repeated ring wraps, reset, and bit-exact comparison with the previous
  Hermite arithmetic for every tested capacity with valid old bounds.
- Direct GCC 14 C++20 runs passed with `_GLIBCXX_ASSERTIONS`, and with
  AddressSanitizer plus UndefinedBehaviorSanitizer (leak detection disabled).
- The full JUCE-free target build completed. The delay-line, plate-reverb-rate
  and tape-echo-time CTest suites passed. This focused run is not a full
  plugin, GUI or DAW qualification.

Reproduce the focused checks with:

```sh
cmake -S . -B build-core -DCMAKE_BUILD_TYPE=Release -DCS80_BUILD_PLUGIN=OFF -DCS80_BUILD_TESTS=ON
cmake --build build-core --parallel
ctest --test-dir build-core --output-on-failure -R '^cs80_(delay_line|plate_reverb_rate|tape_echo_time)_tests$'
c++ -std=c++20 -O2 -D_GLIBCXX_ASSERTIONS -Isrc tests/DelayLineTests.cpp -o /tmp/cs80-delay-line-tests
/tmp/cs80-delay-line-tests
```
