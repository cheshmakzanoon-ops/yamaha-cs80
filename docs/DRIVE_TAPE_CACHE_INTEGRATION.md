# Combined modern Drive and Tape control caches

## Source and scope

This integrates the narrow production changes from #73
(`542674580a1dc9283359e39a1c01434c9ed1712a`) and #75
(`cac8c5440a92da8512d880af34ad1a6f81780769`) onto main
`6e2c171fa8a7ed6fc91e1a1153f78b226860e71c`. Their existing test files are
retained unchanged. This is not a merge of broad historical branches and does
not include the Ensemble cache from #77.

The Drive gain/tone laws and Tape delay/tail laws are cached by exact raw input
values. Their original float/double expressions are unchanged. Each model owns
its cache; reset/prepare invalidates it. Drive's rate-dependent tone coefficient
is therefore recomputed after preparation. Tape's moving delay and its feedback
gain still update each sample. There is no control-rate approximation, changed
smoothing, common/shared cache, allocation, parameter ID, or state-format change.

Independent source review confirmed that stages and audio history are not
inputs to the cached laws, and that signed-zero equality is safe for these
positive-base powers. NaN keys miss; this does not make NaN Time a supported
delay-line input. Existing finite-input/clamp behavior is preserved.

These are modern software-effect optimizations, not Yamaha hardware corrections.
The classic effects and PURE CS-80 policy are unchanged. No whole-instrument
CPU target, additive savings with other optimizations, or release qualification
is claimed.

## Combined verification, 2026-11-09

Linux, GCC 14.2.0. All nine focused Release CTests passed on the combined tree:

| Test | Checks | Failures |
| --- | ---: | ---: |
| Core | 11,508 | 0 |
| Parity mode | 128 | 0 |
| Source Performance | 485 | 0 |
| Modulation matrix | 4,239 | 0 |
| Modern FX | 445 | 0 |
| Delay line | 1,125 | 0 |
| Tape time/glide | 104 | 0 |
| Drive control cache | 396 | 0 |
| Tape control cache | 231 | 0 |

The unchanged Tape regression compares 21,845,986 stereo frames by bit pattern,
including audible delayed/feedback output, buffer wraps, moving controls,
reset/reprepare, trim and independent instances. Its frozen Tape class exactly
matches the integration baseline after the class rename. The Drive reference's
original formulas and processing order were independently inspected.

Both cache tests also passed fresh ASan + UBSan + float-cast-overflow builds
with libstdc++ assertions and no sanitizer recovery. Local leak detection was
disabled; this record does not claim a LeakSanitizer pass.

Separate stale-key mutations prove the tests detect missed updates: making
Drive refresh its gain only on first use fails 145/396 checks; making Tape
refresh its target only when the key is NaN fails 31/231 checks. Both return
failure. The unmodified combined tests pass. These mutations were applied only
to temporary header copies, not committed production code.

### Serial-slot and whole-Engine comparison

`tests/diagnostics/DriveTapeCacheProbe.cpp` is compiled unchanged against the
pinned pre-change source and the candidate. It covers:

* Six serial-slot cases, 2,450,784 stereo frames: both model orders, duplicate
  model instances, 88.2/96/192 kHz, loudness matching, mix/bypass, live type and
  oversampling changes, reset, rate reprepare, held and per-sample controls,
  feedback tails and runs longer than the maximum tape delay/buffer wrap.
* Eighteen Engine cases, 294,912 stereo frames: 44.1/48/96 kHz, Legacy/Pure/
  Source Performance, both model orders, control changes, offline transitions,
  reset, reprepare and note release. Every case is finite and non-silent.

All 21,965,568 raw output bytes match within each compiler profile:

| Profile | SHA-256 of both pre/post files |
| --- | --- |
| `-O2` | `7db4c2948c65cf880f5f54dec7853ebf4f0875a220aca5f13583d7ef2ae769b8` |
| `-O3 -march=native -ffp-contract=fast` | `3132625dab6b23b52eba89dcbd0dcf2af6e938650ef4ff01297c1f9e65cfa0ed` |

The two compiler profiles need not match each other. The same-profile pre/post
pair must match. `scripts/check-compat-gates.sh --strict-v2` also passed:
Legacy default and tails versus `d56911a`, V1 circuit numerics versus `22d6a32`,
and V2 circuit audio versus `00ceccf` were byte-identical.

## Timing, not an acceptance threshold

The existing opt-in model probes were run sequentially with CPU-0 affinity on
this shared Linux machine. Alternating original/cached order, warmup outside
timing, and checksum agreement are retained. This is not an exclusive-core
measurement, worst-case deadline analysis, or whole-plugin benchmark.

Observed held-control savings were 44.2%, 3.2%, and 2.4% for Drive at 1x, 4x,
and 8x, respectively. Tape held controls saved 15.8% at 96 kHz and 16.0% at
384 kHz. When controls change every sample there is no avoided law evaluation:
Drive 1x was 1.3% slower and Tape at 384 kHz was 0.6% slower in this run.
Other varying-control measurements were within about 1.5% of the original.
Do not advertise a uniform speedup. Full probe output is preserved in
[`DRIVE_TAPE_CACHE_TIMING.txt`](DRIVE_TAPE_CACHE_TIMING.txt).

## Validation still separate

Historical exact-head push runs for #73 (`42218446793`) and #75
(`42236308737`) failed Linux `Validate VST3 headless`, despite green PR build
and DSP runs. Focused local correctness results do not erase those failures
or establish their root causes. The combined candidate still needs current
native/plugin/GUI/host CI; no such pass is claimed here.

## Reproduce

```sh
cmake -S . -B build-fx -DCS80_BUILD_PLUGIN=OFF \
  -DCS80_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-fx --parallel 2 --target cs80_drive_control_cache_tests \
  cs80_tape_echo_control_cache_tests cs80_modern_fx_tests \
  cs80_tape_echo_time_tests cs80_delay_line_tests cs80_mod_matrix_tests \
  cs80_tests cs80_parity_mode_tests cs80_source_performance_tests
ctest --test-dir build-fx --output-on-failure \
  -R '^(cs80_drive_control_cache_tests|cs80_tape_echo_control_cache_tests|cs80_modern_fx_tests|cs80_tape_echo_time_tests|cs80_delay_line_tests|cs80_mod_matrix_tests|cs80_core_tests|cs80_parity_mode_tests|cs80_source_performance_tests)$'
bash scripts/check-compat-gates.sh --strict-v2
```

For the raw comparison, from the source root with the pinned baseline available:

```sh
baseline=6e2c171fa8a7ed6fc91e1a1153f78b226860e71c
work=$(mktemp -d)
mkdir "$work/baseline"
git archive "$baseline" src | tar -x -C "$work/baseline"
probe=tests/diagnostics/DriveTapeCacheProbe.cpp
c++ -std=c++20 -O2 -I"$work/baseline/src" "$probe" \
  "$work/baseline/src/core/Engine.cpp" -o "$work/before"
c++ -std=c++20 -O2 -Isrc "$probe" src/core/Engine.cpp -o "$work/after"
"$work/before" "$work/before.f32"
"$work/after" "$work/after.f32"
cmp "$work/before.f32" "$work/after.f32"
sha256sum "$work/before.f32" "$work/after.f32"
```

For sanitizer runs, compile each cache test with `-std=c++20 -O1 -g -Isrc
-fsanitize=address,undefined,float-cast-overflow -fno-sanitize-recover=all
-fno-omit-frame-pointer -D_GLIBCXX_ASSERTIONS`. The optional model timing is
`build-fx/cs80_drive_control_cache_tests --benchmark` and
`build-fx/cs80_tape_echo_control_cache_tests --benchmark`; timing never decides
test success. Source identities and machine-readable outcomes are in
[`DRIVE_TAPE_CACHE_RESULTS.json`](DRIVE_TAPE_CACHE_RESULTS.json). These dated
results do not qualify unrelated future audio changes against this old baseline.
