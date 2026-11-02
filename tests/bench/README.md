# Yamaha CS-80 ADJ bench-observation intake (test-only)

This standalone, Python-standard-library tool accepts a **qualified
technician's future, separately authorized observation metadata**, validates
the exact **schema v1** below and prints a descriptive Markdown report.
It has **no hardware access**, cannot acquire audio or voltages, cannot
authenticate the technician, and does not declare any manufacturer
pass/fail result. Schema acceptance is **not** proof that the stated
measurement occurred. Do not treat schema-test placeholders as evidence.

```sh
python3 tests/bench/adj_intake.py --input /path/to/technician.json
python3 tests/bench/adj_intake.py --input /path/to/technician.json --output /path/to/new-report.md
python3 -m unittest discover -s tests/bench -p 'test_*.py' -v
```

`--output` creates a **new** file only; existing reports are never
overwritten. No **actual instrument** measurements, waveform files or
technician identities are supplied or committed here. Numeric unit-test
sentinels exist **only in memory** and are explicitly not hardware evidence.
Submit only documentation the owner authorizes sharing.

## Schema v1 contract (strict; no additional keys)

Root:

| Field | Required type and interpretation |
| --- | --- |
| `schema_version` | Exact integer `1` (not Boolean) |
| `record_kind` | Literal `cs80_adj_technician_observation` |
| `technician` | Object with nonempty `identifier`, `report_reference`, `authorization_reference` |
| `instrument` | Object with nonempty `serial`, `m_board`, `board_revision`, and `channel`: `I` or `II` |
| `snapshots` | Nonempty array, no repeated checkpoint; HP/LP checkpoints must follow their printed sequence when both are supplied (noise-reference record may appear anywhere) |

Each snapshot:

| Field | Required type and interpretation |
| --- | --- |
| `checkpoint` | `hp_as_found`, `hp_after_vr7_cp3_max`, `hp_after_vr8_cp3_setting`, `lp_after_vr9_cp3_max`, `lp_after_vr10_cp3_setting`, or `noise_reference` |
| `panel_setup` | Must match checkpoint: `as_found`, `yamaha_hp_p11`, `yamaha_lp_p11`, or `noise_reference` |
| `captured_at_utc` | Strict UTC `YYYY-MM-DDTHH:MM:SSZ` timestamp |
| `held_note` | `C1` for HP/LP checkpoints, `C1` or `not_applicable` for separate noise reference |
| `observations` | Nonempty array of distinct, explicit node statuses |
| `scope` | Required for any observed Vpp/RMS reading, optional for a DC-only observation; otherwise omitted. Instrument name, actual bandwidth (Hz), sampling rate (samples/s), time window (s), coupling `AC`/`DC` and grounding/reference description |
| `meter` | Alternative to scope for DC-only readings at FH/FL/IC10; optional when scope present. Nonempty instrument name, positive input resistance in ohms and mode `DC_V` |
| `connected_m_board_count` | Optional positive integer for NO1/NI shared load (if known); its absence is **unknown**, not zero |
| `control_reference` | Optional nonempty account of trimmer/control positions, without inventing a knob-to-gm/Q law |
| `ambient_temperature_c`, `warmup_minutes` | Optional finite numeric instrument context; warmup must be nonnegative |
| `notes` | Optional nonempty free text; treated as untrusted |

Each node observation uses `node`: `CP2`, `CP3`, `NO1`, `NI`,
`IC10_VFC4`, `IC10_QADJ6`, `FH` or `FL`. Status is **exactly one** of:

- `not_acquired`: requires nonempty `reason` and **forbids**
  `quantity`, `value_v`, `probe`, `evidence`.
- `observed`: requires `quantity`, finite numeric `value_v`,
  `probe` and `evidence`; **forbids** `reason`. Quantities are
  `vpp`/`vrms` for CP2, CP3, NO1 and NI;
  `vdc`/`vrms` for IC10_VFC4 and IC10_QADJ6;
  `vdc` for FH/FL. RMS and Vpp values must be nonnegative.

`probe`: positive `input_resistance_ohm`, nonnegative
`input_capacitance_pf`, positive `attenuation`. `evidence`: nonempty
`artifact_reference` and lowercase 64-hex-character `sha256`.
The tool checks the digest's **syntax only**; it does not open or verify
the referenced artifact. The report labels it unverified. Instrument
bandwidth, acquisition duration, channel/probe loading, and noise
statistics are capture conditions, **not supplied Yamaha acceptance limits**.

The importer refuses duplicate JSON keys, NaN/Infinity, extra fields,
wrong types, mismatched checkpoint/setup, repeated nodes/checkpoints,
missing acquisition instrument/probe/evidence for numeric readings,
invalid timestamp, and
negative Vpp/RMS. It **never supplies missing measurements**, estimates
NO1 source impedance, converts VR7/VR8 angles to gm/Q, substitutes CP2
for CP3, or infers which of the 25 T51–T54 revisions was installed.
Unmentioned nodes remain **not supplied**, distinct from explicit
`not_acquired`.

Yamaha ADJ printed p11 explicitly uses CP3, HP VR7 maximum then VR8
**4.3 ± 1 Vpp**, followed by LP VR9 maximum then VR10 **2.2 ± 1 Vpp**.
These are explanatory source references, not an intake gate. The current
frozen software HP **1.692 Vpp MISS**, V1 fingerprint
`e5f877a025fc52c6`, tests, production source and tolerances stay
unchanged. Hardware work remains qualified-technician/owner authorized,
as specified in
[YAMAHA_ADJ_QUALIFIED_TECH_MEASUREMENT_SPEC_20261102.md](../../docs/evidence/continuation-2026-10-31/YAMAHA_ADJ_QUALIFIED_TECH_MEASUREMENT_SPEC_20261102.md).
