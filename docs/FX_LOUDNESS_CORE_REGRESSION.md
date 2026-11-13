# Focused ordinary-signal loudness matcher regression

Status: independently source-reviewed, then compiled and executed once on Linux
with GCC 14.2.0: **42/42 assertions passed**, exit 0, on 2026-11-13.
Six terminal errors were at most 2.8113067429558214e-12 against the frozen 1e-6
bound. Source hashes were unchanged after execution. See the retained
[result and identities](experiments/fx-loudness-core/result.json) and
[complete output](experiments/fx-loudness-core/run.log).
CMake registration was source-reviewed, not configured/executed in this run.
This header-only test includes `core/FxLoudness.h`, not Engine, plugin code or
FX model implementations. It adds a maintained `cs80_fx_loudness_core_tests`
CMake/CTest target within the existing core-tests configuration. No broad
ModernFxTests suite is required for this bounded check.

## Why this regression

Existing ModernFxTests covers K-weighting reference coefficients/frequency
response, full-effect static/matched loudness, enable transitions and broader
model stability. This fixture covers direct LoudnessMatcher behavior with
ordinary finite deterministic signals, exposing reset and rate reprepare
regressions without plugin, effect-model or calibration dependencies.

The header's ballistic comment is corrected: the input/output energy gate limits
**target updates**, while gain continues its 150 ms slew even after the gate
closes. No executable DSP code or numeric constants change. This fixture does
not separately qualify gate-closed recurrence or precise threshold crossings.

## Fixed tests and bounds

At each of 44.1, 48 and 96 kHz, 14 assertions (42 total):

1. One second of fresh silence retains exact gain 1. One second of identical
   stereo input/output also retains exact gain 1.
2. Four seconds of deterministic input with left amplitude 0.1 at 1 kHz and right
   amplitude 0.07 at 733 Hz; output is multiplied by exactly representable 0.5
   or 2.0. Gains must remain finite and monotonic between unity and the expected
   reciprocal target (2 or 0.5), within a 1e-12 arithmetic allowance. Terminal
   absolute gain error must be <=1e-6.
3. During each proportional-signal case, a separate matcher receives exchanged
   stereo channels. Returned float and internal public `currentGain()` double
   sequences must compare exactly equal.
4. Establish non-neutral gain for 0.25 seconds at output scale 0.5, require its
   difference from 1 >0.1, then reset. Immediate gain must equal 1 and a new
   0.25-second history at output scale 2 must exactly match a fresh instance.
5. Require that history again establishes gain differing from 1 by >0.1; prepare
   the reused matcher at the next rate (44.1->48->96->44.1 kHz). A 0.25-second
   history must exactly match a fresh instance prepared at that next rate.

The reciprocal targets follow linear K-weighting applied to exact power-of-two
amplitude scaling; there is no independent standard-coefficient or calibrated
loudness oracle here. Four seconds is over 26 of the existing 150 ms slew time
constants, with constant signals well above the input floor. The terminal 1e-6
bound is fixed before execution, not fit to measurements. Tests compare ordinary
public API output, using no private access or state hacks. Results report six
terminal gains/errors and total assertions. No timing/performance measurement,
malformed-input, parser, scanner, GUI or host behavior is exercised.

## Bounded reproduction

After independent source review, from the repository root, create a fresh output
directory. Do not overwrite an incomplete or prior attempt:

```sh
mkdir build-fx-loudness-core
c++ -std=c++20 -O2 -Isrc tests/FxLoudnessCoreTests.cpp \
  -o build-fx-loudness-core/fx-loudness-core
build-fx-loudness-core/fx-loudness-core
```

CMake registration is provided for maintenance; this qualification can use only
the standalone compile above without configuring/running unrelated suites.
Record exact source/compiler/binary hashes, command, output and exit code before
claiming a pass. A failure is retained and investigated, not retried with relaxed
tolerances. No new plugin module build, production DSP behavior change,
full-FX qualification, loudness-standard compliance, CPU-target or cross-platform
claim is implied by this test.
