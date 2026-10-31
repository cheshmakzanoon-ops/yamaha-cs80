# Native review of the 25 unresolved tone cells

2026-10-31. Original Yamaha service-manual scans retrieved by
[Circuit source inspection run 41224106605](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41224106605),
at `0281af873389a13452ac5dca2ef9b90dcec07c72`. All ten jobs succeeded.
The eight tone pages are 5100 × 3300 pixels. Their native tiles were decoded
from the job logs and every PNG was checked against its emitted SHA-256.
Reading was from these pixels, not OCR or an enlarged thumbnail.

## What the inspection establishes

The table transcribes the marked component at each address and preserves the
distinction between reading a mark and establishing which revision belongs in
the model. A legible replacement next to a revision triangle does not identify
the applicable serial-number range. Crossed symbols do not establish whether a
diode, jumper, or resistor was installed in a particular instrument.

All 25 cells therefore remain explicitly labelled hypotheses in
`FactoryToneHypotheses.h`. The frozen `cells` array, its median excitation law,
the classified `HardwareControl.h` table, and fingerprint
`e5f877a025fc52c6` are unchanged. No V1 numeric value changes. No hardware
measurement is claimed.

## Cell-by-cell transcription

`K` means the printed kilohm marking. “Direct diode” describes the drawn
branch without a readable series resistor, not an installed-part assertion.
Outputs refer to the unprimed row number at the right of each schematic.

| Cell | Schematic / paired board page | Native-pixel reading | Remaining qualification |
| --- | --- | --- | --- |
| T51 A20 | S31 / S32 | `1.2K`, with revision triangle | Value legible; revision applicability unresolved. |
| T51 A25 | S31 / S32 | Overdrawn direct-diode symbol, revision triangle | Altered symbol/topology remains a hypothesis. |
| T51 C25 | S31 / S32 | `10K` above a marked parallel/bypass drawing before the diode | Whether the resistor or bypass applies remains unresolved. |
| T51 E22 | S31 / S32 | `12K`, revision triangle | Value legible; revision applicability unresolved. |
| T51 E26 | S31 / S32 | Marked/overdrawn direct diode, triangle | Altered branch remains a hypothesis. |
| T51 F22 | S31 / S32 | Crossed `3.6K`, `12K` written below | Both readings visible; applicable revision unresolved. |
| T51 F25 | S31 / S32 | Crossed resistor marking, consistent with the retained `5.6K` candidate, and an overdrawn diode | Strike-through obscures the leading glyph; no stronger reading is promoted. |
| T51 F26 | S31 / S32 | Marked direct diode, triangle | Revision/topology unresolved. |
| T52 A26 | S33 / S34 | Overdrawn direct diode, triangle | Revision/topology unresolved. |
| T52 B9 | S33 / S34 | Crossed `12K`; `10K` below | Both readings legible; applicable revision unresolved. |
| T52 C22 | S33 / S34 | Crossed former value; clear `12K` below | Former value not reliably readable; retain labelled hypothesis. |
| T52 E20 | S33 / S34 | `1.2K`, triangle | Value legible; revision applicability unresolved. |
| T53 A20 | S35 / S36 | `1.2K`, triangle | Value legible; revision applicability unresolved. |
| T53 A25 | S35 / S36 | Overdrawn direct diode, triangle | Revision/topology unresolved. |
| T53 B14 | S35 / S36 | Schematic first glyph remains ambiguous before `5K`; S36 board artwork prints `75K(39K)` at the candidate footprint | Board reading is clear; trace/paired-board convention is an inference. Keep the frozen 25K-looking alternative and separate 75K board candidate. |
| T53 B20 | S35 / S36 | `1.2K`, triangle | Value legible; revision applicability unresolved. |
| T53 C20 | S35 / S36 | `1.2K`, triangle | Value legible; revision applicability unresolved. |
| T53 E23 | S35 / S36 | Crossed `16K`; `22K` below | Both readings legible; applicable revision unresolved. |
| T53 E26 | S35 / S36 | Crossed/overdrawn direct diode, triangle | Revision/topology unresolved. |
| T53 F22 | S35 / S36 | `12K`, triangle | Value legible; revision applicability unresolved. |
| T53 F26 | S35 / S36 | Crossed/overdrawn direct diode, triangle | Revision/topology unresolved. |
| T54 A26 | S37 / S38 | Crossed/overdrawn direct diode, triangle | Revision/topology unresolved. |
| T54 C22 | S37 / S38 | Crossed `8.2K`; `12K` below | Both readings visible; applicable revision unresolved. |
| T54 C25 | S37 / S38 | Crossed `56K` and altered diode symbol | Resistor/direct alternatives remain hypotheses. |
| T54 C26 | S37 / S38 | Crossed/overdrawn direct diode, triangle | Revision/topology unresolved. |

The lower-row readings are on the native `r1-c1` and `r1-c2` tiles of each
schematic, with overlap into `r0-c1`/`r0-c2`. T52 B9 and T53 B14 are on
`r0-c1`, with the output labels on `r0-c2`. T53's paired-board `75K(39K)`
is on `t53w-native-r0-c0`. Its proposed T53/T51 interpretation is supported
by neighbouring `20K(13K)` and `750(1K)` pairs, but this is not a printed
legend or proof of the historical installed revision.

## Source identity and reproducibility

All pages are under the original scan collection at
`https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/`.
The workflow logs include the complete URL, byte count, source SHA, crop
coordinates, PNG SHA and base64 payload. No downloaded scans are committed.

| Page | Workflow job | Source SHA-256 |
| --- | --- | --- |
| S31, T51 circuit diagram | [124604536250](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41224106605/job/124604536250) | `009384403e4c4b8e855daf6153219b3a94e6904d1f605393f3eda27968b9e014` |
| S32, T51 board/wiring | [124604536475](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41224106605/job/124604536475) | `8b95a93a2466865b01fea466b7613bd91f34714cccee49b381f819aff7c79f13` |
| S33, T52 circuit diagram | [124604536252](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41224106605/job/124604536252) | `7608294846030c6efa89b5e248c48964c9f3ab5b7333fb920a5db5f70063f113` |
| S34, T52 board/wiring | [124604536285](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41224106605/job/124604536285) | `604751efb4a4732d0ab15e47de1d6a9e22c8770a45887913323568feba2be8f5` |
| S35, T53 circuit diagram | [124604536281](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41224106605/job/124604536281) | `204b97b6b408bfff051f978dfc9cc0cd70933b8ac80c941e6440f10325c19443` |
| S36, T53 board/wiring | [124604536223](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41224106605/job/124604536223) | `9a906d74022b923842f11462af1931740c847f2f21e0bb53a95a3158411ce4f4` |
| S37, T54 circuit diagram | [124604536310](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41224106605/job/124604536310) | `fd149fd9b1ca9be5fb39d04d3d0bb2cf6c6ecc95834c755c778511536dfac227` |
| S38, T54 board/wiring | [124604536123](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41224106605/job/124604536123) | `d1f3749203eb9cec8dc177a09321bd3116bfc34faa1454f18bd5959f3ae19423` |

Tile boxes are `[0,0,2264,2264]`, `[2136,0,4464,2264]`,
`[4336,0,5100,2264]`, `[0,2136,2264,3300]`,
`[2136,2136,4464,3300]`, `[4336,2136,5100,3300]`.
The 64-pixel overlaps preserve row tracing across tile boundaries.
