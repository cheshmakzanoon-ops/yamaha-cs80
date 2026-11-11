# Offline filter reference renderer

`cs80_truth_render` exports the existing double-precision conditional IG00156
HP-to-LP reference model. This is **filter-stage research infrastructure**, not a
whole-note renderer, hardware measurement, or claim of instrument parity. The
whole-note renderer in IMPROVEMENT_PLAN.md remains open. Unlike `cs80_dsp_probe`
and `cs80_engine_audit`, this tool does not run the realtime engine. Its opt-in
comparison runs only the production filter stages (see below).

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

## Opt-in production filter comparison

To compare the reference against the actual production `Ig00156Stage` HP network
followed by its LP network at fixed controls:

```sh
cs80_truth_render --out NEW_COMPARISON --rate 96000 --duration .1 \
  --frequency 500 --amplitude 2.5 --hp-hz 1000 --lp-hz 1000 --q 5 \
  --compare-realtime true --realtime-rate 96000
```

The explicit `--realtime-rate` is the **internal filter clock**, not a host audio
rate. It must equal the reference endpoint rate. There is no hidden oversampling,
resampling, engine voice, decimator or latency compensation. Use the actual
internal filter rate for an engine-stage experiment. Sample input works with the
same comparison options, provided its first value is exactly zero. This ensures
that processing sample zero from reset leaves both production histories and the
reference capacitor states zero at time zero. Subsequent production samples are
compared to reference outputs at the same endpoint times. There is no automatic
warmup, trimming, delay/phase alignment, gain fitting or normalization.

Comparison requires unity-passband adapters and the production OTA level (2.5).
Synthetic impulse is rejected because its initial nonzero source and rectangular
jump are not this endpoint-alignment contract. To study a sampled pulse, provide
an explicit zero-start CSV; its reference forcing is piecewise linear rather than
the synthetic rectangular pulse. A total cutoff whose **float-cast gm control**
would be clamped by production at 0.45 times the internal rate is rejected. Small
float quantization of an otherwise legal cutoff is retained and reported, not
silently corrected. The original reference controls and forcing are unchanged.

`trace.csv` remains the original fine-reference trace. Comparison adds:

- `realtime.csv`: endpoint index/time, actual float-cast input, production HP and
  cascade LP float outputs, and signed production-minus-reference HP/LP differences.
  The HP float output is passed directly to the LP stage, as in production.
- `manifest.json` gains `realtime_comparison`: actual float rate, gm-cutoff and Q
  controls, effective total cutoffs and Q conductances, production OTA level,
  source hashes of `Ig00156Filter.h`, `Dsp.h` and `FastMath.h`, and HP/LP metrics.
  For each stage, absolute RMS is sqrt(sum(difference²)/N), absolute maximum is
  max(abs(difference)), reference RMS is sqrt(sum(reference²)/N), and relative RMS
  is sqrt(sum(difference²)/sum(reference²)). All N exported rows, including initial
  zero and startup transient, contribute. Relative RMS is JSON null when reference
  energy is zero, including an all-zero run, rather than a misleading zero or an
  arbitrary denominator floor. There is **no acceptance threshold** for these metrics.

Production uses cutoff prewarp, bounded tan/tanh approximations and a capped
scalar Newton iteration with its own stopping rule. The reference is unwarped,
uses double-precision forcing/state with refined integration and certified
Newton residuals. Production controls, input, interstage signal and output are
floats. Reference forcing remains analytic sine or piecewise linear supplied
double endpoints. Consequently this diagnostic includes discretization,
approximation and quantization differences. It is useful for locating differences
between these two conditional software implementations; it does not isolate a
single error source or establish hardware fidelity or whole-instrument parity.

Without the comparison options, no production processing or comparison file is
performed. Existing stdout and exit conventions and reference numerical checks
remain unchanged. Comparison outputs must be finite before publication. File
failure cleanup also removes this run's `realtime.csv`. Production source files,
existing nodal thresholds, and the parity harness are unchanged.

### Comparison verification checkpoint

Developed against `498d2481`. The renderer contract/oracle, input SHA-256,
checked-step diagnostics and existing nodal suites passed together (20.55 s).
Short sine and impulse reference traces match the preserved pre-increment
executable byte for byte on this platform. Comparison tests independently
recompute exported metrics from both CSVs, check exact float conversions and
header hashes, compare reference-only and comparison trace bytes, exercise zero
and driven inputs, and reject incompatible options and clamped cutoff controls.

One exact existing nonlinear nodal fixture was also replayed: 144,000 endpoints
at 96 kHz, 500 Hz analytic sine, amplitude 2.5, Q=5, both total cutoffs 1 kHz,
16 fine reference substeps. Its final 9,600 rows yielded relative LP RMS
0.001320787 (within that existing fixture's 0.03 threshold). The new untrimmed,
all-row diagnostic yielded relative LP RMS 0.003707358, absolute RMS 0.003493139
and absolute maximum 0.117630850. This illustrates why the time window must be
specified; the fixture threshold is **not** applied to arbitrary renderer runs.
These focused checks do not qualify the full plugin or hardware fidelity.

Independent source review and a repeat of all four affected suites passed in
19.93 seconds with process exit 0. The reviewer also recomputed exported CSV
metrics and verified preserved synthetic trace bytes. Their separate diagnostic
render used 144,001 endpoints; it corroborates the metric calculation, not an
additional exact replay of the 144,000-endpoint frozen fixture above.

## Time-varying cutoff and resonance diagnostics

An optional `--controls FILE.csv` supplies a filter-control trajectory at the
same endpoint clock as the stimulus. Its exact header is
`sample,hp_hz,lp_hz,q`. Rows must start at sample 0, use consecutive indices, and
contain exactly as many endpoints as the waveform. With synthetic input this is
`floor(duration * rate) + 1`; with sample input it is `--input-samples`.
The trajectory replaces the scalar HP cutoff, LP cutoff and Q options, so
combining it with `--hp-hz`, `--lp-hz` or `--q` is rejected rather than silently
choosing one source.

For example, prepare a short rising-cutoff trajectory, then inspect both
reference and production-stage responses:

```sh
python - <<'PY'
import csv
rate, intervals = 48000, 4800
with open('filter-sweep.csv', 'w', newline='') as f:
    out = csv.writer(f)
    out.writerow(['sample', 'hp_hz', 'lp_hz', 'q'])
    for i in range(intervals + 1):
        phase = i / intervals
        out.writerow([i, 200, 500 + 4500 * phase, 1 + 3 * phase])
PY
./cs80_truth_render --out new-sweep-reference --rate 48000 --duration .1 \
  --frequency 440 --controls filter-sweep.csv \
  --compare-realtime true --realtime-rate 48000
```

This is a declared **software control stimulus**, not a recovered CS-80 envelope
law. Inside each waveform interval, the requested HP/LP total cutoff frequencies
and Q are linearly interpolated. The existing declared cutoff and Q mappings are
then evaluated at each integration substep endpoint. The renderer does not
interpolate already-mapped transconductances. Network topology, source adapters,
OTA level and capacitor states retain their existing meanings.

The checked reference step uses the start controls for its old-state derivative
and the end controls for its new-state derivative and Newton Jacobian. Its
residual check uses those same endpoints. Both numerical refinement runs follow
the same continuous requested-control trajectory. Fine/coarse agreement is still
an estimate, not an error bound or a hardware acceptance test.

Optional production comparison instead calls the actual stage setters with each
float-cast endpoint control before processing that endpoint sample, retaining
the stages' existing histories. No reset or artificial state correction is
inserted. This differs from the fine-step reference's control interpolation as
well as its forcing, precision and prewarp. Reported differences cannot be
attributed to a single cause or called a production defect merely because they
are nonzero. No universal modulation acceptance threshold is introduced.

Control values must be finite and within the same declared cutoff/Q bounds as
scalar options. Comparison additionally rejects every endpoint whose effective
float cutoff would trigger the production clamp. Endpoint bounds also bound the
linearly interpolated requested values between them. The exact consumed control
bytes are retained and hashed, so the file used for the experiment can be
recovered without relying on a later read of its original pathname.

Dynamic runs add `controls.csv` (the exact input snapshot) and
`control_endpoints.csv` (requested Hz/Q and mapped reference gm/Q conductances at
each waveform endpoint). In comparison mode the latter also records production
float controls, effective total cutoffs and Q conductances. The manifest's
`controls` object records filenames, SHA-256, consumed byte count, endpoint
count/rate and interpolation semantics. Scalar control metadata describes only
the initial endpoint in this mode; it must not be mistaken for the entire
trajectory. Output-time endpoint values do not enumerate every fine integration
substep, which is defined by the interpolation rule above.

The control file has a 64 MiB read limit and 128-byte line limit. It uses strict
ASCII decimal values, no blank or extra rows, and a terminating newline on every
line; LF and CRLF are accepted. Indices must be contiguous integers. These are
input-format constraints, not tolerances on the modeled control values.

### Dynamic-control verification checkpoint

Developed against `646dda88`. The final four affected registered suites passed
in 20.29 seconds; independent source review and a repeat passed in 19.93 seconds,
both with exit 0. The renderer suite contains 15 tests, including an independent
linear time-varying matrix oracle, constant-control trace equality, malformed
trajectories, final-endpoint clamp rejection and artifact-write cleanup.
The documented 4,801-endpoint sweep completed with the default numerical settings
and a refinement estimate of 3.40844e-06. This verifies the specified diagnostic
workflow, not a recovered hardware modulation law or general production error
threshold. Realtime DSP source files are unchanged.

## Optional diagnostic listening export

Add `--wav-gain 0.1` to explicitly enable `reference.wav` (fine reference LP output)
and, when `--compare-realtime true` is enabled, `realtime.wav` (production diagnostic
LP output). Both receive exactly the same supplied linear gain. Gain must be finite
and in [0,1000000]; zero explicitly mutes. Omit the option to produce no WAV files.
The gain applies only to these files, never to CSV traces or numerical metrics.

Start playback at low volume. These are uncalibrated diagnostic signals, not a
finished instrument, measured hardware truth, or perceptual proof. Float samples
above full scale are retained; players and audio devices may clip them. There is no
clipping, limiting, normalization, auto-level, latency shift, trimming or resampling.

Files use little-endian mono IEEE float32 WAV (format 3, fmt extension size 0,
`fact` sample count). Each double endpoint is multiplied by the explicit gain and
rounded once to float32. There are N samples including t=0: sample i occurs at
i/rate; the endpoint span is (N-1)/rate, while WAV playback lasts N/rate. The
manifest `audio_export` records both durations, shared gain, and each exported
float32 peak and count of samples strictly above absolute 1. Metadata and output
bytes are deterministic. Invalid/nonfinite/out-of-float-range values or write
failures reject the export and clean up files owned by the run.
