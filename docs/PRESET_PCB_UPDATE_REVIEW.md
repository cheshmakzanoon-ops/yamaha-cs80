# Preset PCB and update-source follow-up

Status: evidence review only, 2026-10-29. No matrix cell, preset parameter, solver, or audio changes. All 25 excluded addresses remain excluded pending source reconciliation and independent review.

## Findings

- T53 PCB drawing page36, native r0c0, shows a legible **75K(39K)** at the footprint traced provisionally to B/output14. The schematic's previously uncertain25K-looking glyph is therefore not confirmed as25K. Keep B14 unknown until a second reader verifies the footprint-to-column/bus trace and reconciles the schematic glyph.
- T51 page32 and T53 page36 visibly reuse the same paired-value component-layout artwork. Examples:20K(13K) at C/output12,750(1K) at A/output9,75K(39K) at B/output14. Comparison with existing schematic transcriptions supports the hypothesis that outside values represent T53 and parenthesized values represent T51. This is **an inference, not an explicit legend** and must not be generalized to all brackets or revision marks. Parentheses must not be silently interpreted as old/new revision values.
- Both T51/T53 bottom notes identify PCB LC22612 and diode1S1555. The inspected notes do not explain parentheses, schematic triangle marks, or serial applicability.
- T52/T54 component-layout artwork is likewise paired. Both bottom notes identify PCB LC22621, diode1S1555, and J:J10. Their shown board terminals are unprimed and no extra panel-input diode/local10k column is shown. This supports the local-shunt distinction already recorded; it does not alone prove applicability of crossed primed schematic terminals or total assembled-bus loading.
- The reviewed update PDF physical pages31–32 are printed adjustment pages29–30: expression-wah, expression, and output-amplifier calibration/PRA layout. They do not resolve preset changes.
- Physical pages33–36 are Yamaha Service News E-46, January1978, four bulletin pages about pitch stabilization. They describe IG00153 thermal ranking and relocating the1S1555 diode onto the IC with silicone grease. These are not preset-board revision instructions.

## E-46 separate thermal evidence

Bulletin page2 lists CS80 serial1169 and later (October and later production), plus earlier regional units: US1116–1122,1124,1133–1147; Canada1148–1155; Northern Europe1033,1128,1131,1132,1159–1168. This applicability belongs to the thermal countermeasure, not preset revisions.

Pages3–4 classify interval changes on C1/C6 across25–50°C in5-cent bins (the prose uses an ambiguous degree/C-looking notation; the graph axes explicitly say cents). Page4 requires a common C6 rank and neighboring C1 ranks within one instrument; examples54/64 and23/13. The diode relocation is mechanical thermal coupling, not a changed M-board PCB. Do not invent a universal drift coefficient from these rank bins.

## Next gates

1. Independently trace T53 B14 on PCB and revisit original schematic glyph; only then propose a narrow correction.
2. Locate explicit paired-value notation/board assembly applicability before converting bracketed artwork into additional source entries.
3. Find preset-specific service bulletins or documented serial/assembly revisions for the marked24 other exclusions. E-46 does not supply this.
4. Trace T52/T54 unprimed bus wiring to T51/T53 and receivers; do not infer an extra10k or authentic receiver conversion from layout absence alone.

## Reproducible source inspection

GitHub Actions run40801679102; source commit c5d5476a2a47920419068a9f24dbf4bb7ce8278f. Jobs: T51w123210997016,T52w123210997022,T53w123210997002,T54w123210996778,PDF31–33 123210996985,PDF34–36 123210997064.

Raw source and viewed native-crop metadata follow. Native board r0c0 is[0,0,2264,2264]; r1c0 is[0,2136,2264,3300]; r1c1 is[2136,2136,4464,3300]. PDF full-native images are1391×1800.

```json
{"page": "t51w", "url": "https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/32%20-%20T51%20Circuit%20Board%20%26%20Wiring.jpg", "sha256": "8b95a93a2466865b01fea466b7613bd91f34714cccee49b381f819aff7c79f13", "bytes": 1569691, "width": 5100, "height": 3300, "purpose": "Original Yamaha schematic analysis; not measured hardware evidence"}
{"id": "t51w-native-r0-c0", "source_box": [0, 0, 2264, 2264], "width": 2264, "height": 2264, "resized": false, "png_sha256": "f4bd392574039c21ebf982c199c100bf7e05c7a843217d19fa289db412402407", "base64_length": 1903532}
{"id": "t51w-native-r1-c1", "source_box": [2136, 2136, 4464, 3300], "width": 2328, "height": 1164, "resized": false, "png_sha256": "3a607f87b5744dad3268ad919f69b00cb5be6234689804791a6b9a459e452542", "base64_length": 169868}
```

```json
{"page": "t52w", "url": "https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/34%20-%20T52%20Circuit%20Board%20%26%20Wiring.jpg", "sha256": "604751efb4a4732d0ab15e47de1d6a9e22c8770a45887913323568feba2be8f5", "bytes": 1399081, "width": 5100, "height": 3300, "purpose": "Original Yamaha schematic analysis; not measured hardware evidence"}
{"id": "t52w-native-r0-c0", "source_box": [0, 0, 2264, 2264], "width": 2264, "height": 2264, "resized": false, "png_sha256": "2f6b1d21f7236a50f1f4115d223c3b9b5f9d79c6c407b7eba00ef2565e645137", "base64_length": 1691704}
{"id": "t52w-native-r1-c1", "source_box": [2136, 2136, 4464, 3300], "width": 2328, "height": 1164, "resized": false, "png_sha256": "e5d24029b9dc3673cc55b9d58abeda60c4b48340b98d5f1532e03de4cc53fb0e", "base64_length": 153524}
```

```json
{"page": "t53w", "url": "https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/36%20-%20T53%20Circuit%20Board%20%26%20Wiring.jpg", "sha256": "9a906d74022b923842f11462af1931740c847f2f21e0bb53a95a3158411ce4f4", "bytes": 1675734, "width": 5100, "height": 3300, "purpose": "Original Yamaha schematic analysis; not measured hardware evidence"}
{"id": "t53w-native-r0-c0", "source_box": [0, 0, 2264, 2264], "width": 2264, "height": 2264, "resized": false, "png_sha256": "4cf8c532d1baa988ee0217b243be7c5433949d8f739071703d615fd57b9b9dd9", "base64_length": 2014472}
{"id": "t53w-native-r1-c0", "source_box": [0, 2136, 2264, 3300], "width": 2264, "height": 1164, "resized": false, "png_sha256": "ebfc0778e4a98a5f1020260b44965f191d5cd320a05004fa647030a9feb80f4d", "base64_length": 541364}
{"id": "t53w-native-r1-c1", "source_box": [2136, 2136, 4464, 3300], "width": 2328, "height": 1164, "resized": false, "png_sha256": "4f9e9c76d3c9fd39a98fd4090abd56b45aff8a68f4979e8fce410d1deb8f5132", "base64_length": 181792}
```

```json
{"page": "t54w", "url": "https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/38%20-%20T54%20Circuit%20Board%20%26%20Wiring.jpg", "sha256": "d1f3749203eb9cec8dc177a09321bd3116bfc34faa1454f18bd5959f3ae19423", "bytes": 1416928, "width": 5100, "height": 3300, "purpose": "Original Yamaha schematic analysis; not measured hardware evidence"}
{"id": "t54w-native-r0-c0", "source_box": [0, 0, 2264, 2264], "width": 2264, "height": 2264, "resized": false, "png_sha256": "ce18eb06703873be5f9a25e0e4aecede9c8f884fc04b4aa6738f02e1379c812f", "base64_length": 1749676}
{"id": "t54w-native-r1-c1", "source_box": [2136, 2136, 4464, 3300], "width": 2328, "height": 1164, "resized": false, "png_sha256": "2dd0d9e41b46a438913ce224169d242d7b9c89a4a007eb091e269b1efd0e5c03", "base64_length": 152452}
```

```json
{"page": "adjupdates_a-page31", "url": "https://www.therogoffs.com/cs80/CS80%20Adjustment%20Procedures%20and%20updates.PDF", "sha256": "eea101eb338494f3d1d3fddced2d8aa7f1d877ec9d6155ca44cc77ece88276ba", "bytes": 6351033, "width": 1391, "height": 1800, "purpose": "Original Yamaha schematic analysis; not measured hardware evidence"}
{"id": "adjupdates_a-page31-full-native", "source_box": [0, 0, 1391, 1800], "width": 1391, "height": 1800, "resized": false, "png_sha256": "7a93bac951f1ee28103f1cd2df2f3fe3291b6f2e2fbf8cc48e7a9de4193c2549", "base64_length": 283272}
{"page": "adjupdates_a-page32", "url": "https://www.therogoffs.com/cs80/CS80%20Adjustment%20Procedures%20and%20updates.PDF", "sha256": "eea101eb338494f3d1d3fddced2d8aa7f1d877ec9d6155ca44cc77ece88276ba", "bytes": 6351033, "width": 1391, "height": 1800, "purpose": "Original Yamaha schematic analysis; not measured hardware evidence"}
{"id": "adjupdates_a-page32-full-native", "source_box": [0, 0, 1391, 1800], "width": 1391, "height": 1800, "resized": false, "png_sha256": "74f5f2b577fa6be7c7dc6db21d74111d4cc3455b96f89a2ead55f7b4ce3774cf", "base64_length": 129240}
{"page": "adjupdates_a-page33", "url": "https://www.therogoffs.com/cs80/CS80%20Adjustment%20Procedures%20and%20updates.PDF", "sha256": "eea101eb338494f3d1d3fddced2d8aa7f1d877ec9d6155ca44cc77ece88276ba", "bytes": 6351033, "width": 1391, "height": 1800, "purpose": "Original Yamaha schematic analysis; not measured hardware evidence"}
{"id": "adjupdates_a-page33-full-native", "source_box": [0, 0, 1391, 1800], "width": 1391, "height": 1800, "resized": false, "png_sha256": "bd7b4fb209328c29b3b123a192c11354abbc08f98b89d56f5408a8298f2692ed", "base64_length": 469500}
```

```json
{"page": "adjupdates_b-page34", "url": "https://www.therogoffs.com/cs80/CS80%20Adjustment%20Procedures%20and%20updates.PDF", "sha256": "eea101eb338494f3d1d3fddced2d8aa7f1d877ec9d6155ca44cc77ece88276ba", "bytes": 6351033, "width": 1391, "height": 1800, "purpose": "Original Yamaha schematic analysis; not measured hardware evidence"}
{"id": "adjupdates_b-page34-full-native", "source_box": [0, 0, 1391, 1800], "width": 1391, "height": 1800, "resized": false, "png_sha256": "090cbac9158289a0f8799356f83b777ae5344e8e066a2fd028671ac484b459bd", "base64_length": 467172}
{"page": "adjupdates_b-page35", "url": "https://www.therogoffs.com/cs80/CS80%20Adjustment%20Procedures%20and%20updates.PDF", "sha256": "eea101eb338494f3d1d3fddced2d8aa7f1d877ec9d6155ca44cc77ece88276ba", "bytes": 6351033, "width": 1391, "height": 1800, "purpose": "Original Yamaha schematic analysis; not measured hardware evidence"}
{"id": "adjupdates_b-page35-full-native", "source_box": [0, 0, 1391, 1800], "width": 1391, "height": 1800, "resized": false, "png_sha256": "2464690fa3b285bce0d70847098056a5a58b65a62c50e8e01325b4c24928ce0f", "base64_length": 427056}
{"page": "adjupdates_b-page36", "url": "https://www.therogoffs.com/cs80/CS80%20Adjustment%20Procedures%20and%20updates.PDF", "sha256": "eea101eb338494f3d1d3fddced2d8aa7f1d877ec9d6155ca44cc77ece88276ba", "bytes": 6351033, "width": 1391, "height": 1800, "purpose": "Original Yamaha schematic analysis; not measured hardware evidence"}
{"id": "adjupdates_b-page36-full-native", "source_box": [0, 0, 1391, 1800], "width": 1391, "height": 1800, "resized": false, "png_sha256": "1fb82bae66834a355a48488ff725600d44b6341523e65beb52ef028042aa138d", "base64_length": 472924}
```
