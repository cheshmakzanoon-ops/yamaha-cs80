# Loudness matcher closed-gate regression

Status: independently source-reviewed, then compiled and run once on Linux with
GCC 14.2.0 on 2026-11-13: **18/18 assertions passed**, exit 0. Maximum analytic
slew error was 1.2878587085651816e-14 against the fixed 1e-10 bound; post-closure
gain movement ranged from 0.020529755996661514 to 0.021410704766060995.
See [retained output](experiments/fx-loudness-gate/run.log),
[identities](experiments/fx-loudness-gate/result.json), and the
[frozen pre-run plan](experiments/fx-loudness-gate/pre-run-plan.md).
CMake registration was source-reviewed only, not executed.
Base: `ab46660e3d8e34db60147ed3829fb21a3fd0a39a`.

## Unmet contract and bounded fixture

`LoudnessMatcher` holds its target when its input energy gate closes, but
continues the 150 ms gain slew toward that target. Existing proportional-signal
and reset/reprepare tests do not directly witness this closed-gate distinction.
No production code changes are made here; previous lifecycle evidence is untouched.

`tests/FxLoudnessGateTests.cpp` feeds exactly two seconds per rate at 44.1, 48,
and 96 kHz. The first 0.5 seconds are stereo quadrature 1 kHz tones of amplitude
0.0008; the remaining 1.5 seconds are zero input. Output is exactly half input
throughout. There are no live parameter changes, invalid samples, plugin hosts,
FX models, CPU benchmarks or audio device interactions.

An external observer uses the public `KWeighting` filters and independently
maintains one-second mean-square energy, three-second peak decay and the
existing gate conditions. This intentionally shares weighting implementation:
it is not an independent K-weighting or loudness-standard validation. Exact
binary scaling makes output energy one quarter of input energy, including filter
ring-down, and the open-gate target is therefore known to be 2. No private
matcher state is accessed.

After a genuine opening, the observer locates the first closed sample. The
fixture snapshots the prior gain and checks every subsequent double-precision
`currentGain()` against `2 + (g0 - 2) * exp(-N / (sampleRate * 0.15))`, with
N starting at one. It also requires that closure happens during the silent
portion, never reopens, and leaves at least 250 ms of closed observations.

## Predeclared acceptance criteria

Six assertions per rate, 18 total:

- All float-returned and double-observed gains remain finite.
- At least one actual open-gate sample occurs.
- Closure occurs after the burst, stays closed, and covers at least 250 ms.
- Gain remains at least `1e-5` below target at closure.
- Maximum closed-form slew error is at most `1e-10`.
- Gain increases by at least `1e-5` after the first closed sample.

The 150 ms exponential and two-second histories are fixed before execution.
The double error allowance accommodates iterative rounding relative to the
closed form, while being far tighter than the required movement. The movement
and closure checks prevent vacuous success from a never-open gate, settled gain,
or empty observation window. Thresholds must not be tuned after seeing results.
The test targets this ordinary proportional history only: it does not establish
behavior with arbitrary changing output tails, an independent standard, full
model loudness calibration, a live matcher toggle, or all sample rates.

## Bounded reproduction (after source approval)

From the checkout root, in a fresh output directory:

```sh
mkdir build-fx-loudness-gate
c++ -std=c++20 -O2 -Isrc tests/FxLoudnessGateTests.cpp -o build-fx-loudness-gate/fx-loudness-gate
build-fx-loudness-gate/fx-loudness-gate
```

The header-only CMake/CTest target is `cs80_fx_loudness_gate_tests`, with only
`src` include access and a 60-second timeout, no engine linkage. This task does
not configure CMake or execute any other native suites. Freeze source identities
before an approved one-shot run; retain logs and failures rather than retrying
or relaxing thresholds. The result above covers only the standalone run.
