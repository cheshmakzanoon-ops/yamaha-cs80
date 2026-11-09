# IG00156 top-octave numerical accuracy guards

2026-11-09. Baseline: `497be8482d0c8ff065b83c4c8ec3300e738c645b`.

**This change fixes a validation gap, not the filter's top-octave error.**
No production coefficient, state, algorithm, oversampling policy or audio path
changes. The original full-grid small-signal acceptance gate remains **NOT
MET** at the lower internal rates. The ADJ p11 HP setup-point miss is also
unchanged. This is numerical work against a declared circuit graph, not
measured CS-80 parity.

## What was missing

`cs80_ig00156_nodal_tests` correctly reported the continuous-truth misses and
asserted the original acceptance limits on the passing sub-grids. It did not
bound further degradation in the failing cells. Its direct lock-in checks
covered 250 Hz and 4 kHz LP cutoffs at 96 kHz, not the high-cutoff HP/LP/cascade
and rate grid.

The new `cs80_ig00156_top_octave_tests` adds two separate protections:

- Explicit regression ceilings on every high-cutoff cell, including the
  previously report-only failures. They are recorded separately in
  `tests/parity/known-deviations/ig00156-top-octave.json` and are **not relaxed
  parity acceptance limits**. The failed cells get rounded roughly 5%
  headroom over already-known observations. Passing cells retain the original
  limits. These are characterized regression bounds, not preregistered claims
  that the old model met them before evaluation.
- Actual stage/cascade output measured by complex coherent lock-in, compared
  with the independent nodal response under the current documented
  trapezoidal map and with the unwarped continuous response. Phase error is
  included; equal magnitude alone does not pass.

The literal reference graph and declared control-law calculation are pinned
in the new fixture rather than copied from runtime network values or calling
the runtime cutoff-calibration helper. They use the existing offline nodal
matrix solver. Both implementations still embody the same declared/inferred
circuit topology; numerical agreement does not validate those inferences
against hardware.

## Coverage and results

Four internal rates, two total cutoffs (4/12 kHz), five low-frequency Q values
(0.5/1/2/5/9), and HP, LP and HP-to-LP cascade configurations. For each of the
120 fixtures, actual output is measured at the requested cutoff and at the
analytic sweep's worst full-band frequency, rounded to a coherent bin.
Amplitude is `1e-5`, settling is 0.35 seconds, and the measurement is 8,192
samples. Normalization is the continuous response peak on the same sampled
peak-search grid used by the original gate. The direct-discrete tolerance
`5e-6` and rate-refinement ratio `0.35` were selected before running the new
fixtures, not fitted to the outcomes.

| Internal rate | Total cutoff | Core error | Full-band error | Direct measured full-band maximum |
| --- | --- | --- | --- | --- |
| 88.2 kHz | 4 kHz | 1.9842% | 11.2860% | 11.2918% |
| 88.2 kHz | 12 kHz | 4.7923% | 14.0323% | 14.0403% |
| 96 kHz | 4 kHz | 1.6688% | 11.0510% | 11.0560% |
| 96 kHz | 12 kHz | 4.0300% | 13.7093% | 13.7146% |
| 192 kHz | 4 kHz | 0.4112% | 2.7970% | 2.7943% |
| 192 kHz | 12 kHz | 3.4948% | 3.4948% | 3.4920% |
| 384 kHz | 4 kHz | 0.1024% | 0.6878% | 0.6887% |
| 384 kHz | 12 kHz | 0.8632% | 0.8632% | 0.8642% |

Bands are unchanged: core `0.5..2 fc` and at most `fs/8`; full band
`20 Hz..min(20 kHz,fs/5)`. The maxima are over sampled grids, not rigorous
bounds on every frequency/control combination. Direct bins differ slightly
from the geometric analytic grid, explaining their slightly different maxima.
The worst actual-output error versus the independent **discrete** response is
`1.14604308e-7` of continuous response peak, below `5e-6`. This confirms the
running filter implements its documented discretization; it does not remove
the discretization error versus continuous truth.

Common-physical-band sweeps at base/2x/4x rates, for bases 88.2 and 96 kHz,
shrink maximum errors by factors `0.2343..0.2469` per doubling. The comparison
holds the frequency band fixed, avoiding a false convergence comparison in
which `fs/5` or `fs/8` changes between runs. The final 352.8/384 kHz targeted
4/12 kHz cells also meet the original 2% high-rate limit over the full 20 kHz
band. This is timestep-refinement evidence, not a CPU-budget result or an
instruction to silently raise the runtime quality setting.

Local verification, GCC 14.2.0, Linux x86-64:

- New test: **508 checks, zero failures**.
- Five registered IG00156 CTests: **5/5 pass**, including the original nodal,
  linear reference, first-stage filter and coefficient-cache tests.
- AddressSanitizer + UndefinedBehaviorSanitizer + float-cast-overflow checks:
  **508 checks, zero failures**. Leak detection was disabled; no leak-clean
  claim is made.
- Isolated negative controls: removing runtime prewarp causes **311 failures**;
  adding one sample of output delay causes **474 failures**. The latter keeps
  steady-state magnitude unchanged but is rejected by the complex comparison.
- Putting the original 3%/8% acceptance limits into the 88.2 kHz/12 kHz
  known-deviation slot causes **14 failures**, demonstrating that the original
  numerical acceptance miss is still present and the configured ceiling is
  actually enforced.

No plugin, DAW, other-platform or full-core-suite execution was done for this
test-only increment. No production behavior was modified.

## Why no quick runtime substitution was promoted

The root cause is the bilinear frequency map, not an inaccurate Newton solve
or a stale coefficient cache. An empirical cutoff/gain adjustment would trade
errors between frequencies and change the declared model, not generally solve
the discretization problem. Changing global quality from 2x/4x to 8x also
changes CPU cost and latency policy and needs its own qualification.

A diagnostic local-substep candidate keeps the current external input rate,
linearly interpolates input between successive samples, and runs the pair at
1/2/4/8 substeps. At 96 kHz external rate, 12 kHz total cutoff, Q=5 and an
18 kHz input, continuous-truth errors are:

| Configuration | One step | Eight local substeps |
| --- | --- | --- |
| HP | 5.7318% | 4.5006% |
| LP | 5.7997% | 2.8447% |
| HP-to-LP | 3.6549% | 1.5803% |

Extra solves alone do not supply the missing intersample waveform. The
remaining input-reconstruction error is not cured by taking more steps. This
limited diagnostic is not a full candidate-acceptance grid and is **not an
accepted runtime path**.

A true improvement needs a justified input reconstruction or jointly clocked
source/filter path, a suitable higher-order nonlinear solver or qualified
rate increase, plus preserved idle-history/snapshot/replay semantics. It must
meet the unchanged continuous-truth limits, large-signal/stability/aliasing
and continuity checks, and measured callback budgets. A new discretization
would also require a separately derived discrete oracle rather than relaxing
the continuous-truth gate to accommodate it.

## Reproduction

```
cmake -S . -B build-core -DCS80_BUILD_PLUGIN=OFF -DCS80_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-core --target cs80_ig00156_top_octave_tests
ctest --test-dir build-core -R '^cs80_ig00156_top_octave_tests$' --output-on-failure

# Optional unpromoted local-substep diagnostic:
build-core/cs80_ig00156_top_octave_tests tests/parity/known-deviations/ig00156-top-octave.json --substep-experiment

# Builds isolated mutations under a new directory; leaves the checkout intact:
python3 scripts/test-ig00156-top-octave-mutations.py /tmp/cs80-filter-negative-controls

# The executed Linux sanitizer configuration:
c++ -std=c++20 -O1 -g -Wall -Wextra -Wpedantic -fsanitize=address,undefined,float-cast-overflow -fno-sanitize-recover=all -D_GLIBCXX_ASSERTIONS -Isrc tests/Ig00156TopOctaveTests.cpp -o /tmp/cs80-filter-guard-sanitized
ASAN_OPTIONS=detect_leaks=0 /tmp/cs80-filter-guard-sanitized tests/parity/known-deviations/ig00156-top-octave.json
```
