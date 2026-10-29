# W0 reference comparison plumbing

**This is a synthetic plumbing check, not a CS-80 model, engine render comparison,
calibration, or analog-fidelity result.** No hardware corpus exists. The four
stage names label mathematical test signals, not measurements of those circuits.
Parity-mode engine checks are separate from this harness.

## Run (Python 3.9+, standard library only)

From the repository checkout:

```sh
python3 tests/test_parity_harness.py
python3 scripts/parity_harness.py synthetic --out artifacts/parity-synthetic
python3 scripts/parity_harness.py compare \
  --manifest artifacts/parity-synthetic/manifest.json \
  --report artifacts/parity-synthetic/report.json
```

The generator requires Git and a checkout with a valid HEAD. It records that
revision **and whether the checkout was dirty**; a local run is not authoritative
exact-commit CI evidence. The source-byte hash identifies the actual generator,
including uncommitted changes. CI should archive all generated CSVs, manifest and
report with the exact source commit, plus the tolerance file. Parent build/CI
integration may register the unittest script with CTest and invoke `synthetic`
to produce archived artifacts. No NumPy, plotting package or hardware is needed.

Exit codes: **0** valid synthetic comparison passes; **1** valid comparison
exceeds a tolerance; **2** malformed/missing input or unavailable provenance.
An invalid comparison replaces an existing report with an explicit invalid/fail
record when the destination is writable; check the exit status, not merely the
existence of an old artifact.

## Deterministic fixture and alignment contract

The version-1 generator emits 4,096 samples at 48 kHz for an analytic 750 Hz sine,
a one-pole recurrence, a linear attack/release envelope and their product. These
are invented test signals, **not sourced Yamaha equations**. The candidate is
the reference delayed by exactly 63 samples plus an alternating ±1e-10
perturbation. This exercises nonzero residuals and explicit latency compensation.
63 samples illustrates the production decimator's declared latency but does not
measure that latency or exercise that decimator.

Each reference contains exactly `comparison_samples`; each candidate contains
exactly `comparison_samples + latency_samples`. Alignment compares reference
sample `i` with candidate sample `i + latency_samples`, retaining the complete
registered reference window. The only excluded samples are the declared
candidate prefix; their values are still parsed, hashed and checked for finiteness.
There is **no** automatic correlation alignment, gain matching, DC removal,
resampling, fractional delay, best-window selection or silent tail truncation.
A negative/fractional latency, missing tail or unequal window is an input error.
Future acquisitions needing those operations require a separately reviewed schema
and protocol, not a hidden fitting step.

Per stage, `report.json` publishes RMS error, maximum absolute error and absolute
mean signed error (bias), all in the declared units. Every limit must pass.
Unaligned RMS is reported separately as an alignment diagnostic, not a gate.
The report includes settings, train/validation split, source row, sample rate,
exact alignment window, thresholds and pass/fail for every stage. These elementary
time-domain metrics do **not** replace future complex response, harmonic/IMD,
noise, envelope timing or uncertainty analyses.

## Manifest version 1

`manifest.json` is the executable example; generate it rather than copying a
stale hash from documentation. It contains:

- `schema_version: 1`, `evidence_kind: synthetic | hardware`.
- `provenance`: nonempty instrument identity, board/IC revisions, rails,
  temperature, warm-up, trims, probe, scaling, redistribution status and source
  description. The synthetic generator explicitly marks physical fields
  `not applicable: synthetic mathematical fixture`, never invents a real unit.
- A full Git `source_revision`, Boolean `working_tree_dirty`,
  `generator_sha256` and a nonempty source-name-to-SHA-256 `source_hashes` map.
- A nonempty list of uniquely named `stages`, each with a positive integer
  `sample_rate_hz`, nonnegative integer `latency_samples`, integer
  `comparison_samples >= 2`, nonempty `settings`, `source_row`, `units`, and
  `split: train | validation`.
- Per-stage `reference` and `candidate` descriptors: corpus-relative `path`,
  lowercase SHA-256 and positive integer `sample_count`.

Captures are UTF-8 CSV with the exact header `sample,value`, zero-based contiguous
integer indices and finite numeric values. SHA-256 is checked against the exact
file bytes before parsing. Absolute paths, `..` traversal and symlink escapes
are rejected. Empty/missing files, duplicate JSON keys, JSON NaN/Infinity or
numeric overflow, missing provenance, duplicate/missing stages, count mismatch,
nonfinite CSV values and unknown/incomplete metric sets all fail closed.

The manifest records evidence claims but does not authenticate their truth or
prove a source revision contains a supplied hash. A later hardware-ingestion
process must review acquisition records, calibration, quantified uncertainty,
units/scaling and redistribution authorization, not merely fill these strings.
Hardware manifests are structurally recognizable for future extension, but
**all hardware comparisons are currently refused** even if their metadata is
present or a caller supplies a tolerance file labelled `hardware`.

## Tolerances and evidence boundaries

`tests/parity/tolerances-v1.json` is the versioned W0 tolerance profile. Its
1e-8 dimensionless absolute limits allow synthetic floating-point roundoff;
they are not voltage/audio acceptance limits and have no analog interpretation.
The report hashes both the entire manifest and tolerance file, as well as the
running comparator. The stage set must exactly match the profile, so deleting a
failing stage cannot silently produce a pass. The fixture marks every sample set
`validation`; no model fitting occurs. This is not a train/hold-out hardware
experiment and provides no operating-range coverage.

**Before any real reference is compared**, commit a separate, reviewed acquisition
schema and tolerance profile justified against measurement uncertainty. That
work must explicitly enable real-data ingestion; do not relabel this synthetic
profile or reuse its numbers. It also needs circuit/source rows, identified and
calibrated unit records, authorized captures, a predefined control grid,
train/validation separation and the appropriate stage-specific metrics from
`PARITY_ROADMAP.md` sections 1 and 7.

The tests deliberately mutate delay, amplitude and DC bias and recompute file
hashes: metrics must still fail. Other mutations prove that bad provenance,
nonfinite samples, damaged files, invalid thresholds and incomplete stage sets
cannot become passing reports. A green run establishes comparison plumbing only.

## Actual-engine candidate stimulus export (separate from synthetic acceptance)

`cs80_parity_capture` exports an **actual core-engine output**, not the synthetic
stand-ins above. It deliberately supplies no reference, tolerance, fidelity
score or passing analog result:

```sh
./build/cs80_parity_capture --out artifacts/parity-engine-candidate
python3 tests/test_parity_capture.py --capture ./build/cs80_parity_capture
```

The core-linked tool captures exactly 96,000 host frames (two seconds at 48 kHz),
including startup, the existing 63-sample output latency and release tails.
`left.csv` and `right.csv` use the comparison harness's `sample,value` format.
`capture.json` records every submitted core parameter by stable ID (including
values overridden internally by feature isolation), the channel file names,
counts, host/internal rates, declared latency and initialization policy. No
factory-preset or host state is loaded. The patch enables the uncalibrated
original-feature-isolation mode, uses both saw ranks, disables variation/noise,
chorus and ring, and includes nonzero original pressure-to-level/brilliance paths.
All remaining submitted default values are recorded explicitly, not left implicit
in the metadata. Changes to those defaults therefore change the artifact.

Events are applied **before** rendering the specified zero-based host sample:

| Sample | Event (MIDI channel 1) |
| --- | --- |
| 4,800 | note 60 on, velocity 0.75 |
| 19,200 | note 60 poly pressure 0.5 |
| 33,600 | note 60 poly pressure 0 |
| 48,000 | note 60 off |
| 57,600 | note 67 on, velocity 0.625 |
| 72,000 | note 67 off |

A fresh Engine receives `setParams`, `prepare`, then `reset`. No warmup samples
are discarded; initial silence, note-start phase/reset behavior and transients
are retained. Rendering blocks end at event boundaries. The existing deterministic
engine seed is used; this does not assert real oscillator reset/free-run behavior.
The export adds **no clipping, normalization, gain matching, resampling or latency
trimming**. Feature isolation bypasses the engine's output DC blocker and output
soft clip. The production decimator and provisional filter saturation remain;
this distinction is explicitly labelled in metadata. Units are raw engine
floats, not calibrated volts. CSV values use enough decimal digits to round-trip
binary32, with a fixed locale and newline format.

The seven CLI tests invoke two independent processes and require byte-identical
artifacts on the **same build/platform**, finite nonzero channel output, complete
sample counts, exact events/settings metadata, safe argument/destination failures,
and rejection of deliberately corrupted test artifacts. They are software export
checks, not a cross-platform bit-identity promise or analog acceptance test.

`capture.json` is intentionally **not** a reference-corpus manifest. It has no
hardware identity, acquisition rights or measured uncertainty. Before comparing
it to any reference, create a reviewed corpus manifest with source/binary/CSV
hashes, exact-commit provenance, units/scaling and an explicitly registered window.
The capture retains latency inside its 96,000 frames; it is not automatically a
96,000-reference-sample plus latency-tail pair. Do not silently truncate a
reference to force compatibility. Archive these candidate files with the exact CI
commit and binary provenance; a local capture alone is not authoritative evidence.
