# Tone-tab revision provenance: independent Yamaha parts-list cross-check

2026-11-02. **Finding: 0 of the 25 T51–T54 cell hypotheses promoted.**
The Yamaha CS-80 original parts list confirms board-family identifiers but
does **not** resolve any disputed installed resistor, diode, bypass or
revision-triangle alternative.

## Newly inspected primary source

- [Yamaha **CS-80 Parts List** (14-page scan)](https://www.therogoffs.com/cs80/cs80_parts_list.pdf),
  SHA-256 `eaea3c1bdc542b8e0d057b9a7b711aeda22a6d258562f5b5e05cd732a4aabe28`.
- Cover at PDF page 1: original print line **`12.77 1.5K S.F.`**
  and **Printed in Japan**. This is a document print/date mark, *not*
  a dated revision-effective notice for any installed tone resistor.
- PDF page 2 / printed p1 identifies the assembly view.
  PDF page 3 / printed p2, table **`CS-80 (S/# 1001 ~)`**,
  **Circuit Boards & Components**: exact relevant ref numbers,
  descriptions and board artwork identifiers:

| Ref on printed p2 | Named board | Printed board number | Inference allowed |
| ---: | --- | --- | --- |
| 10 | T51 | `#22612` | Pairs with T53 at the level of printed board identity |
| 11 | T52 | `#22621` | Pairs with T54 at the level of printed board identity |
| 15 | T53 | `#22612` | Does **not** pick a T53 B14 resistor alternative |
| 16 | T54 | `#22621` | Does **not** pick crossed-diode alternatives |

The separate printed **part numbers** occupy the `Part No.` column;
the paired `#22612/#22621` identifiers appear in the **Description**
column after `Circuit Board`. Do not conflate this board-family identity
with a numbered component footprint or serial-qualified board revision.
The header `S/# 1001 ~` describes the parts-list product scope, **not**
an effective serial for each redrawn component. The generic resistor
inventory at PDF pp4–5 / printed pp3–4 has part/type values but **no**
T51–T54 row/column application or effective-revision table.

## Cross-check against the 25 previously decoded native Yamaha drawings

The [original Yamaha service-manual index](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/index.html)
lists each circuit diagram beside its populated circuit-board/wiring
counterpart. The six unresized crops per page, image hashes, all 25
line-by-line transcriptions, and two independent tile verifications are
already retained in [TONE_CELL_NATIVE_REVIEW.md](TONE_CELL_NATIVE_REVIEW.md).
The parts list supplies **board-family corroboration** beyond those scans,
not a new numerical reading of an ambiguous glyph:

| Source pair | Explicit cells still requiring installed-version evidence | Unresolved |
| --- | --- | ---: |
| S31/S32, T51 / parts-list board `#22612` | A20, A25, C25, E22, E26, F22, F25, F26 | 8 |
| S33/S34, T52 / board `#22621` | A26, B9, C22, E20 | 4 |
| S35/S36, T53 / board `#22612` | A20, A25, **B14**, B20, C20, E23, E26, F22, F26 | 9 |
| S37/S38, T54 / board `#22621` | A26, C22, C25, C26 | 4 |
| **Total** | **No installed physical board or authoritative T-revision-to-serial mapping supplied** | **25** |

The S36 native board print **`75K(39K)`** at T53 B14's *proposed*
location remains readable. The parts-list pairing of T51 and T53
supports the existence of a common board family, but cannot say which
side of the parentheses belongs to which voice, board population, or
instrument serial; it also cannot repair the still-ambiguous leading
glyph before `5K` on S35. Similarly, crossed/overdrawn branches elsewhere
remain circuit-revision alternatives rather than proved installed parts.

The original Yamaha **Service News E-46 (January 1978)** in the
[36-page ADJ and service-bulletin source](https://www.therogoffs.com/cs80/CS80%20Adjustment%20Procedures%20and%20updates.PDF)
has serial criteria for the **IG00153 VCO temperature-rank/IS1555 diode**
countermeasure, **not** for T51–T54. A document dated in 1977 and an
unrelated bulletin dated in 1978 do **not** jointly establish a chronology
for the marked preset-tone changes. Broader source searches located no
authenticated Yamaha T51–T54 component-to-serial change order; this is a
bounded negative search result, **not a claim such a record cannot exist**.

## What would discriminate an actual installed revision

The owner's authorized **qualified, de-energized** board inspection,
board identity/date, physical serial, readable component and solder-side
photographs indexed to each address, or a manufacturer change order
explicitly giving T-board revision and effective serials. The generic
parts-list `#22612/#22621` identity by itself cannot discriminate any
of the 25. No physical observations have been made in this study.
Frozen `FactoryToneHypotheses.h`, V1 fingerprint
`e5f877a025fc52c6`, full-band test tolerances and all production
numerics remain unchanged.
