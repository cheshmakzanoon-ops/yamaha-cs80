# Reference-chain endpoint-clock sensitivity

`scripts/study_reference_chain_clock.py` is a separate Python 3.10+, POSIX,
stdlib-only numerical diagnostic around the unchanged
[`render_reference_chain.py`](REFERENCE_CHAIN.md) workflow. It uses only the
conditional filter and VCA reference paths. There is no runtime-DSP change,
production comparison, WAV export, CPU measurement, hardware qualification or
whole-instrument model.

## What it compares

Given N source/control endpoints at rate R, the study runs three chains:

| Run | Endpoint rate | Endpoints | Filter/VCA nominal substeps |
| --- | --- | --- | --- |
| `baseline` | R | N | F, V |
| `matched_step` | 2R | 2N-1 | F/2, V/2 |
| `finer_step` | 2R | 2N-1 | F, V |

F and V must both be even. Every component still runs its normal coarse/fine
check and exports the fine trace at twice its nominal substeps. Thus the selected
baseline and matched-step calculations have the same nominal integration time
step. The finer-step path halves that time step as well as doubling the endpoint
clock. All runs have the same endpoint span `(N-1)/R` and initial conditions.

The doubled files retain original endpoints at even indices and insert one
midpoint between each pair, computed as `left + (right-left)*0.5`, serialized
with 17 significant decimal digits. Refinement applies to the source, LC, EI,
external envelope, and, when present, **requested** HP Hz, LP Hz and Q. It does
not interpolate already mapped filter conductances, VCA gain, or derived LP.
The original binary64 endpoint values round-trip exactly. The requested PWL
functions are preserved up to floating-point interpolation and roundoff, not
exact real arithmetic. Original input file bytes are also retained unchanged.

The existing filter starts with zero states. VCA coupling starts at zero, with
the same explicit `--initial-li`, or the same first LC when it is omitted.
There is no invented prehistory. Direct LP-to-VCA numeric units, OTA level,
gain mapping and all component nonlinearities remain unchanged. In particular,
the VCA still reconstructs the filter's exported LP endpoints piecewise linearly;
the study does not supply a jointly integrated filter trajectory.

## Example

Build the two reference tools and create explicit signal/control inputs as in
[the existing chain example](REFERENCE_CHAIN.md#build-and-reproducible-example).
Then use a new output path:

```sh
python3 scripts/study_reference_chain_clock.py \
  --filter build/reference/cs80_truth_render \
  --vca build/reference/cs80_vca_reference \
  --input chain-signal.csv --controls chain-controls.csv \
  --rate 48000 --samples 4801 \
  --hp-hz 200 --lp-hz 2000 --q 1 --ota-level 2.5 --gain unity \
  --filter-substeps 16 --vca-steps 8 --out NEW_CLOCK_STUDY
```

For dynamic filter controls, replace all three `--hp-hz`, `--lp-hz`, `--q`
options with `--filter-controls chain-filter-controls.csv`. File schemas, strict
decimal parsing, units, gain modes, initialization and separate component
tolerance options are unchanged. `--compare-realtime` and `--wav-gain` are
rejected rather than silently ignored. No additional source/control values are
inferred beyond the explicit PWL midpoint rule.

## Common-clock metrics and interpretation

`common_clock.csv` has exactly N rows. Row i pairs baseline index i with index
2i from **each** doubled-clock run. Both the initial and final endpoints are
included. Odd refined endpoints are deliberately excluded from every aggregate
comparison. Integer index columns and time `i/R` make this mapping auditable.

The three differences are `matched_step_minus_baseline`,
`finer_step_minus_baseline`, and `finer_step_minus_matched_step`. Each reports:

- Maximum absolute difference.
- Absolute RMS: `sqrt(sum(difference²)/N)`.
- Relative RMS: `sqrt(sum(difference²)/sum(reference²))`, with JSON `null`
  when reference energy is zero. The denominator is baseline for the first
  two differences and matched-step for the third.

These are descriptive **endpoint-clock and numerical sensitivity** results.
They are not interstage-only errors, rigorous error bounds, Richardson
extrapolation, convergence orders or evidence of production equivalence. The
matched-step comparison controls nominal solver time steps, but changed
segmentation, interpolation rounding and LI-floor crossings may still affect
results. The finer-step comparison changes both solver step and endpoint clock.
No generic pass/fail threshold is applied to study differences.

Every filter Newton/convergence report and VCA coarse/fine report stays in its
original component manifest under the three run bundles. Those component guards
can reject a run; their successful completion does not certify chain-wide error.
The root manifest links to every component report rather than collapsing them
into a new numerical certification.

## Output and provenance

- `inputs/{signal,controls,filter_controls}.csv` preserves supplied bytes
  (the last file exists only for dynamic controls).
- `inputs/refined_*.csv` contains the declared 17-digit midpoint refinement.
- `tools/` retains filter/VCA executable bytes, the chain workflow and study
  source file bytes. A fresh in-process module is loaded from the retained chain
  source bytes, and all three runs execute copies of the retained binaries.
- `runs/{baseline,matched_step,finer_step}/` retains complete unchanged chain
  bundles, with their own inputs, binary/workflow snapshots, relative component
  commands, manifests and selected reference traces.
- `common_clock.csv` retains selected values, indices and signed differences.
- `manifest.json` records clocks, nominal and selected fine substeps, work
  limits, metric definitions, component report links, and SHA-256/byte counts
  for all retained files except the root manifest itself.

Each run is verified when it finishes. All retained run manifests, executable
snapshots and artifact hashes are checked again before publication so a later
stage cannot silently change an earlier run after its comparison values were read.

Hashes identify actual copied binary bytes and chain source bytes used in the
runs, not a claim that a checkout revision produced a binary. The study source
snapshot records file bytes, not a Python runtime closure. Original component
source hashes are retained without reinterpretation. Python, dynamic libraries,
runpaths, the OS and runtime are not bundled. Only use trusted locally built
executables that can run from a copied staging location. These artifacts are
not a portable runtime image or binary-authenticity certificate.

## Bounds and failure behavior

Validation happens before any study staging/output is created:

- Original rate 8000..192000 Hz; doubled rate at most 384000 Hz.
- Original count 2..50001; doubled count at most 100001; original endpoint
  span at most ten seconds.
- Even F in 2..256 and V in 2..128.
- Combined coarse+fine component work across all three runs at most ten million
  integration steps: `12*(N-1)*(F+V)`.
- Original and refined input snapshots plus binaries and workflow sources
  together at most 64 MiB. Existing per-file/line/finite-value bounds apply.

The retained bundle is limited to 512 MiB, checked before publication, including
its root manifest. Each read is bounded; the known fixed artifact inventory and
endpoint counts bound retained arrays and text. This is not an OS-enforced RSS
cap. Each child is limited to 120 seconds, with a shared 240-second study deadline
covering all six child invocations and checked before publication. The deadline
is not a hard real-time guarantee during bounded Python work or filesystem
operations. Child stderr is drained with only the last 16 KiB retained. On a
timeout the child's process group is terminated.

Existing outputs, including dangling symlinks, are rejected. A private staging
directory is created beneath the requested existing parent only after input and
work preflight. The final directory is created exclusively, files are hard-linked
without overwriting, and the root manifest is installed last. Normal failures
remove only tracked files/directories whose filesystem identities still match.
Unexpected additions or replacements are retained; no recursive deletion is
used. Abrupt process termination or host failure may leave staging files and is
not claimed to provide transactional recovery. Hard-link support is required.

## Verification

```sh
python3 tests/test_reference_chain_clock.py \
  --filter build/reference/cs80_truth_render \
  --vca build/reference/cs80_vca_reference
ctest --test-dir build/reference -R '^cs80_reference_chain(_clock)?_tests$' \
  --output-on-failure
```

Tests use independently evaluated PWL midpoint/source preservation, a linear
nodal-matrix filter and exponential-convolution VCA witness, dynamic controls,
common-clock metric recomputation, zero denominators, retained component
provenance, bounds and failure cleanup. They qualify this bounded software
diagnostic, not a hardware or whole-instrument model.
