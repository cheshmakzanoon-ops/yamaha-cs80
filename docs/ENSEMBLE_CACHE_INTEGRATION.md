# Exact adaptive Ensemble cache: integration evidence

## Recommendation and scope

Do not merge #77 unchanged. Its sample-varying Rate/Depth benchmark was slower
in three local runs. This replacement retains its exact held-control cache but
uses the original arithmetic for 64 samples after each cache miss. It clears the
local repeated long-trial moving-control gate described below, and fresh exact
combined comparisons pass. No control is held, smoothed, decimated or approximated.

This is a modern software-effect optimization, not a Yamaha hardware-model
correction or a whole-instrument CPU/deadline qualification. The historical
Linux PR validator crash remains unresolved; fresh platform/native validation
is still required for integration. No validation or retry policy is weakened.

## Source and exact arithmetic

Baseline: main `0b340d9aa925141c50f2fd354c936163afe8a4f0`, already containing the
Drive/Tape caches. Starting source: #77 head
`d7dd23adc94eaddc72da6128aaee95800b33945d`. The frozen Ensemble class is unchanged
from that PR and exactly matches this baseline after its class rename. The
original regression scenarios remain, with adaptive transition tests added.

The cache key is the same clamped float consumed by the original double `exp2`
expression. Cached increments retain the original left-associated double
multiply/divide order. On a miss, both increments refresh before use, then the
next 64 samples each execute the original expression using **that sample's**
Rate. At the next probe, an equal old key is safe because increments depend only
on Rate and unchanged sample rate, not on intervening controls. A different key
refreshes both values and starts another bounded fallback window.

Both phases advance/wrap every sample, including every fallback sample. Depth,
trigonometry, delay interpolation, gain and audio history are unchanged. Reset
clears the nonnegative bounded counter and invalidates the key; prepare changes
sample rate before resetting. Signed-zero key equality is safe for the original
expression. NaN remains unequal; NaN controls remain outside the tested contract.

Independent review approved both the original exact cache and this adaptive
lifecycle/arithmetic. It is not independent CPU certification.

## Fresh verification, 2026-11-09

Linux GCC 14.2.0. Eight focused Release CTests passed: core (11,508 checks), parity
(128), Source Performance (485), modulation matrix (4,239), modern FX (445),
Drive cache (396), Ensemble cache (3,160) and Tape cache (231). After strengthening
the test-only reprepare setup, the exact final Ensemble target was rebuilt and
passed again. No production source changed after the eight-test run.

The Ensemble suite compares **14,566,394 stereo frames by bit pattern**. Added
cases alternate moving controls and holds of 1/2/31/63/64/65/66/127/1024 samples
at five rates, and reset/reprepare at six positions within or beside the fallback
window. Every reprepare fixture begins at its declared original rate.
A fresh ASan + UBSan + float-cast-overflow build, assertions enabled and sanitizer
recovery disabled, passed all 3,160 checks. Local leak detection was disabled.

The committed mutation script passes its baseline and detects each isolated
incorrect variant as an ordinary test failure, not a compile error or crash:

| Mutation | Failing checks |
| --- | ---: |
| Remove reset key invalidation | 300 |
| Refresh Rate only on first use | 370 |
| Hold cached increments during the uncached fallback | 370 |

`tests/diagnostics/EnsembleCacheProbe.cpp` combines Ensemble with already-cached
Drive/Tape, real slot mixing/loudness matching, duplicate Ensemble instances,
bypass/type/quality changes, moving controls, reset, rate reprepare, offline
transitions and release tails. Nine serial-slot cases cover 3,676,176 stereo
frames at 88.2/96/192 kHz. Twenty-seven Engine cases cover 442,368 frames across
44.1/48/96 kHz, all three policies and three effect orders. All are finite and
non-silent. The 32,948,352 bytes are identical before/after within each profile:

| Profile | SHA-256 of both raw files |
| --- | --- |
| `-O2` | `499d33f638620c859123800e64ebeed39a8bf5f8e6b1abb3630331ecf64c5092` |
| `-O3 -march=native -ffp-contract=fast` | `4ddc2a48ae700e672cdf2697d3f861622016f0dc5bc7e162bfc53c4168bdeada` |

Profiles need not match each other. The strict compatibility script also passed:
Legacy default/tails versus `d56911a`, V1 circuit numerics versus `22d6a32`, and
V2 audio versus `00ceccf`. These are local combined core checks, not full plugin,
GUI, DAW or cross-platform qualification.

## Moving-control performance gate

Before adaptive evaluation, the stated gate was no repeated moving-control
slowdown above 2% in either compiler profile, plus useful held-control savings.
The saved notes are in [ENSEMBLE_CACHE_ADAPTIVE_GATES.md](ENSEMBLE_CACHE_ADAPTIVE_GATES.md).
No correctness tolerance was relaxed to obtain a speed result.

The unchanged #77 cache took **+6.79%, +3.50%, +8.82%** more time for fully
sample-varying Rate/Depth in short generic/generic-repeat/native measurements.
A speed-only cache was also rejected (+5.03% generic), as was a shared-arithmetic
fallback layout (+6.81% generic). The selected 64-sample fallback's first short
generic observation was +2.50%, so that observation alone did **not** clear the
gate; its short native run was noisy and -5.83%.

Trial length was then increased eightfold, from 524,288 to 4,194,304 precomputed
stereo frames at 96 kHz, keeping seven alternating-order trials, unchanged
scenarios and the 2% gate. Two exploratory long repeats were +0.63%/-0.42%
(generic) and +0.96%/-1.00% (native) for moving controls. The final committed
benchmark driver was then rebuilt and rerun twice per profile, after adding the
transition regressions. Final median time changes are below; **positive is slower**.

| Controls | Generic 1 | Generic 2 | Native 1 | Native 2 |
| --- | ---: | ---: | ---: | ---: |
| Held Rate/Depth | -9.52% | -8.37% | -15.55% | -15.78% |
| Stepped controls | -10.74% | -14.63% | -14.58% | -18.06% |
| Both controls vary each model sample | +1.18% | -0.42% | -6.55% | -2.50% |
| Held Rate, sample-varying Depth | -10.01% | -7.39% | -12.64% | -16.36% |

All four final runs passed 3,188 checks, including output checksums for every
benchmark trial. Setup and output hashing are outside the timed processing loop.
Runs were sequential with CPU-0 affinity on a shared machine, not an exclusive
reserved core. Final profiles are `-O3 -DNDEBUG` and
`-O3 -march=native -ffp-contract=fast -DNDEBUG`.

These longer repeated observations clear the chosen local moving-control gate;
they do not prove zero overhead on every platform or every automation sequence.
The initial short unfavorable result is retained, not silently discarded. The
Engine normally holds FX A/B between parameter updates, so the direct-model
per-sample stress is also not the same as ordinary host automation. Savings are
not additive with other optimizations. Full trial output, including rejected
and exploratory runs, is retained in
[ENSEMBLE_CACHE_TIMING.txt](ENSEMBLE_CACHE_TIMING.txt).

## Historical CI: different failure causes

* [Intel macOS push job 128134063846](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/42288096594/job/128134063846)
  failed during CMake Configure, before candidate compilation/tests. JUCE's
  clone retried connection failures twice and then failed DNS resolution for
  `github.com`. This is supported fetch-infrastructure evidence.
* [Linux PR job 128147986326](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/42288153169/job/128147986326)
  completed pluginval 1.0.4 tests through Fuzz parameters and printed SUCCESS,
  then exited **139**. The wrapper correctly refused a post-test retry. No local
  ELF core was available, so the cause cannot be assigned to Ensemble, pluginval,
  or teardown from this log. It is not a passed validator or a download failure.
  This was the PR synthetic merge `77a73e9` of `d7dd23a` into `a53c1a8`; the
  separate branch-push Linux validation passed. That difference does not erase
  the failed attempt or establish its cause.

Exact excerpts are preserved in
[`ENSEMBLE_CACHE_CI_FAILURES.txt`](ENSEMBLE_CACHE_CI_FAILURES.txt). No workflow,
retry, compatibility, or validator policy is changed by this candidate.

## Reproduce

```sh
cmake -S . -B build-ensemble -DCS80_BUILD_PLUGIN=OFF \
  -DCS80_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-ensemble --parallel 2 --target cs80_ensemble_rate_cache_tests \
  cs80_drive_control_cache_tests cs80_tape_echo_control_cache_tests \
  cs80_modern_fx_tests cs80_mod_matrix_tests cs80_tests \
  cs80_parity_mode_tests cs80_source_performance_tests
ctest --test-dir build-ensemble --output-on-failure \
  -R '^(cs80_ensemble_rate_cache_tests|cs80_drive_control_cache_tests|cs80_tape_echo_control_cache_tests|cs80_modern_fx_tests|cs80_mod_matrix_tests|cs80_core_tests|cs80_parity_mode_tests|cs80_source_performance_tests)$'
bash scripts/check-compat-gates.sh --strict-v2
python3 scripts/test-ensemble-cache-mutations.py /tmp/new-ensemble-mutations

# Seven longer alternating trials per scenario. Repeat twice per profile.
g++ -std=c++20 -O3 -DNDEBUG -Isrc tests/EnsembleRateCacheTests.cpp -o /tmp/ensemble-generic
g++ -std=c++20 -O3 -march=native -ffp-contract=fast -DNDEBUG -Isrc \
  tests/EnsembleRateCacheTests.cpp -o /tmp/ensemble-native
taskset -c 0 /tmp/ensemble-generic --benchmark-long
taskset -c 0 /tmp/ensemble-native --benchmark-long
```

For raw comparison, materialize `git archive
0b340d9aa925141c50f2fd354c936163afe8a4f0 src` into a separate directory. Compile
the same `tests/diagnostics/EnsembleCacheProbe.cpp` with that directory's `src`
include path and `src/core/Engine.cpp`, and once with current source. Use identical
flags and separate raw output filenames, then `cmp` and `sha256sum`. The command
pattern is the same as [the Drive/Tape comparison](DRIVE_TAPE_CACHE_INTEGRATION.md#reproduce).

Source hashes and machine-readable outcomes are in
[ENSEMBLE_CACHE_RESULTS.json](ENSEMBLE_CACHE_RESULTS.json). Dated results apply
to those inputs, not arbitrary later audio changes.
