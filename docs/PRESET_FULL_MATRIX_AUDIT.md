# Full factory preset source-matrix audit

2026-10-29. **Source-domain data only: no production audio or factory preset
parameters changed. No physical CS-80 measured.** All22 tone columns ×26 named
outputs are represented. **547 cells are independently classified;25 remain
unknown.** This is not a claim of22 authenticated sounds or electrical1:1 fidelity.

## Coverage and review gate

| Board | Active local columns | Addresses | Classified | Unknown |
|---|---|---:|---:|---:|
| T51 | A–F |156|148|8|
| T52 | A–E |130|126|4|
| T53 | A–F |156|147|9|
| T54 | A–E |130|126|4|
| Total |22 tones|572|547|25|

The six-column baseline contributed150 classified cells. Independent second
reviews accepted199 additional rank-I cells and198 rank-II cells; the remaining
19 new exclusions join the original six. A classified cell can be absent
(`open`), diode-only (`direct`) or a resistor+diode branch; it is not necessarily
a resistor. Counts are not a sound-quality score.

Companion boards print A–E as their active local columns. T52/F and T54/F have
no drawn vertical tone network and are not extra tones. Global selector
positions must not replace local schematic lettering.

## All25 excluded addresses

Visible labels below describe evidence only, never accepted replacement values.

| Address | Reason held unknown |
|---|---|
| T51/A/20 | 1.2K revision applicability |
| T51/A/25 | overdrawn diode |
| T51/C/25 | marked10K/bypass topology |
| T51/E/22 | 12K revision applicability |
| T51/E/26 | marked/overdrawn diode |
| T51/F/22 | overwritten3.6K/12K |
| T51/F/25 | overdrawn5.6K/diode |
| T51/F/26 | marked diode |
| T52/A/26 | marked/overdrawn diode |
| T52/B/9 | crossed12K/10K |
| T52/C/22 | overwritten former value/12K |
| T52/E/20 | 1.2K revision applicability |
| T53/A/20 | 1.2K revision applicability |
| T53/A/25 | overdrawn diode |
| T53/B/14 | uncertain25K-looking glyph; readability gate |
| T53/B/20 | 1.2K revision applicability |
| T53/C/20 | 1.2K revision applicability |
| T53/E/23 | crossed16K/22K |
| T53/E/26 | crossed diode |
| T53/F/22 | 12K revision applicability |
| T53/F/26 | crossed diode |
| T54/A/26 | crossed diode |
| T54/C/22 | crossed8.2K/12K |
| T54/C/25 | crossed56K/diode |
| T54/C/26 | crossed diode |

T53/B/14 is a glyph-readability exclusion, not a presumed hardware revision.
Every other listed cell retains revision/topology applicability uncertainty.
Even a clearly readable1.2K or12K beside a revision mark is not promoted.

## Board-aware local loading, not inferred connected loading

`OutputRow::localShunts` has four explicit entries ordered T51,T52,T53,T54:

| Outputs | T51 | T52 | T53 | T54 |
|---|---|---|---|---|
|4/5 waveform switches|none|none|none|none|
|all other24 rows|one10k local|none local|one10k local|none local|

This replaces the former single row-level shunt assumption. No local shunt is
invented on T52/T54, and no second10k is added to the companion bus.
Conversely, “none local” does **not** establish zero total loading or a floating
bus in the assembled instrument. That requires continuity, selection-state and
receiver analysis.

T52/T54 primed terminals1′–26′ are crossed/revision-marked. Their applicability
and alternate-input routing are unresolved independently of the readable
selected-column branch data. No routing deletion or serial-number applicability
is inferred.

`solveMatrixBus` remains an offline conditional DC calculation. Unknown
addresses/cells return no voltage. For any no-local-shunt row, including every
T52/T54 row, it requires an explicit positive finite linear ground load and
otherwise returns `unresolvedLoading`. With a supplied load, only that load is
used; analog T51/T53 rows use their one local10k in parallel with any supplied
load. Direct branches also require explicit loading on no-shunt rows as a
conservative API gate.

The generic `solveBus` still means an explicitly assumed one10k divider.
Neither entry point models BA source impedance, diode I–V, alternate sources,
loads tied to non-ground references, receiver fan-out, IC conversion, envelope
times or switch thresholds. Caller-supplied loading is an assumption, not newly
verified hardware evidence. A computed voltage is not a calibrated preset.

## Data and regression preservation

- Original six-column ordering is retained at the start of `verifiedColumns`
  so the original66 and supplemental84 regression fixtures remain applicable.
- Remaining16 columns have a separate frozen416-address fixture using ohms,
  independent of the C++ source branch objects. All397 accepted entries and19
  exclusions are checked, including printed units.
- Native750Ω (T53/A/9),510Ω (T52/D/13),390Ω (T52/D/25) remain Ω entries;
  they are not mislabeled kΩ. Accepted numeric rank-II additions all use kΩ.
- Aggregate572-address and25-exclusion counts, canonical-address uniqueness,
  all572 local-loading declarations and invalid-load precedence are tested.
- Explicit no-hidden-shunt numeric fixtures cover T52's510Ω/390Ω branches and
  T54/C/20's330kΩ branch with a caller10k load.
- Software tests validate implementation consistency, not analog authenticity.

## Source and independent-review provenance

Native scans were inspected in
[run40790769942](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/40790769942)
at inspection commit `e912db1f7877b48d07463e0f5a4d3a915f64b981`.

| Board | Native job | Original source SHA256 |
|---|---|---|
|T51|123175135239|`009384403e4c4b8e855daf6153219b3a94e6904d1f605393f3eda27968b9e014`|
|T52|123175134934|`7608294846030c6efa89b5e248c48964c9f3ab5b7333fb920a5db5f70063f113`|
|T53|123175135110|`204b97b6b408bfff051f978dfc9cc0cd70933b8ac80c941e6440f10325c19443`|
|T54|123175135114|`fd149fd9b1ca9be5fb39d04d3d0bb2cf6c6ecc95834c755c778511536dfac227`|

Exact crop boxes/hashes and branch tables remain in these pinned review artifacts:

- [Six-column native review](PRESET_NATIVE_MATRIX_REVIEW.md).
- [Rank-I candidate table](https://github.com/cheshmakzanoon-ops/yamaha-cs80/blob/6167ceefe5cd8b44752275670ceb77148ba93bd7/docs/PRESET_RANK_I_REMAINING_CANDIDATES.md),
  blob `b44f46f62852a16b630a67b482cae822513724f1`.
- [Rank-I independent review](https://github.com/cheshmakzanoon-ops/yamaha-cs80/blob/6ad0a24fc52c82c94a1537736e984ecbd994f763/docs/PRESET_RANK_I_INDEPENDENT_REVIEW.md),
  blob `05dae6dd2de4898bd5285cd44fb50794bab0c024`:199 agreements,9 exclusions.
- [Rank-II source table](https://github.com/cheshmakzanoon-ops/yamaha-cs80/blob/71ffab56d8890549e3f11088938b9d3c558ac3f8/docs/RANK_II_PRESET_TRANSCRIPTION.md),
  blob `e9150192774b5cd031941f370b63684c1f373580`. Independent reader
  `verify_preset_matrix_completion` subsequently confirmed198 entries and
  retained the exact ten exclusions listed above; confirmation was supplied to
  the integrator2026-10-29. The pinned first-pass artifact retains its historical
  draft status; this audit records the completed review, not a silent rewrite
  of that earlier status.

No production circuit-derived factory bank is claimed. Next gates remain
service-revision resolution, connected-bus/receiver extraction, source/diode
operating points, documented IC transfer and service-calibration comparisons.
