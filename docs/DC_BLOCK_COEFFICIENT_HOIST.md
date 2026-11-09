# Exact DC-blocker coefficient hoist

## Scope

This narrowly extracts the idea from historical commits
`1fa16fa0b3241f6839c386f374e498bb80fe9bc3` (test gate) and
`01f75f4b26774077b34ef64c1f8e016b82cc69db` (hoist), onto main
`a6569a079e4c8f40975ba041e34e513abe0560b8`. Their broad parent histories
are not merged.

The 10 Hz DC-blocker pole is constant for an internal sample rate. Previously,
the internal sample function evaluated its exponential every sample. It now
reads a coefficient computed by `configureInternalRate()`. The calculation
retains exactly the original cast, reciprocal, float constants, left-associated
multiplications, and `std::exp(float)` overload:

```cpp
const float sr = (float) internalRate;
const float invSr = 1.0f / sr;
const float coefficient = std::exp(-kTwoPi * 10.0f * invSr);
```

The sample recurrence, history variables, reset behavior, and audio routing
are unchanged. The default coefficient matches the default 96 kHz internal
rate before preparation. Every later assignment to `internalRate` occurs in
`configureInternalRate()`, including preparation and effective-factor changes.
The read-only test getter returns the stored coefficient, not a recomputation.

This removes redundant evaluation; it does not change the cutoff model or
claim a measured whole-plugin CPU gain. Runtime floating-point environment
changes between configuration and rendering are not qualified by this record.

## Verification on 2026-11-09

Linux, GCC 14.2.0. An independent source review checked arithmetic order and
rate/reset lifecycle and found no blocker.

Release CTest passed all four focused targets:

| Test | Checks | Failures |
| --- | ---: | ---: |
| Oversampling | 186 | 0 |
| Core | 11,508 | 0 |
| Parity mode | 128 | 0 |
| Source Performance | 485 | 0 |

The expanded oversampling test compares coefficient bits through default
construction, host-rate bounds and fractional rates, all three policies,
quality/factor changes, offline transitions, reset, invalid-rate fallback, and
render-thread control-frame changes. Its existing allocation-free transition
and audio/latency checks remain in place. A fresh `-O1` build with UBSan,
float-cast-overflow instrumentation, no sanitizer recovery, and libstdc++
assertions passed all 186 checks.

Two independent negative mutations were detected: removing the coefficient
refresh caused 84 failing checks; rewriting it in double precision caused
11 failing checks. Both processes returned failure. The fractional 8001.375 Hz
host-rate fixture is sensitive to the latter rewrite on this GCC/libm build.

The same raw-render probe was compiled against the pinned pre-change tree and
the candidate. It covers 27 non-silent combinations of 44.1/48/96 kHz, three
render policies and three quality modes. Each case includes note/controller
events, quality and offline changes, a policy switch, reset, re-prepare at a
different rate, and invalid-rate fallback. Both compiler profiles produced
byte-identical pre/post output over 101,574 stereo frames (812,592 bytes):

| Compiler profile | SHA-256 of each pre/post raw file |
| --- | --- |
| `-O2` | `c680bd3d76657ad79cb0ec3d15c1eb846a68c88a1c54cf03d571a3594737ee87` |
| `-O3 -march=native -ffp-contract=fast` | `6eb030bbeb62dcac4f9f2d46879fbaaffbf62c6ad672b578c7d38063afe02570` |

Different profiles need not match each other. Equality was required within
each pre/post pair. These are local software checks, not hardware calibration,
cross-platform, GUI, DAW-host, full-suite, or release certification.

## Reproduce

```sh
cmake -S . -B build-dc -DCS80_BUILD_PLUGIN=OFF \
  -DCS80_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-dc --parallel 2 --target cs80_oversampling_tests \
  cs80_tests cs80_parity_mode_tests cs80_source_performance_tests
ctest --test-dir build-dc --output-on-failure \
  -R '^(cs80_oversampling_tests|cs80_core_tests|cs80_parity_mode_tests|cs80_source_performance_tests)$'
```

For raw comparison, run from the source root with the pinned baseline available
in local Git history. Use the same compiler and flags for both executables:

```sh
baseline=a6569a079e4c8f40975ba041e34e513abe0560b8
work=$(mktemp -d)
mkdir "$work/baseline"
git archive "$baseline" src | tar -x -C "$work/baseline"
probe=tests/diagnostics/DcBlockCoefficientProbe.cpp
c++ -std=c++20 -O2 -I"$work/baseline/src" "$probe" \
  "$work/baseline/src/core/Engine.cpp" -o "$work/before"
c++ -std=c++20 -O2 -Isrc "$probe" src/core/Engine.cpp -o "$work/after"
"$work/before" "$work/before.f32"
"$work/after" "$work/after.f32"
cmp "$work/before.f32" "$work/after.f32"
sha256sum "$work/before.f32" "$work/after.f32"
```

The recorded results apply to the source hashes in
[`DC_BLOCK_COEFFICIENT_HOIST_RESULTS.json`](DC_BLOCK_COEFFICIENT_HOIST_RESULTS.json),
not arbitrary later changes. Unrelated future audio changes may correctly
differ from this historical baseline and need their own comparison source.
