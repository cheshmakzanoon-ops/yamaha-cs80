# Native preset matrix review: T51/T53 sampled columns

**Current full-table status:**547 classified /25 unknown across22 tone columns.
See [the full matrix audit](PRESET_FULL_MATRIX_AUDIT.md). The150/6 counts below
refer specifically to the original six-column subset and its historical promotion.

2026-10-29; source inspection at commit `e912db1f7877b48d07463e0f5a4d3a915f64b981`.
**No production audio changes. One independently verified source-matrix correction: T51/C/12, 15K → 13K.**
These are visual readings of Yamaha scans, not measurements. Independent review
on 2026-10-29 confirmed all 84 unmarked supplemental cells without transcription
disagreements. Only those 84 are promoted into `HardwareControl.h`; six marked
revision cells remain explicitly unknown. No production audio or preset parameters change.

## Recheck of the original 66 entries

All 33 T53 entries for A/C/D and rows 7–15,18–19 agree with the native
schematic. Of the corresponding 33 T51 entries, 32 agreed with the original
ledger; the remaining entry has now been independently corrected:

- **T51/C/12**, Brass 1 AL: native print is **13 kΩ**, not the **15 kΩ**
  previously in the ledger, implementation and duplicated test expectation.
  The primary reviewer traced C to output12; a second independent reviewer
  viewed the actual native tiles, compared handwritten 3/5 elsewhere on the
  page, and independently traced the same bus to12'.
- Source crop `t51-native-r0-c1`, approximately source x3260–3390,
  y1610–1730; output-number continuity is in `t51-native-r0-c2`.
  Independent confirmation was recorded on 2026-10-29. Source/crop hashes below
  identify the exact evidence.
- The source table and regression fixture now use13K. The explicit regression
  also checks the printed unit and illustrative divider value
  4.347826086956522V at Vs10.6V/Vd0.6V with one10k shunt and no receiver load.
  That arithmetic is not an authenticated AL voltage at the IC or a patch value.
- The initial reread agreed65/66; after this isolated correction all66 sampled
  entries agree with the inspected source. The 84 unmarked supplemental entries below have now also received independent
  visual verification; six marked revision cells remain excluded.
- T53/A/9 is indeed printed **750** without K; preserve Ω units.

Both drawings bear CS-80 S/#1001~ and KEC-4447-67. Revision triangles and crossed
components occur lower in the matrices. They are evidence that a source-revision
choice is necessary, not permission to silently combine revisions.

## Independently reviewed completion of the six sampled columns

Numerical cells are kΩ. `direct` includes the isolation diode.
`open` means no selected-column branch is drawn; it does not mean the bus is
unloaded or unconditionally zero. `unknown revision` must stay unresolved.

| Output/control | T51 A String1 | T51 C Brass1 | T51 D Flute | T53 A String2 | T53 C Brass2 | T53 D Brass3 |
|---|---:|---:|---:|---:|---:|---:|
| 1 Speed | open | open | open | open | open | open |
| 2 PWM | open | open | open | open | open | open |
| 3 PW | open | open | open | open | open | open |
| 4 Square switch | open | open | open | open | open | open |
| 5 Saw switch | direct | direct | direct | direct | direct | direct |
| 6 Noise | open | open | open | open | open | open |
| 16 VCF level | direct | direct | direct | direct | direct | direct |
| 17 Sine | 220 | 220 | open | open | open | open |
| 20 VCA sustain | 1.2* | 6.2 | 9.1 | 1.2* | 1.2* | 11 |
| 21 VCA release | 1.3 | direct | 3.9 | 4.7 | direct | 5.1 |
| 22 VCA level | direct | direct | direct | direct | direct | 1.8 |
| 23 Initial brilliance | 12 | 18 | 62 | 22 | 22 | 22 |
| 24 Initial level | direct | direct | direct | direct | direct | direct |
| 25 After brilliance | unknown revision | unknown revision | 62 | unknown revision | 10 | 10 |
| 26 After level | direct | direct | direct | direct | direct | direct |

`*`: 1.2K is legible, but the branch carries a revision triangle. Preserve
that marker and check the applicable service update before declaring a target
revision. The three starred readings are preserved as source observations only;
their implementation cells remain unknown, just like the three overdrawn cells.

**Promotion gate completed:** the independent reader confirmed the other84
unmarked classifications and local shunt observations with no disagreement.
Implementation coverage is now **150 known /6 unknown** among156 six-column
addresses: the original66 plus84 supplemental cells. At that historical checkpoint, all22 factory columns ×26 outputs contained
150 classified /422 unresolved addresses. The later full-table extension is
547/25; neither count is a fidelity score or calibrated electrical-state count.

The six unknown implementation locators are T51/A/20, T53/A/20, T53/C/20,
T51/A/25, T51/C/25 and T53/A/25. No revision-applicability assumption is made.

### Ambiguities requiring explicit resolution

- T51/A/25 and T53/A/25 show a triangle and a struck/overdrawn diode. Determine
  whether the service change deletes or otherwise alters the branch.
- T51/C/25 shows a marked 10K element with an apparent parallel bypass/alteration.
  Do not guess a zero-ohm bypass or retain 10K merely because the label remains.
- Other columns, not transcribed in this table, show crossed/replaced resistance
  values (e.g. T51/F/22, T53/E/23), and marked diodes in output26. A future full
  matrix must include revision status per cell.
- Rows4/5 have **no drawn 10K shunts** on these boards, unlike the analog rows.
  The generic illustrative10k helper must not be used to model these
  switch buses. The matrix-aware entry point now enforces this boundary. A missing switch branch is not proof of a zero-volt switch node
  until its receiver is traced.
- Top-row opens here are not evidence that all factory tones lack PWM/PW:
  other columns have branches. Do not generalize these six sampled tones.
- The control-name association comes from Panel1; the row numbers above were
  traced to the matrix terminals, not inferred from vertical spacing alone.

## Native source provenance

Run [40790769942](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/40790769942).
T51 job123175135239; T53 job123175135110.
All listed crops are native-size, without resampling.

### T51

[Original source](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/31%20-%20T51%20Circuit%20Diagram.jpg)

- Source SHA256: `009384403e4c4b8e855daf6153219b3a94e6904d1f605393f3eda27968b9e014`
- Source bytes: 1262128; dimensions: 5100 × 3300.

| Crop ID | Source box [left,top,right,bottom] | PNG SHA256 |
|---|---|---|
| t51-native-r0-c1 | [2136,0,4464,2264] | `27ae6687ad865d4c4ee88482f0aa314a0ec1df28c5a0267505bc9888805394b2` |
| t51-native-r0-c2 | [4336,0,5100,2264] | `393f88d6d508e00cba8856446a2553f1d75181ea8e31e3e62862acec60a7de92` |
| t51-native-r1-c1 | [2136,2136,4464,3300] | `68795583a18f17b6753c2eab8c36a162083374ac6a10725ce4315b115f184e38` |
| t51-native-r1-c2 | [4336,2136,5100,3300] | `d1627ec23e2648b71cb8e8db6d78699e0363a2de87599c37a502090f34db76d1` |

### T53

[Original source](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/35%20-%20T53%20Circuit%20Diagram.jpg)

- Source SHA256: `204b97b6b408bfff051f978dfc9cc0cd70933b8ac80c941e6440f10325c19443`
- Source bytes: 1206481; dimensions: 5100 × 3300.

| Crop ID | Source box [left,top,right,bottom] | PNG SHA256 |
|---|---|---|
| t53-native-r0-c1 | [2136,0,4464,2264] | `0cdc2b30a12ed70334c49169e73662aaeeebde636b575d008b98e79579c824ad` |
| t53-native-r0-c2 | [4336,0,5100,2264] | `b39c3c863d05836976ecbbeb25511d78b99160fcb621f9ef31c95fb37316c6e7` |
| t53-native-r1-c1 | [2136,2136,4464,3300] | `64540888ccb299995fdef132e93a85b0656445c0ee04f086ffea11795e6a0230` |
| t53-native-r1-c2 | [4336,2136,5100,3300] | `92e72ceed09e859fe29d71d287303279120871083cf5d475af67faac7182109a` |

## Next review gate

1. T51/C/12 independent review is complete; retain the isolated13K correction and its regression. Do not extrapolate its verification to the supplemental table.
2. Independent verification of the84 unmarked cells is complete, including
   row21's1.3K and switch classifications. Keep all six marked cells unknown.
3. Obtain the applicable revision explanation for marked branches.
4. The remaining sixteen columns have now been independently reviewed and their
   unmarked cells promoted; selection/receiver mapping remains unfinished.
5. Keep matrix voltages separate from normalized parameters and IC controls:
   neither this table nor its completion establishes authentic audible presets.

## Source-domain implementation and conditional solver boundary

Each output row now explicitly records board-specific local shunts: outputs4/5
are `LocalShunt::none` everywhere; the other24 rows on T51/T53 are
`LocalShunt::tenK`, while T52/T54 have no local shunts on any row.
Invalid boards/columns/outputs return unknown metadata.
This does not infer a second shunt on T52/T54 or the complete receiver impedance.

`solveMatrixBus(board,column,output,conditions)` preserves unknown branch status.
For no-shunt rows it returns `unresolvedLoading` without a voltage unless the
caller supplies an explicit positive finite linear ground load. This conservative
rule also applies to an ideal direct branch. With that load, only the supplied
load is used; no10k shunt is invented. Analog rows use their one local10k shunt
and an optional parallel caller load. Invalid conditions are rejected before
topology or revision status can produce a result.

The existing `solveBus(Branch,...)` remains explicitly a generic illustrative
one10k-divider helper, not an address-aware hardware model. Both paths assume an
ideal selected source, fixed supplied diode drop, no alternate sources and no
non-ground reference load. Neither computes a real switch threshold, diode I–V,
BA source impedance, envelope time, IC voltage conversion or authentic preset.

Regression fixtures independently enumerate all90 supplemental addresses:
84 known and6 unknown. They check printed units, all26 local-shunt declarations,
unresolved no-load switch buses, explicit-load results, invalid-load precedence,
unknown-address isolation, preservation of the original66 conditional results,
and the150/6 aggregate count. Hardware authenticity still requires receiver and
revision analysis, not only passing these software consistency tests.
