# Offline filter reference renderer

`cs80_truth_render` exports the existing double-precision conditional IG00156
HP-to-LP reference model. This is **filter-stage research infrastructure**, not a
whole-note renderer, hardware measurement, or claim of instrument parity. The
whole-note renderer in IMPROVEMENT_PLAN.md remains open. Unlike `cs80_dsp_probe`
and `cs80_engine_audit`, this tool does not run the realtime engine.

Build the `cs80_truth_render` target (also available with CS80_BUILD_PLUGIN=OFF).
Example:

```
cs80_truth_render --out NEW_DIR --stimulus sine --frequency 440 \
  --hp-hz 200 --lp-hz 2000 --q 1 --amplitude 2.5 --duration .1
```

Use `--help` for options and defaults. `--ota-level 0` selects the linear limit;
positive values select the already-declared `gm * L * tanh(v/L)` law. The default
L=2.5 and unity-passband source adapters are inferred engine-unit assumptions,
not calibrated physical volts. `--gain physical` removes those adapters but does
not establish a hardware voltage calibration. Cutoffs specify total frequencies,
including the feedforward contribution, through the existing inferred 180 pF
compensation mapping. Q is the low-frequency Q, not a promise of high-frequency Q.
Network values, actual gm/Q conductances and adapters are included in the manifest.
See [IG00156_NODAL_MODEL.md](IG00156_NODAL_MODEL.md) for evidence and alternatives.

The model uses ideal amplifiers, inferred HP 75 kOhm and LP 150 kOhm source
resistances, the conditional unilateral Q current, and linear Q/feedforward
paths. No new device law, realtime prewarp, oscillator, envelope or effect is used.
Both models start with all eight capacitor states zero.

## Reuse a sampled oscillator or note as filter input

The optional sample mode runs an arbitrary mono waveform through **only this
fixed-control filter pair**. It does not add oscillator/envelope/voice rendering,
replace the realtime filter, or calibrate hardware:

```sh
cs80_truth_render --out NEW_FILTER_DIR --input oscillator.csv \
  --input-rate 48000 --input-samples 4801 --gain physical
```

`oscillator.csv` must have exactly the header `sample,value`, then exactly the
specified number of rows with canonical contiguous indices starting at zero:

```csv
sample,value
0,0.25
1,-0.5
2,0.125
```

For that three-value example, specify `--input-samples 3`. N endpoint values
define **N-1 intervals**, from time zero through `(N-1)/input-rate`. There is no
extra held final interval, extrapolation, latency trimming or resampling. Forcing
is piecewise linear between the supplied endpoints at both solver refinements;
this is not bandlimited reconstruction. Input/output rates are identical. All
eight filter states start at zero even when the first input value is nonzero.
Controls remain fixed for the entire render.

Values are signed, finite ASCII decimals in [-8, 8], interpreted directly as
**declared, uncalibrated model input volts** before the selected gain adapters.
No normalization or implicit amplitude scaling is performed. Engine floats and
recordings are not automatically calibrated volts: any numerical-unit mapping
is an explicit modeling assumption, not new hardware evidence. `--gain physical`
removes the inferred unity-passband adapters; it does not establish calibration.

All three input options are mandatory together. `--stimulus`, `--frequency`,
`--amplitude`, `--duration` and `--rate` conflict with input mode and are rejected,
including values equal to their defaults. Input rate must be an integer from
8000 through 384000 Hz. The same ten-second and ten-million-combined-step budgets
apply. Files must be regular files, at most 64 MiB of actual bytes read, with at
most 128 bytes before each newline. LF and CRLF are accepted; every line,
including the final row, needs a newline. Quoting, blank lines, whitespace within
fields, missing/extra/reordered indices, extra columns, nonfinite/hexadecimal
numbers, out-of-range or unrepresentable decimals, and count mismatches fail
before output. Declaring N separately detects whole-row truncation as well as a
truncated final token.

For a concrete software-note workflow, the existing core capture exports the
same two-column CSV layout (see [PARITY_HARNESS.md](PARITY_HARNESS.md)):

```sh
cs80_parity_capture --out NEW_NOTE_DIR
cs80_truth_render --out NEW_FILTER_DIR --input NEW_NOTE_DIR/left.csv \
  --input-rate 48000 --input-samples 96000 --gain physical
```

This deliberately re-filters an already-filtered engine output. Treating one
engine float as one model input volt is the explicit, uncalibrated unit mapping
in this example. It is useful for conditional input-response experiments, **not**
a comparison to the engine's internal filter input or a whole-note reference.
The 96,000 endpoints cover 95,999 intervals; startup silence, decimator latency
and note/release shape remain in the source. Keep the original `capture.json`,
source revision and binary provenance alongside the experiment. Other mono
oscillator or note exports can use this interface after an explicitly documented
unit conversion into this exact layout. The parity harness and its synthetic
acceptance policy are unchanged.

Sample mode also exports `stimulus.csv`, an **exact byte copy of the same bounded
snapshot parsed and rendered**, and records its SHA-256, byte count, sample count,
rate, units and interpolation in `manifest.json`. Neither hashing nor exporting
rereads the source file. Thus later source changes do not relabel the waveform
that was rendered. Source paths are omitted for deterministic, portable outputs.
Synthetic-only frequency/amplitude/pulse-definition metadata are null in sample
mode. The hash helper's own source hash is included in all manifests.

## Output and numerical checks

- `trace.csv`: initial state at time zero followed by floor(duration * rate)
  endpoints. Time, stimulus, HP and cascade LP outputs, and all eight states are
  written at 17-digit precision. Sample mode instead writes exactly N endpoint rows as described above.
  State polarity is the physical inverting
  integrator convention. The impulse is a rectangular amplitude pulse over
  [0, 1/rate), not a unit-area Dirac impulse. The input column is right-continuous;
  the jump is integrated on separate intervals without smearing.
- `manifest.json`: schema, scope, model assumptions, full network/control values,
  requested/actual duration, stimulus settings, step sizes and checks. Build-time
  SHA-256 hashes identify renderer, network, linear solver, control header,
  model document and offline input-hash helper source contents, including
  uncommitted edits. CMake regenerates these hashes when any of these inputs
  changes. No wall
  clock, output path or random seed affects output. Bytes repeat on the same
  executable/platform; cross-platform transcendental last bits may differ.
- Two runs use N and 2N substeps per output interval (N=16 by default). The finer
  run is exported. The maximum state/HP endpoint difference divided by the global
  fine-run state/HP peak must not exceed `--tolerance` (default .001). The scale
  floor is 1e-30. This is a **time-refinement estimate**, not a rigorous continuous
  solution error bound or a hardware acceptance threshold.
- Every step separately certifies finite states and the implicit trapezoid
  equation residual <= 1e-14 times the maximum state magnitude (floor 1e-30).
  An exhausted Newton solve is not silently accepted. The final residual and
  iteration-count diagnostics are independent of the refinement estimate.

Arguments are bounded to avoid accidental unbounded runs (10 million combined
steps; at most ten seconds). Unknown/duplicate options, invalid numbers, existing
output paths, failed Newton checks, failed refinement and file errors return
nonzero. Numerical checks finish before creating the output directory. File-write
failures remove this run's CSV/JSON files (including the sample-mode stimulus
copy) and remove its directory only if empty. Success is printed only after all
output files close. A process crash or external concurrent mutation can still leave incomplete
files; consumers should require a successful process exit. No existing directory
is replaced. Increase substeps when numerical checks fail; do not interpret a
looser tolerance as better model evidence.

Tests include executable deterministic output, pinned assumptions, an independent
Python linear trapezoid oracle for sine/pulse and arbitrary signed, constant,
zero and multitone sample traces; SHA-256 standard vectors and Python hashlib
cross-checks; exact input-copy/provenance and determinism; malformed/count/rate/
budget/collision failures and injected write-failure cleanup; and explicit
finite Newton exhaustion/nonfinite diagnostics. Existing nodal gates continue to use the original unchecked step API and arithmetic.

## Verification checkpoint

The initial implementation was built against production baseline `b7f801d8`.
The three affected registered suites passed together: renderer executable tests,
checked-step diagnostics, and the existing IG00156 nodal gate (19.40 seconds).
An independent source review and repeat passed the same three suites in 20.59
seconds with process exit 0. This qualifies the bounded exporter and reference
API refactor, not the full plugin, whole-instrument reference rendering, or
hardware fidelity. Realtime audio source files are unchanged.

### Sample-input increment

The input increment was developed against `3148cfdf`. Four affected registered
suites passed together in 19.78 seconds: renderer executable contracts and
independent oracle, input SHA-256 vectors, checked-step diagnostics, and the
existing nodal gate. An independent review repeated all four successfully in
19.18 seconds with exit 0. Existing short sine/pulse trace files matched the
preserved pre-increment executable byte for byte on this platform.

The documented current-core capture workflow also completed: 96,000 endpoints
at 48 kHz, 95,999 intervals, exact input snapshot and SHA-256 verified, and
relative refinement estimate 1.3156733609106375e-05 (default tolerance .001).
This demonstrates reusable conditional filter input, not hardware fidelity,
whole-note reference synthesis, a full-plugin regression run, or a speed claim.
