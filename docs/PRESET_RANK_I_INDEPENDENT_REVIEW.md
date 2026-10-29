# Independent review: remaining rank-I preset cells

Reviewed2026-10-29 by a second reader, independently of the first-pass transcription. Scope: all208 T51 B/E/F and T52 A–E output addresses.

## Result

**199 unmarked source classifications agree;9 revision-marked addresses remain unknown. Zero table disagreements.** This verifies documentary source transcription, not physical-hardware measurements, loaded bus voltages, factory sounds or1:1 analog fidelity. No code, production audio, preset parameters or branch refs are changed by this review artifact.

The reader first decoded and viewed native tiles for both boards, recorded all208 readings, and only then fetched the candidate table at commit `6167ceefe5cd8b44752275670ceb77148ba93bd7`, path `docs/PRESET_RANK_I_REMAINING_CANDIDATES.md`, blob `b44f46f62852a16b630a67b482cae822513724f1`. A normalized comparison (open, diode-only, resistor value/unit, unresolved-revision) gave zero differences across78 T51 and130 T52 cells. Original candidate table remains unchanged.

## Specific confirmations

- T52/D/13: printed510, noK: **510Ω**, not510kΩ.
- T52/D/25: printed390, noK: **390Ω**, not390kΩ.
- T51/B/13=75kΩ; T51/F/20=68kΩ; T52/C/19=16kΩ.
- T52 physical column labels are A–E for active networks; F(96) has no vertical network. They are not G–K.
- Direct means a diode-only branch, never a wire. Open means no selected-column branch is drawn, not guaranteed zero output voltage.
- T51 local shunts:10k on24 rows, absent on switch rows4/5. T52 no drawn local shunts on any row. Do not duplicate the companion board's shunt or infer no total receiver/connected-bus load.
- T52 crossed/triangle-marked primed terminals1′–26′ are board-level alternate-input routing/revision uncertainty, not a reason to change readable unprimed branch classifications. No routing deletion/applicability claim is made.

## Retained revision exclusions

T51/E/22; T51/F/22; T51/F/25; T51/E/26; T51/F/26; T52/B/9; T52/E/20; T52/C/22; T52/A/26.

Every exclusion independently matches a triangle/crossed/overdrawn source component. Legible glyphs do not resolve revision applicability. T52/C/22 former text appears1.8K to this reader, with12K below, but that incidental glyph observation is not promoted as an accepted value. No service-update choice was inferred.

## Source and crop provenance

[Native inspection run40790769942](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/40790769942); T51 job123175135239; T52 job123175134934. All four c1/c2 upper/lower tiles for each board were decoded from CS80_CROP_DATA and viewed with original detail. Source/crop hashes below are workflow metadata, not newly recomputed local hashes.

### T51

[Source](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/31%20-%20T51%20Circuit%20Diagram.jpg); 5100×3300; 1262128 bytes. SHA256 `009384403e4c4b8e855daf6153219b3a94e6904d1f605393f3eda27968b9e014`.

| Crop | Source box | PNG SHA256 |
|---|---|---|
| t51-native-r0-c1 | 2136,0,4464,2264 | `27ae6687ad865d4c4ee88482f0aa314a0ec1df28c5a0267505bc9888805394b2` |
| t51-native-r0-c2 | 4336,0,5100,2264 | `393f88d6d508e00cba8856446a2553f1d75181ea8e31e3e62862acec60a7de92` |
| t51-native-r1-c1 | 2136,2136,4464,3300 | `68795583a18f17b6753c2eab8c36a162083374ac6a10725ce4315b115f184e38` |
| t51-native-r1-c2 | 4336,2136,5100,3300 | `d1627ec23e2648b71cb8e8db6d78699e0363a2de87599c37a502090f34db76d1` |

### T52

[Source](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/33%20-%20T52%20Circuit%20Diagram.jpg); 5100×3300; 1102612 bytes. SHA256 `7608294846030c6efa89b5e248c48964c9f3ab5b7333fb920a5db5f70063f113`.

| Crop | Source box | PNG SHA256 |
|---|---|---|
| t52-native-r0-c1 | 2136,0,4464,2264 | `863a60e6c7c2bd93cd7ef93aaf985f02b303a427bb08429d4f1976eee278efa0` |
| t52-native-r0-c2 | 4336,0,5100,2264 | `9b54b80f1f4bd4b50a9c79ad947249722ee68811a6829953312339618bcce1a8` |
| t52-native-r1-c1 | 2136,2136,4464,3300 | `8c4a474c5636b3f19c1621aedd32657e3008aebad3a65bd4ad23d6c322d5ca00` |
| t52-native-r1-c2 | 4336,2136,5100,3300 | `f7140063dfe90ee97e897f9fd32d418c933b2e270da72a82700ac5c23785943a` |

## Integration limit

Only the199 agreed unmarked source classifications may be considered for source-domain implementation, preserving explicit units and all9 unknown exclusions. Loaded-network topology, source impedance, diode behavior, receiver conversion and physical validation remain separate gates. This note is an unreferenced document-only commit based on20b3f16f2bca8b9b4016ee5db8380b741b62fc3e; overlay only the new document onto newer work.
