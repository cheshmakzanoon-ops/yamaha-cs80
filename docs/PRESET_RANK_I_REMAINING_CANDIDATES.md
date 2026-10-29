# Remaining rank-I preset matrix: source candidates

Research date: 2026-10-29. **Candidate transcription only; no code, production
audio or preset parameters changed.** Every unmarked reading requires a second
independent visual review before promotion. Revision-marked cells remain
unknown even where a numeric label is legible. No physical instrument was measured.

## Scope and column identities

This document covers the remaining eight rank-I tone columns: T51/B,E,F and
T52/A,B,C,D,E, totaling208 tone/output addresses. First-pass coverage is
**199 unmarked candidate classifications and9 revision-unknown cells**:
T51 contributes73/5 of78; T52 contributes126/4 of130.
These are inventory counts, not calibrated voices or a sonic-fidelity score.

**T52 actually prints local columns A–F, not G–L or G–K.** Its five drawn active
columns are A–E; terminalF has no vertical selector branch network.
Do not substitute global tone-selector positions for local board lettering.

| Board/column | Tone correspondence inherited from original foldout ledger |
|---|---|
| T51/B | String 3 |
| T51/E | Electric Piano |
| T51/F | Clavichord 1 |
| T52/A | Harpsichord 1 |
| T52/B | Organ 1 |
| T52/C | Guitar 1 |
| T52/D | Funky 1 |
| T52/E | Funky 3 |

Local letters were directly checked in native board pixels. Tone-name
correspondence is inherited from [the original ledger](PRESET_CIRCUIT_LEDGER.md),
which attributes it to the original foldout; this reading did not substitute
an imagined A–K numbering scheme.

## Branch tables

Numbers are printed **kΩ**, except the explicit Ω entries.
`direct` means a drawn diode branch with no series resistor; the diode remains.
`open` means no branch is drawn from that selector column to that output.
Neither word assigns a normalized software parameter or unconditionally fixes
the bus voltage. `unknown revision` is not open or direct.

### T51: remaining three columns

| Output/control | B String3 | E Electric Piano | F Clavichord1 |
|---|---:|---:|---:|
| 1 Speed | open | open | open |
| 2 PWM | open | 150 | open |
| 3 PW | open | 20 | 4.3 |
| 4 Square switch | open | direct | direct |
| 5 Saw switch | direct | open | direct |
| 6 Noise | open | open | open |
| 7 HPF | open | 47 | 6.2 |
| 8 RESH | direct | 4.7 | 6.2 |
| 9 LPF | direct | 33 | 4.3 |
| 10 RESL | direct | 12 | 10 |
| 11 IL | open | open | open |
| 12 AL | 5.1 | 8.2 | 10 |
| 13 VCF attack | 75 | direct | direct |
| 14 VCF decay | 39 | 18 | 33 |
| 15 VCF release | 11 | direct | 3.3 |
| 16 VCF level | direct | direct | direct |
| 17 Sine | open | 10 | open |
| 18 VCA attack | 150 | 3.3 | direct |
| 19 VCA decay | open | 36 | 20 |
| 20 VCA sustain | 7.5 | 160 | 68 |
| 21 VCA release | 9.1 | 5.6 | direct |
| 22 VCA level | direct | unknown revision | unknown revision |
| 23 Initial brilliance | 18 | 15 | 82 |
| 24 Initial level | direct | direct | direct |
| 25 After brilliance | open | open | unknown revision |
| 26 After level | direct | unknown revision | unknown revision |

### T52: all five active columns

| Output/control | A Harpsichord1 | B Organ1 | C Guitar1 | D Funky1 | E Funky3 |
|---|---:|---:|---:|---:|---:|
| 1 Speed | open | open | open | 18 | 7.5 |
| 2 PWM | open | open | open | 110 | 120 |
| 3 PW | 7.5 | open | 13 | 6.2 | 8.2 |
| 4 Square switch | direct | open | direct | direct | direct |
| 5 Saw switch | open | direct | open | direct | open |
| 6 Noise | open | open | open | open | open |
| 7 HPF | 3.6 | open | 11 | 8.2 | 7.5 |
| 8 RESH | direct | direct | 6.8 | 13 | open |
| 9 LPF | 1.8 | unknown revision | 4.3 | 30 | open |
| 10 RESL | 1.5 | open | 27 | 11 | open |
| 11 IL | open | open | 220 | direct | direct |
| 12 AL | open | open | 20 | 1.6 | direct |
| 13 VCF attack | direct | direct | direct | 510 Ω | open |
| 14 VCF decay | direct | direct | 10 | 3 | open |
| 15 VCF release | direct | direct | direct | 13 | open |
| 16 VCF level | direct | direct | direct | direct | direct |
| 17 Sine | open | open | open | 9.1 | 3.6 |
| 18 VCA attack | direct | direct | 3.3 | direct | direct |
| 19 VCA decay | 36 | open | 16 | 11 | open |
| 20 VCA sustain | open | 2.7 | 51 | 10 | unknown revision |
| 21 VCA release | 3.6 | direct | 5.6 | 3 | 4.7 |
| 22 VCA level | direct | direct | unknown revision | direct | direct |
| 23 Initial brilliance | open | open | 22 | 6.8 | direct |
| 24 Initial level | direct | direct | direct | direct | direct |
| 25 After brilliance | open | open | open | 390 Ω | 16 |
| 26 After level | unknown revision | direct | direct | direct | direct |

Important unit checks: **T52/D/13 is printed510 without K, and T52/D/25 is
printed390 without K**. Read as510Ω and390Ω, not510kΩ/390kΩ.
T51/B/13 is75K; T51/F/20 is68K; T52/C/19 is16K. These are deliberately
highlighted for the independent reader, not asserted as already double-verified.

## Nine excluded revision cells

| Locator | Visible source observation | Interpretation boundary |
|---|---|---|
| T51/E/22 | Triangle beside12K branch | Legible value, revision applicability unknown |
| T51/F/22 | Overwritten/crossed3.6K with12K below and triangle | Do not choose revision |
| T51/F/25 | Marked/overdrawn5.6K branch and diode | Do not infer deletion, bypass or retained value |
| T51/E/26 | Marked/overdrawn diode | Do not classify direct or open |
| T51/F/26 | Triangle at diode branch | Do not classify direct or open without revision scope |
| T52/B/9 | Crossed12K with10K below and triangle | Do not choose revision |
| T52/E/20 | Triangle beside1.2K branch | Legible value, revision applicability unknown |
| T52/C/22 | Crossed former value with12K below and triangle | Former marking not needed to resolve present uncertainty |
| T52/A/26 | Triangle and struck/overdrawn diode | Do not classify direct or open |

T51 revision details are in `t51-native-r1-c1`; T52/B/9 is in
`t52-native-r0-c1`; other T52 branch exclusions are in
`t52-native-r1-c1`. The output-number tiles c2 independently identify the rows.

## Local topology and separate routing uncertainty

- On T51, outputs4/5 have no drawn10k shunt; the other24 outputs have the
  local10k shunt already reviewed in the six-column work.
- **T52 has no drawn local10k shunts on any of its26 outputs.** Its localE
  terminal is shown without the T51 vertical shunt network. That observation
  does not prove the connected instrument bus is floating: cross-board
  continuity and receiver loading are separate evidence.
- T52's primed output terminals1'–26' are crossed out and revision-marked
  at the right margin. Preserve this as **board-level alternate-input routing
  uncertainty**, distinct from the nine branch-value exclusions. It does not
  make an otherwise legible selected-column resistor unreadable, but it blocks
  assuming an unchanged primed-input topology.
- Do not copy T51's10k shunt into T52 local metadata. Likewise, do not conclude
  there is no total ground load on a connected T52 bus. A future matrix-aware
  solve needs an explicit, source-traced connected-bus model rather than
  either a duplicate shunt or a presumed floating node.
- Rows4/5 are switch branches, not ordinary analog-divider outputs. Their
  receiving logic thresholds and pull paths remain outside this table.
- These pages expose diode symbols and resistor values, not loaded BA output
  voltage, diode operating point, receiving-IC conversion, envelope durations
  or normalized slider positions.

## Native source provenance

Inspection commit `e912db1f7877b48d07463e0f5a4d3a915f64b981`;
[run40790769942](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/40790769942).
T51 job123175135239; T52 job123175134934.
Both source images are5100×3300; listed crops preserve native pixels without
resampling. T51 drawing KEC-4447-67; T52 drawing KEC-4448-67; both bear
CS-80 S/#1001~.

### T51

[Original source](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/31%20-%20T51%20Circuit%20Diagram.jpg)

Source SHA256: `009384403e4c4b8e855daf6153219b3a94e6904d1f605393f3eda27968b9e014`; 1262128 bytes.

| Crop ID | Source box [left,top,right,bottom] | PNG SHA256 |
|---|---|---|
| t51-native-r0-c1 | [2136,0,4464,2264] | `27ae6687ad865d4c4ee88482f0aa314a0ec1df28c5a0267505bc9888805394b2` |
| t51-native-r0-c2 | [4336,0,5100,2264] | `393f88d6d508e00cba8856446a2553f1d75181ea8e31e3e62862acec60a7de92` |
| t51-native-r1-c1 | [2136,2136,4464,3300] | `68795583a18f17b6753c2eab8c36a162083374ac6a10725ce4315b115f184e38` |
| t51-native-r1-c2 | [4336,2136,5100,3300] | `d1627ec23e2648b71cb8e8db6d78699e0363a2de87599c37a502090f34db76d1` |

### T52

[Original source](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/33%20-%20T52%20Circuit%20Diagram.jpg)

Source SHA256: `7608294846030c6efa89b5e248c48964c9f3ab5b7333fb920a5db5f70063f113`; 1102612 bytes.

| Crop ID | Source box [left,top,right,bottom] | PNG SHA256 |
|---|---|---|
| t52-native-r0-c1 | [2136,0,4464,2264] | `863a60e6c7c2bd93cd7ef93aaf985f02b303a427bb08429d4f1976eee278efa0` |
| t52-native-r0-c2 | [4336,0,5100,2264] | `9b54b80f1f4bd4b50a9c79ad947249722ee68811a6829953312339618bcce1a8` |
| t52-native-r1-c1 | [2136,2136,4464,3300] | `8c4a474c5636b3f19c1621aedd32657e3008aebad3a65bd4ad23d6c322d5ca00` |
| t52-native-r1-c2 | [4336,2136,5100,3300] | `f7140063dfe90ee97e897f9fd32d418c933b2e270da72a82700ac5c23785943a` |

## Promotion gate

1. Second reader independently traces all208 addresses, including opens and
   diode-only connections, against the actual native images.
2. Confirm units and all nine exclusion locators; retain uncertainty rather
   than reconciling source revisions by preference.
3. Review the T52 local-shunt versus shared-bus distinction separately before
   any matrix-aware solver integration. Do not modify production DSP.
4. After independent agreement, promote only unmarked source cells with
   fixtures preserving printed units, unresolved revision cells and local
   topology. Counts in this candidate document do not change implementation
   coverage on their own.
