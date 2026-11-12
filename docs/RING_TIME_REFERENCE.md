# Independent ring time reference

`scripts/render_ring_reference.py` is a Python 3.11+ standard-library offline
reference for a fixed sine input and a piecewise-linear (PWL) **converter-node
voltage**. It adds time-domain phase integration to the algebraic anchors and
offset multiplier in `src/reference/RingModulator.h`, without modifying that
header, production processing, old exporters or contracts.

## Evidence and scope

The adjustment-procedure period anchors and their interpretation are classified
in [ws4-circuits Step 2](workstreams/ws4-circuits.md). The 150 ms period at +5 V
and 5 ms maximum-SPEED period inform the model. The maximum node voltage of +10 V
and exponential interpolation remain declared inferences. Offset values are
those of the existing independent double-precision header model, not measured
hardware calibration. Engine input/output units are not electrical volts.

This tool accepts node voltage directly. It does **not** derive voltage from
SPEED, depth or an envelope, generate an envelope, apply production smoothing,
model a whole voice, claim production equivalence or qualify alias suppression.
The input sine and amount are fixed; both initial phases are explicit cycles.
No audio playback, WAV, normalization or listening claim is included.

## Equations and clock

```
k = log(30)/5
f(V) = (1/.150) exp(k*(V-5))
z = k*(Vend-Vstart)*elapsed/duration
phase_increment = f(Vstart)*elapsed*expm1(z)/z
carrier = sin(2*pi*cycles)
x = amplitude*sin(2*pi*(input_phase + input_frequency*t))
wet = (x+.0005)*(carrier+.005)+.001
output = (1-amount)*x+amount*wet
```

The zero-z limit is exactly `f(Vstart)*elapsed`. The bounded voltage change is
formed as a rational before conversion, avoiding an unnecessary huge reciprocal
slope for closely spaced knots. `expm1` preserves small slopes. This is an
analytic integral evaluated in binary64, not an interval error certificate.

Sample times are exact `Fraction(i, rate)` and decimal knots are exact rationals.
Phase anchors are accumulated **only at actual knots**, never output samples.
Each query integrates from the fixed left knot; inserting output samples cannot
change a shared endpoint. Voltage and phase remain continuous at knots. Knot
queries select the right segment; the final knot uses its stored phase. There is
no time-zero tick. Total carrier cycles are retained separately from periodic
argument reduction for sine evaluation. Platform libm final bits can differ.

## Explicit CLI

All options are required; the output directory must not exist and its parent
must exist. Example for an 81-endpoint, 8 kHz, 10 ms span:

```csv
knot,time_s,volts
0,0,5
1,.0037,7
2,.01,6
```

```sh
python3 scripts/render_ring_reference.py --knots knots.csv \
  --input-amplitude .4 --input-frequency 440 --input-phase .125 \
  --amount .8 --carrier-initial-phase .25 \
  --rate 8000 --samples 81 --out NEW_RING_REFERENCE
```

The CSV must end with a newline, use consecutive zero-based indices and strictly
increasing knot times, begin at zero and end at the exact last sample time.
Bounds: 2–1,024 knots, 0–10 V; 8,000–384,000 Hz output rate;
2–20,001 endpoints spanning at most two seconds. Input amplitude and amount are
0–1, input frequency is positive and at most 2,000 Hz, initial phases are in
[0,1) cycles. Input bytes are capped at 1 MiB and the output bundle at 32 MiB.

Artifacts: `trace.csv` with voltage, instantaneous carrier Hz, total cycles,
carrier/input/wet/output; `knots.json` and `segments.json` exact-clock audits;
`input_knots.csv`; renderer and imported helper source snapshots; a provenance-only
`RingModulator.h` snapshot; and `manifest.json` written last with parameters,
constants, inferences, limitations and artifact byte counts/SHA-256 hashes.
The header is not executed. Source snapshots identify bytes, not an authenticated
execution image. Replay from the repository checkout: the exporter locates the
header relative to its repository source path, so the archived source pair alone
is not a standalone replay bundle. Output uses existing bounded snapshot and owned-file cleanup
helpers without modifying them; cleanup does not recursively delete directories.

## Bounded verification

The independent test entry point is:

```sh
python3 tests/test_ring_time_reference.py
```

CMake registers `cs80_ring_time_reference_tests` with a 120-second timeout when
`CS80_BUILD_TESTS`, UNIX and discovered Python >=3.11 are all satisfied. Existing
registrations remain unchanged. No production executable is required.

Source implementation is not a passing result. Runtime/version results and
independent review must be recorded separately. Intended bounded checks cover
analytic anchors, constant/swept-voltage phase, independent numerical integration,
shared-grid identity, dry blend and offset/sideband algebra, plus one ordinary
CLI provenance bundle. No malformed-input, fuzz, native GUI, broad regression,
CPU/benchmark or hardware qualification is implied.

## Verified local result

One direct run of all nine bounded valid-input tests passed, including refined
independent phase quadrature, exact shared-grid endpoints and signed finite-window
spectral projections. One ordinary CLI example exported 81 rows; its final knot
and every declared artifact hash were verified. Configuration generated one test
registration, but no build or CTest execution is claimed. Python 3.12.14 is the
only tested runtime; Python 3.11 is the declared baseline, not separately qualified.
Production sources, existing exporters and all 20 earlier contracts remain
unchanged. These results do not establish hardware fidelity or alias/antialias
qualification.

Exact commands, logs, source/interpreter hashes and the complete small example
bundle are retained in [the verification evidence](experiments/ring-time-reference/verification.json).
