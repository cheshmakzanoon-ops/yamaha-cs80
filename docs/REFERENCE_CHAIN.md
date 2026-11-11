# Filter to VCA endpoint diagnostic

`scripts/render_reference_chain.py` composes the existing conditional IG00156
HP→LP reference exporter and IG00151 VCA reference exporter, without changing
either solver or any realtime source. It requires Python 3.10+ on a POSIX host
and locally built `cs80_truth_render` / `cs80_vca_reference` executables.

This is an **externally controlled filter/VCA experiment**, not a
whole-instrument renderer, a hardware measurement, or a perceptual/CPU/parity
qualification. LC, EI and envelope are user-supplied endpoint values, not an
envelope-generator or voice lifecycle model. Filter controls may be either
explicit fixed scalars or an explicit cutoff/Q trajectory. The latter uses the
existing standalone filter exporter's `--controls` semantics unchanged.

## Build and reproducible example

```sh
cmake -S . -B build/reference -DCS80_BUILD_PLUGIN=OFF -DCS80_BUILD_TESTS=ON
cmake --build build/reference --target cs80_truth_render cs80_vca_reference

# Both input filenames must be new. Declared numeric mapping: one sampled
# source unit is one uncalibrated filter input unit; LP passes unchanged to VCA.
python3 - <<'PY'
import math
rate, count = 48000, 4801
with open('chain-signal.csv', 'x') as signal, open('chain-controls.csv', 'x') as control:
    signal.write('sample,value\n')
    control.write('sample,lc,ei,envelope\n')
    for i in range(count):
        t = i / rate
        signal.write(f'{i},{.2 * math.sin(2 * math.pi * 440 * t):.17g}\n')
        envelope = max(0, min(t / .01, (.1 - t) / .03, 1))
        control.write(f'{i},0.8,0,{envelope:.17g}\n')
PY

python3 scripts/render_reference_chain.py \
  --filter build/reference/cs80_truth_render \
  --vca build/reference/cs80_vca_reference \
  --input chain-signal.csv --controls chain-controls.csv \
  --rate 48000 --samples 4801 \
  --hp-hz 200 --lp-hz 2000 --q 1 --ota-level 2.5 --gain unity \
  --compare-realtime true --wav-gain .1 --out NEW_CHAIN
```

All signal/VCA-control paths, executable paths, rate/count, OTA level, gain mode
and output path are required. Supply either all three scalar filter options
(`--hp-hz`, `--lp-hz`, `--q`) or `--filter-controls FILE.csv`. Combining either
source with the other, even when values match, is rejected. `--controls` always
means the VCA-control file; `--filter-controls` is a separate schema and input.
Optional numeric controls:

- `--filter-substeps 16` and `--vca-steps 8`: each stage reruns at twice this
  value and exports its fine reference trace. Bounds are 1..256 and 1..128.
- `--filter-tolerance .001`: existing relative filter refinement guard in
  (0,.1]. `--vca-tolerance .001`: existing absolute VCA guard in (0,1]. They
  remain **separate component checks**, not a combined chain error estimate.
- `--initial-li X`: [-8,8], otherwise the first LC. VCA coupling and filter
  states start at zero. A nonzero first envelope can produce a nonzero initial
  reference output from the declared VCA offset/feedthrough.
- `--compare-realtime true`: omit to disable. Requires unity filter adapters,
  OTA level 2.5, first input and envelope exactly zero, initial LI equal first
  LC, and every cutoff endpoint accepted by the existing production filter
  comparison. A final-endpoint cutoff that would be clamped also fails.
- `--wav-gain G`: omit to disable. Explicit finite linear gain in [0,1000000]
  is applied equally to all component WAVs and both selected chain WAVs. Zero
  mutes. CSVs and metrics do not change.

Start playback at low volume. WAV samples above full scale are retained and
may clip in a player/device. No normalization, implicit gain, limiting, clipping,
resampling, trimming, delay shift or gain/phase fitting occurs. The selected WAVs
are exact copies of the correctly selected VCA-stage WAVs, checked against the
selected double output times gain, rounded once to float32.

## Cutoff/Q sweep with external VCA attack/release

The preceding signal and VCA files can be used unchanged. Supply the following
4,801-endpoint filter trajectory instead of scalar HP/LP/Q arguments:

```sh
python3 - <<'PY'
count = 4801
with open('chain-filter-controls.csv', 'x') as control:
    control.write('sample,hp_hz,lp_hz,q\n')
    for i in range(count):
        phase = i / (count - 1)
        control.write(f'{i},200,{500 + 4500 * phase:.17g},{1 + 3 * phase:.17g}\n')
PY

python3 scripts/render_reference_chain.py \
  --filter build/reference/cs80_truth_render \
  --vca build/reference/cs80_vca_reference \
  --input chain-signal.csv --controls chain-controls.csv \
  --filter-controls chain-filter-controls.csv \
  --rate 48000 --samples 4801 --ota-level 2.5 --gain unity \
  --compare-realtime true --wav-gain .1 --out NEW_SWEEP_CHAIN
```

The filter-control header is exactly `sample,hp_hz,lp_hz,q`. It has the same N
canonical endpoint indices and implicit rate as both other files, with total
HP/LP cutoffs in [25,20000] Hz and low-frequency Q in [.35,9]. Strict ASCII,
newline and numeric rules below also apply, with a 128-byte line limit. No rate
is inferred, no source is resampled and no endpoint is clamped or normalized.

The requested Hz and Q are piecewise linear between endpoints; the native
filter maps them to conductances at each integration substep endpoint. It does
not interpolate already-mapped conductances. The optional production stages set
float-cast controls before processing each endpoint, retaining their histories.
These distinct control/forcing/discretization semantics are unchanged and are
not isolated by the root difference metrics. The VCA still receives only the
selected filter LP endpoints, reconstructed piecewise linearly. This sweep and
external attack/release are a software fixture, not a recovered hardware
modulation law, voice model or jointly integrated whole-note reference.

## Endpoint, clock and unit contract

The signal header is exactly `sample,value`. The VCA-control header is exactly
`sample,lc,ei,envelope`. Both have exactly N contiguous canonical indices
starting at zero. Every line, including the last, must end in LF (CRLF is
accepted). Values are finite representable ASCII decimals with no spaces,
quoting, extra columns, blank rows, hexadecimal syntax or underscores. Source
input, LC and EI lie in [-8,8]; envelope lies in [0,1]. EI is octaves; envelope
is unitless. No hardware voltage calibration is introduced.

N endpoints define N-1 intervals. The single explicit rate is both the
reference endpoint clock and the optional production stages' **internal** clock,
not a separately inferred host rate. All stages see the same times i/rate. There
is no engine oversampling, decimator, note event, warmup or lifecycle. Endpoint
span (N-1)/rate differs from WAV playback duration N/rate.

The reference filter integrates the piecewise-linear source. Its fine LP
endpoints are passed unchanged at 17-digit precision to the VCA, which treats
them as a new piecewise-linear input. The filter's continuous trajectory between
endpoints is **not** supplied to the VCA solver. This interstage approximation is
not jointly integrated and has no chain-wide refinement bound. Increasing solver
substeps improves each conditional component calculation but does not refine
the LP endpoint clock. This workflow never inserts new source/control endpoints.
The separate [endpoint-clock sensitivity study](REFERENCE_CHAIN_CLOCK_STUDY.md)
inserts declared PWL midpoints and compares three reference-only paths at their
common original endpoints. It is a sensitivity diagnostic, not an interstage
error bound or a jointly integrated chain.

If either reference or production LP exceeds the VCA input bound [-8,8], the
entire chain is rejected. There is no auto-level adjustment. Choose a smaller
explicit source fixture or different declared filter controls and start a new
experiment. LP-to-VCA unit identity is an explicit uncalibrated assumption.

## Routing and preserved diagnostics

- `filter/` is the unchanged filter run, including its original reference trace,
  manifest, input snapshot and optional production comparison/WAVs. Dynamic
  runs retain the exact `controls.csv` snapshot and `control_endpoints.csv`
  requested/mapped endpoint audit. Their scalar manifest controls describe only
  the initial endpoint, not the complete trajectory.
- `vca_reference_input/` receives reference LP. Its `output` column is the selected
  **reference-filter → reference-VCA** chain. Optional `realtime_output` here is
  only a hybrid diagnostic and is not selected as full production.
- With comparison, `vca_production_input/` receives actual production LP. Its
  `realtime_output` column is the selected **production-filter → production-VCA**
  chain. The reference calculation on that input is another retained hybrid
  diagnostic. Production float LP values round-trip exactly through the 17-digit
  interstage file, then the VCA uses its existing float conversion.
- `chain.csv` contains selected endpoint outputs and, with comparison,
  `production_minus_reference`. The root manifest's `selected_outputs` gives the
  exact source trace/column for each, plus optional `reference.wav` and
  `production.wav` at the root.

All original Newton checks, filter h/h2 metrics, VCA h/h2 metrics and per-stage
production differences remain in their component manifests. Root comparison
metrics are maximum absolute difference, absolute RMS, and relative RMS against
reference energy (null for zero energy), over all endpoints. They are descriptive
only, not a compounded refinement gate, hardware error or whole-engine parity.
Production/reference differences include the existing stage laws, float
arithmetic, forcing and discretization. This wrapper changes none of them.

## Provenance, bounds and safe output

The workflow reads each source once into a bounded snapshot. `inputs/` preserves
exact source/VCA-control bytes, optional `filter_controls.csv`, and the generated
17-digit VCA endpoint files. The filter receives this exact optional snapshot,
never a reserialized trajectory or a later read of its original source path.
`tools/` preserves the exact filter/VCA binary snapshots that are actually
executed, plus a workflow-source snapshot. The root manifest records their
SHA-256 and byte counts, every component output's SHA-256/size, deterministic
relative command arguments and selected signal paths. It checks component schema,
clock/count, input snapshot hashes and trace drives before publication. Dynamic
filter runs also verify the control snapshot, count/rate/hash, artifact names,
audit schema and requested Hz/Q at every endpoint. The root `filter_controls`
metadata identifies the exact snapshot and endpoint audit, preserving native
interpolation and initial-only scalar metadata semantics. The
root manifest itself is the completion marker and is not self-hashed.

These are binary-byte hashes, not guesses based on matching source paths or a
checkout revision. Existing configure-time source hashes remain in the component
manifests without reinterpretation. Copied executables must run from the staging
location on the current host: external dynamic libraries, runpaths, Python and
OS are not bundled. This is not a portable runtime image or binary-authenticity
certificate. Only use trusted, locally built stage tools. Same tools/options/input
on the same runtime produce deterministic bundle bytes; cross-platform math last
bits can differ.

Bounds: integer rate 8000..384000 Hz, 2..100001 endpoints, endpoint span <=10 s,
64 MiB per input/binary/artifact, original 128-byte signal/filter-control and
256-byte VCA-control line limits, and at most ten million aggregate coarse+fine
integration steps (unchanged for dynamic controls):

```
3 * (N-1) * (filter_substeps + vca_steps * (2 if comparing else 1))
```

Each child is limited to 120 seconds (at most three children), independent of
the numerical budget. Child stderr is continuously drained with only its last
16 KiB retained for failure messages; stdout is discarded. There is no unbounded
log capture or log file. Refinement or Newton failures retain useful child error
text; no incomplete bundle is presented as successful output.

The destination's parent must exist. Existing destinations, including dangling
symlinks, are rejected. Work runs in a fresh private staging directory under that
parent. Only after every child exits successfully and schema/hash/trace/WAV
checks finish does the workflow exclusively create the destination, link in
artifacts without replacing files and install the root manifest last. This
requires hard-link support on that filesystem. Normal failures remove only
tracked owned files with unchanged filesystem identity and empty owned
directories. No recursive deletion occurs; unrelated additions/replacements
are preserved. A crash or concurrent external mutation can leave partial files,
so consumers must require process success, the completed root manifest and
matching artifact hashes. The publication is not a crash-durable transaction.

## Verification

`cs80_reference_chain_tests` exercises an independent 8-state linear filter
matrix plus exact exponential convolution for constant-gain VCA, including DC,
signed and zero sources; an independent time-varying nodal matrix plus
integrating-factor convolution with an external attack/release envelope;
constant-trajectory/fixed-control byte equality; actual production LP routing
and deliberately mutated production-only LP fixtures in both control modes;
exact snapshots/binary/artifact hashes; determinism; a 4,801-endpoint sweep with
shared explicit-gain WAV selection; unchanged component output bytes; argument,
input, scalar/trajectory conflict, range, final-endpoint production clamp, child
failure/schema/hash/count/control-audit and timeout rejection; existing output
protection; and injected publication failure retaining an unrelated file.

```sh
cmake --build build/reference --target cs80_float_wav_fixture
ctest --test-dir build/reference --output-on-failure \
  -R 'cs80_(reference_chain|truth_render|vca_reference|float_wav)_tests'
```

See [TRUTH_RENDER.md](TRUTH_RENDER.md), [VCA_REFERENCE.md](VCA_REFERENCE.md),
[IG00156_NODAL_MODEL.md](IG00156_NODAL_MODEL.md) and
[IG00151_VCA_MODEL.md](IG00151_VCA_MODEL.md) for the component equations,
initialization, evidence, inferences and acceptance limits.
