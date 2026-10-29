# M1 reference capture handoff

For the CS-80 owner and qualified technician: supply the records below with any
recordings offered as a reference. **No reference unit or calibrated capture is
currently available in this repository.** This is an acquisition request, not a
service procedure, a calibration certificate or a passing parity result. Start
with existing records and ordinary external audio; do not open the instrument to
complete missing fields.

Use [the intake template](../tests/parity/reference-intake.example.json) as a
non-executable checklist. `null` means unknown, not zero or not applicable. It is
intentionally **not** a version-1 comparator manifest. The current harness still
refuses every hardware comparison; see [PARITY_HARNESS.md](PARITY_HARNESS.md).

## Before recording or sharing

- Identify the unit: model, serial or stable private unit ID, owner-approved public
  ID, known modifications/repairs, and installed board/IC revisions where already
  documented. Include dated service/calibration records, procedure/document
  edition and pages, technician, instruments and their calibration status. Record
  unknown revisions explicitly; do not infer them from the model name.
- Supply known rail and trim records with their measurement dates, units and
  uncertainty, or mark them unmeasured. Record session UTC time, ambient
  temperature, warm-up duration and any tuning/calibration changes. Existing
  records may be stale; identify which apply to this session. Do not adjust trims
  merely to make software agree.
- Record permission for analysis, storage, publication of raw recordings,
  metadata/photos, and derived data separately; name the rights holder, evidence
  of permission, license/restrictions and attribution. Owning the instrument is
  not by itself a grant to redistribute another person's recordings or documents.
  Keep private identity/permission evidence out of public artifacts unless allowed.
- Agree on the planned control grid, repeat count, duration/tails, train versus
  validation setting groups, metric definitions, alignment method, uncertainty
  budget and justified acceptance limits **before viewing candidate residuals**.
  Commit/hash and date that plan. Leave limits unset until reviewed; never use
  the synthetic `1e-8` limits. Exploratory captures stay exploratory, not a
  retrospectively chosen hold-out. Group repeats of a setting together so they
  do not leak across training and validation.

## External audio first pass

Use only the instrument's intended external audio connections and equipment
operated within their documented specifications. Stop if equipment condition or
connection safety is uncertain and refer to a qualified technician. This packet
neither authorizes repairs nor provides electrical connection or live probing
instructions.

For each session, record:

1. A readable whole-panel photo and close-ups of every active control, selector,
   memory/preset choice, output switch and connected pedal/accessory. Add a written
   settings sheet with rank I/II separately; include actual markings/positions,
   not just software values. Date and hash photos; repeat after any change.
2. The complete signal chain: output connector/channel, cables/adapters/load,
   interface/recorder identity and firmware, input mode/impedance, gain/pad,
   coupling, hardware processing, recording software/version and routing.
3. Native sample rate, channel map, sample format/bit depth, sample-clock source,
   synchronization and clock-accuracy/drift evidence. State whether event timing
   shares that clock. Record latency measurement method and uncertainty; do not
   assume hardware latency equals the engine's 63 samples.
4. Level calibration evidence linking recorded digital values to volts at the
   stated connector, including gain, load, units (RMS/peak/peak-to-peak must not be
   interchanged), frequency dependence and uncertainty. An interface gain label
   alone is not calibration. If unavailable, keep native digital units and label
   the capture **uncalibrated exploratory external audio**.
5. Continuous original lossless files including pre-roll, note onset and full
   release tails. No normalization, gain matching, fades, DC removal, EQ,
   denoising, compression, limiting, clipping repair, resampling, trimming or
   lossy export. Record unavoidable acquisition processing; retain all originals
   and failed takes. Flag clipping, dropouts, overload and timing deviations.
   Any later derivative must have its own hash and explicit reproducible recipe.

### Earliest informative takes

Register exact settings and durations before taking these; the list defines
questions, not invented CS-80 slider positions or thresholds.

| Take | What to retain | What it can establish |
| --- | --- | --- |
| E0 baseline | No keys held, same chain/gain as E1, session noise and timing record | Acquisition health and output noise at this setup; not isolated component noise |
| E1 simple held notes | Saw-selected panel setup, rank I then II separately where supported by documented controls; attack, held portion, release, repeats and settings photos | Whole-path waveform/envelope repeatability; not isolated VCO/VCF/VCA calibration |
| E2 fixture-shaped gesture | Both saw ranks, pressure gesture below, continuous recording and actual key/pressure timing evidence | Candidate stimulus correspondence and mapping gaps; not sample-exact hardware equivalence |
| E3 controlled contrasts | One documented control changed at a time: wave selection, filter/resonance, envelope or pressure; repeat baseline between groups | Which measurement to prioritize next; validation only if preregistered and withheld |

Do not suppress natural hardware noise/drift to imitate the software. Record
voice assignment if observable; otherwise mark it unknown and retain repeated
notes rather than selecting the closest sounding take.

### E2 correspondence to the actual engine export

The source is `src/tools/parity_capture.cpp`, fixture
`w0-engine-stimulus-v1`; archive its exact revision/source hash, binary hash and
exported `capture.json` with both channel CSVs. That JSON, not this summary,
contains **all** submitted software parameters. It uses 48 kHz, 96,000 frames,
no discarded warm-up, and these events before rendering each zero-based sample:

| Host sample | Seconds | Software event on channel 1 |
| --- | --- | --- |
| 4,800 | 0.100 | Note 60 on, velocity 0.75 |
| 19,200 | 0.400 | Note 60 poly pressure 0.5 |
| 33,600 | 0.700 | Note 60 poly pressure 0 |
| 48,000 | 1.000 | Note 60 off |
| 57,600 | 1.200 | Note 67 on, velocity 0.625 |
| 72,000 | 1.500 | Note 67 off |

It selects both saw ranks, mix/volume 0.5, detune zero; software variation/noise,
chorus, ring and SUB depths are disabled. Each rank submits HP 25 Hz, LP 2400 Hz,
VCA ADSR 0.02/0.15/0.7/0.12 and pressure brilliance/level 0.25/0.2.
These are **software settings, not validated hardware positions or time laws**.
Record the physical key identities/tuning and actual gesture; MIDI note numbers,
normalized velocity and pressure are not a calibrated key force/voltage mapping.
Do not assume an original unit accepts MIDI or install a retrofit for this task.
A manual rendition without measured event/force mapping is exploratory only.

Keep a longer hardware recording if needed for tails. The two-second candidate
retains its output latency inside the file: it is not a 96,000-sample reference
plus a 63-sample tail. Never crop, stretch, correlate or gain-fit files silently
to force the synthetic comparison contract. Free-running phase and clock drift
may require different preregistered metrics and a reviewed ingestion schema.

## Internal measurements are a separate technician task

Only a qualified technician using a documented safe fixture and the applicable
manufacturer service procedures may plan internal acquisition. First provide
fixture review/sign-off, exact unit/board revision, document edition/page and
unambiguous node identification. CP2/CP3/TP1 are roadmap priorities, **not enough
to identify a safe connection**. No internal work is required for the external
first pass, and this document gives no live-mains probing instructions.

The first proposed stage set is oscillator waveforms, filter small-signal
response, envelopes and the source-verified CP2/CP3/TP1 observations. For each,
the technician must specify reference/return, probe and fixture loading,
bandwidth/coupling, calibrated scaling, stimulus/control conditions, simultaneous
channels/timing, rails/temperature/trims and uncertainty. Node mapping and safe
method must be reviewed before acquisition, not reconstructed from a WAV later.
No rail values, electrical stimulus levels, probe attachment sequence or pass
limits are prescribed here. Output-only recordings cannot substitute for these
stage measurements.

## Delivery and admission

Deliver one folder with the completed intake, preregistration, settings/events,
rights record, session/calibration/fixture records and untouched raw files.
Inventory each file with relative path, byte count and SHA-256; for audio add
channel map, rate, sample format and frame count. Preserve originals separately
from conversions. Record collector and reviewer, receipt time, unresolved fields
and disposition. Do not publish files until their specific rights allow it.

| Missing or adverse evidence | Admission decision |
| --- | --- |
| Unknown sharing rights | Do not redistribute; resolve permission before shared-corpus ingestion |
| Unidentified unit, unknown revisions/calibration, absent scaling/clock evidence or unknown settings | Quarantine as exploratory with explicit gaps; not a calibrated reference |
| Missing bytes, hash mismatch, clipped/processed-only audio, dropouts or untraceable edits | Reject for quantitative reference use; preserve failure record and request original/reacquisition |
| Missing preregistration or already inspected hold-out | Exploratory/training only; register a new untouched validation group |
| Internal nodes without qualified technician, safe fixture or verified source mapping | Do not acquire/admit as internal reference data |
| Complete packet | Eligible for human acquisition review only; not automatic acceptance or M1 completion |

A reviewer must confirm measurement uncertainty, authorized use and stage
coverage, then a separate reviewed schema/metric/tolerance implementation must
explicitly enable hardware ingestion. This handoff changes no harness gates and
makes no analog-fidelity claim. See [PARITY_ROADMAP.md](PARITY_ROADMAP.md),
sections 1, 7 and 8, for the final evidence standard.
