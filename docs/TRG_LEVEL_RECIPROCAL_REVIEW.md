# TRG level-output reciprocal wiring review

2026-10-29. **Source-level cross-check independently reviewed; no production change or hardware-continuity claim.**

## Question and result

R40 prints `RA1-TRG2-L14(51)` at both R51 TI4(62) and TA4(62′). Yamaha service page20, **TRG 1·2 Circuit Board & Wiring**, instead prints two distinct reciprocal L14 destinations:

| Page20 wiring column | Output | Printed destination | Wire color |
|---|---|---|---|
| TRG1-side, adjacent to board's left edge | L14(51) | RA1-R51-TI4(62) | RE |
| TRG2-side, farther left | L14(51) | RA1-R51-TA4(62′) | PK |

The column assignment is corroborated by explicit reciprocal supply destinations, not by assuming TI means TRG1. The board-adjacent column's −15(67) names TWS and TRG2; the outer column names TRG1 and TRG3. The corresponding right-side E(1) columns give the same TWS/TRG2 versus TRG1/TRG3 relationship. Thus the inner wiring pair corresponds to TRG1 and the outer pair to TRG2.

R40 gives the same RE wire for TI4 and PK wire for TA4. Page20 therefore supports **TRG1 L14 → TI4**, **TRG2 L14 → TA4**, and contradicts R40's TI4 upstream `TRG2` annotation. This is evidence of a cross-page labeling conflict, not permission to erase the original label or a measurement establishing the instrument's physical wiring. Do not short TI4 and TA4 together merely because R40 duplicates a driver label. Preserve both readings and identify page20 as the basis for any future provisional wiring choice.

As a nearby check, page20 L15(54) prints R51-TI5(63′), OR wire, on the TRG1-side and R51-TA5(64), BR wire, on the TRG2-side. This agrees with R40's separate TRG1-L15 and TRG2-L15 annotations; TI5/TA5 are not a duplicate-driver conflict.

## Consequence for the conditional level model

The 43-port fixture in `LEVEL_CONTROL_REFERENCE_DESIGN.md` can retain distinct supplied TI4/TA4 boundaries. These are electrical fixture ports, not a claim that all omitted upstream impedances or supply-mediated coupling are independent. Page20 resolves the reciprocal-document question in favor of separate destinations, but the R40 discrepancy remains recorded pending stronger evidence or hardware continuity. This review supplies no source impedance, IG00151 LI input-current law, gain calibration or transient model. Page19 was not required for this bounded connector-label comparison and was not inspected.

## Source and reproducible native crops

Primary Yamaha drawing hosted in the existing service-manual collection:

- [Page20, TRG 1-2 Circuit Board & Wiring](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/20%20-%20TRG%201-2%20Circuit%20Board%20%26%20Wiring.jpg).
- Local research input: `artifacts/w1-trg-level-20261029/trg20.jpg`.
- Downloaded 2026-10-29 over HTTPS; valid JPEG, 5100×3300 pixels.
- Original SHA-256: `31c369621611dd280d892c7caecf978349dad7b09acae30d0b2e471928d64637`.
- No scan is proposed as a repository payload. Crops are unresized Pillow PNG crops; boxes are `[left,top,right,bottom]` in original pixels.

| Native crop | Box | SHA-256 |
|---|---|---|
| `trg20-l14-l15-native.png` | `[560,1440,2220,1860]` | `bb7c3f44f0123d5b7244d84b3662326c9ecc659047549945789c547ff8083962` |
| `trg20-rank-identity-native.png` | `[560,2320,2220,2620]` | `dd51ca3b94ca00d774b48a1e2c33cf7eedc1ea10636510e06adfff0abb898812` |
| `trg20-power-identity-native.png` | `[3310,600,4990,800]` | `c6b5f336221c2acac23dd27c56be670f10baab1449400b1910465dc7dc308a66` |

Crops and local manifest are under `artifacts/w1-trg-level-20261029/`. R40 comparison uses the previously hash-verified original (`b76ad67725a661ab416121210f771e6f3b8c6ce57d2221d9dec81a3e79156db7`) and `r40-level-sources-native.png`, box `[1390,1680,2480,2370]`, SHA-256 `1e9c2b30e63c35195e35a6d5e548ad2c3558b83910638b899e5fcf090bb7e0a5`, documented in `LEVEL_CONTROL_REFERENCE_DESIGN.md`.
