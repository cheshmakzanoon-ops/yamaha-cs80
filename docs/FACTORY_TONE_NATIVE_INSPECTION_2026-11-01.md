# Native inspection of the 25 held tone-tab cells — 2026-11-01

**Result:** all 25 cells have been checked against retrievable, native-pixel
Yamaha CS-80 service-manual crops, but none can be promoted into the classified
V1 resistor matrix. Twenty-four are affected by revisions, crossed/overdrawn
symbols or unresolved topology; T53/B/14 additionally has a plausible **75K
board-artwork hypothesis**, not a confirmed schematic/PCB correspondence. The
source table stays **547 classified / 25 unknown**. This is photographic source
reading, not electrical measurement of a Yamaha instrument.

## Provenance and reproducibility

GitHub Actions `circuit-inspection.yml` ran `scripts/inspect_circuit_sources.py`
on the source JPEGs at original **5100 × 3300** resolution. On 2026-11-01 the
connected GitHub Actions job-log API returned the exact embedded native PNG
crops (`CS80_CROP_DATA`) and their `CS80_CROP_BEGIN` SHA-256 records. Previous
2026-10-31 access failures in [ws6-library.md](workstreams/ws6-library.md)
describe that earlier session, not the present one. The reconstructed image
fragments visually show the indicated marks; the digests below are the **log-
reported** hashes, not independent attestations of a physical specimen.

| Manual page and role | CI inspection job | Source JPEG SHA-256 (`CS80_SOURCE_METADATA`) |
| --- | --- | --- |
| S31, T51 schematic | [123175135239](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/40790769942/job/123175135239) | `009384403e4c4b8e855daf6153219b3a94e6904d1f605393f3eda27968b9e014` |
| S32, T51 board/wiring | [123210997016](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/40801679102/job/123210997016) | `8b95a93a2466865b01fea466b7613bd91f34714cccee49b381f819aff7c79f13` |
| S33, T52 schematic | [123175134934](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/40790769942/job/123175134934) | `7608294846030c6efa89b5e248c48964c9f3ab5b7333fb920a5db5f70063f113` |
| S34, T52 board/wiring | [123210997022](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/40801679102/job/123210997022) | `604751efb4a4732d0ab15e47de1d6a9e22c8770a45887913323568feba2be8f5` |
| S35, T53 schematic | [123175135110](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/40790769942/job/123175135110) | `204b97b6b408bfff051f978dfc9cc0cd70933b8ac80c941e6440f10325c19443` |
| S36, T53 board/wiring | [123210997002](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/40801679102/job/123210997002) | `9a906d74022b923842f11462af1931740c847f2f21e0bb53a95a3158411ce4f4` |
| S37, T54 schematic | [123175135114](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/40790769942/job/123175135114) | `fd149fd9b1ca9be5fb39d04d3d0bb2cf6c6ecc95834c755c778511536dfac227` |
| S38, T54 board/wiring | [123210996778](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/40801679102/job/123210996778) | `d1f3749203eb9cec8dc177a09321bd3116bfc34faa1454f18bd5959f3ae19423` |

The source JPEG URLs are the corresponding `t51`, `t51w`, ... `t54w` entries
in the script's fixed `SOURCES` map. The exact eight schematic crop IDs,
overlapping source coordinates, and PNG SHA-256 values are recorded as the
`nativeSchematicCrops` compile-time table in
[`FactoryToneHypotheses.h`](../src/core/FactoryToneHypotheses.h).
For each record below, `r0-c1` is native crop `[2136,0,4464,2264]` and
`r1-c1` is `[2136,2136,4464,3300]` (128-pixel vertical overlap); **neither
has been downsampled**. `nativeCellEvidence` maps all 25 addresses to these
eight hashes with compile-time completeness and deliberate-mutation checks.

## Cell-by-cell native transcription

`T51/...` means the T51 panel board, selector column and output number, not a
component designator. A written K value is an **image glyph/candidate** only.
"Held" means no applicable installed circuit value is proven; it remains an
`unknown` in `HardwareControl.h` and retains the explicit median hypothesis in
`FactoryToneHypotheses.h`. Board pages provide corroborating context, not a
general source-to-footprint mapping.

| Cell | Schematic / board | Native schematic crop | What is legible, and why held | Status |
| --- | --- | --- | --- | --- |
| T51/A/20 | S31 / S32 | `t51-native-r1-c1` | `1.2K` beside a revision triangle; serial applicability not established | Held hypothesis |
| T51/A/25 | S31 / S32 | `t51-native-r1-c1` | Overdrawn diode/direct symbol; original and revision cannot be separated | Held hypothesis |
| T51/C/25 | S31 / S32 | `t51-native-r1-c1` | Marked `10K` and bypass/direct topology both apparent; selection uncertain | Held hypothesis |
| T51/E/22 | S31 / S32 | `t51-native-r1-c1` | `12K` with revision mark; applicable version unresolved | Held hypothesis |
| T51/E/26 | S31 / S32 | `t51-native-r1-c1` | Diode symbol visibly modified/overdrawn | Held hypothesis |
| T51/F/22 | S31 / S32 | `t51-native-r1-c1` | Crossed/overwritten `3.6K` and `12K`; cannot elect one | Held hypothesis |
| T51/F/25 | S31 / S32 | `t51-native-r1-c1` | `5.6K` and overdrawn diode/direct alternatives | Held hypothesis |
| T51/F/26 | S31 / S32 | `t51-native-r1-c1` | Marked diode/direct path; update applicability unresolved | Held hypothesis |
| T52/A/26 | S33 / S34 | `t52-native-r1-c1` | Marked/overdrawn diode/direct path | Held hypothesis |
| T52/B/9 | S33 / S34 | `t52-native-r0-c1` | `12K` / `10K` crossed values; no definitive revision selection | Held hypothesis |
| T52/C/22 | S33 / S34 | `t52-native-r1-c1` | Replaced former glyph / `12K`; former and applicability unresolved | Held hypothesis |
| T52/E/20 | S33 / S34 | `t52-native-r1-c1` | `1.2K` associated with revision mark | Held hypothesis |
| T53/A/20 | S35 / S36 | `t53-native-r1-c1` | `1.2K` beside revision triangle | Held hypothesis |
| T53/A/25 | S35 / S36 | `t53-native-r1-c1` | Overdrawn diode/direct symbol | Held hypothesis |
| T53/B/14 | S35 / S36 | `t53-native-r0-c1` | Schematic has a **25K-looking, unresolved glyph**; S36 PCB is a separate candidate `75K(39K)` | Held; board candidate |
| T53/B/20 | S35 / S36 | `t53-native-r1-c1` | `1.2K` with revision triangle | Held hypothesis |
| T53/C/20 | S35 / S36 | `t53-native-r1-c1` | `1.2K` with revision triangle | Held hypothesis |
| T53/E/23 | S35 / S36 | `t53-native-r1-c1` | Overdrawn/crossed `16K` and `22K` | Held hypothesis |
| T53/E/26 | S35 / S36 | `t53-native-r1-c1` | Crossed diode/direct symbol | Held hypothesis |
| T53/F/22 | S35 / S36 | `t53-native-r1-c1` | `12K` beside revision triangle | Held hypothesis |
| T53/F/26 | S35 / S36 | `t53-native-r1-c1` | Crossed diode/direct symbol | Held hypothesis |
| T54/A/26 | S37 / S38 | `t54-native-r1-c1` | Crossed diode/direct symbol | Held hypothesis |
| T54/C/22 | S37 / S38 | `t54-native-r1-c1` | Crossed `8.2K` and `12K` | Held hypothesis |
| T54/C/25 | S37 / S38 | `t54-native-r1-c1` | Crossed `56K` / diode/direct alternatives | Held hypothesis |
| T54/C/26 | S37 / S38 | `t54-native-r1-c1` | Crossed diode/direct symbol | Held hypothesis |

The existing [ws6 re-examination](workstreams/ws6-library.md#re-examination-of-the-25-cells)
contains the candidate-selection convention and sensitivity scenarios. This
follow-up supplies retrievable crop provenance, not a changed numerical or
revision conclusion.

### Special caution: T53/B/14, board page S36

The crop `t53w-native-r0-c0`, original-pixel box `[0,0,2264,2264]`, PNG
SHA-256 `4cf8c532d1baa988ee0217b243be7c5433949d8f739071703d615fd57b9b9dd9`,
shows `75K(39K)` near the apparent output-14 footprint. Three nearby classified
pairs are consistent with T53 **unparenthesised** / T51 **parenthesised**:
T53/C/12 `20K` versus T51/C/12 `13K`, T53/A/9 `750` versus T51/A/9 `1K`,
and T51/B/14 `39K`. **This is a declared footprint-pairing inference**, not a
service-manual legend or installed resistor reading. Alternative explanations
include a mistaken footprint trace and a drawing revision that differs from
the S35 schematic glyph. The `75K` candidate, schematic's 25K-looking mark,
and frozen median hypothesis all remain distinct; a second independent board
trace/revision-key confirmation is the next gate.

### Reproduce and close the remaining uncertainty

1. Fetch the two linked GitHub Actions runs' job logs and extract matching
   `CS80_CROP_DATA` chunks between `CS80_CROP_BEGIN` / `CS80_CROP_END`; compare
   their `png_sha256` values with the header's crop registry.
2. The updated `circuit-inspection.yml` also runs **all eight** T51–T54
   schematic/board pages on push or dispatch (and preserves the original OE1
   pages), retaining exact native PNG artifacts for seven days.
3. Obtain a dated service revision/serial-number key and a second independent
   reader's schematic-to-PCB trace, especially for T53/B/14 and every marked
   diode. Until then, classification and V1 numerical gates remain frozen.

**Compatibility boundary:** no audio law, resistor value, tone-table numerical
manifest, tolerance, or v1 fingerprint (`0xe5f877a025fc52c6`) was changed by
this evidence-only review.
