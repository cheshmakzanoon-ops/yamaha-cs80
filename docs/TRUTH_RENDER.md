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

## Output and numerical checks

- `trace.csv`: initial state at time zero followed by floor(duration * rate)
  endpoints. Time, stimulus, HP and cascade LP outputs, and all eight states are
  written at 17-digit precision. State polarity is the physical inverting
  integrator convention. The impulse is a rectangular amplitude pulse over
  [0, 1/rate), not a unit-area Dirac impulse. The input column is right-continuous;
  the jump is integrated on separate intervals without smearing.
- `manifest.json`: schema, scope, model assumptions, full network/control values,
  requested/actual duration, stimulus settings, step sizes and checks. Build-time
  SHA-256 hashes identify renderer, network, linear solver, control header and
  model document source contents, including uncommitted edits. CMake regenerates
  these hashes when any of these inputs changes. No wall
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
failures remove this run's two files and remove its directory only if empty. Success is printed only after both files
close. A process crash or external concurrent mutation can still leave incomplete
files; consumers should require a successful process exit. No existing directory
is replaced. Increase substeps when numerical checks fail; do not interpret a
looser tolerance as better model evidence.

Tests include executable deterministic output, pinned assumptions, an independent
Python linear trapezoid oracle for sine/pulse traces, malformed CLI and output
failures, and explicit finite Newton exhaustion/nonfinite diagnostics. Existing
nodal gates continue to use the original unchecked step API and arithmetic.

## Verification checkpoint

The initial implementation was built against production baseline `b7f801d8`.
The three affected registered suites passed together: renderer executable tests,
checked-step diagnostics, and the existing IG00156 nodal gate (19.40 seconds).
An independent source review and repeat passed the same three suites in 20.59
seconds with process exit 0. This qualifies the bounded exporter and reference
API refactor, not the full plugin, whole-instrument reference rendering, or
hardware fidelity. Realtime audio source files are unchanged.
