# Preset matrix: bounded overall-foldout source review

2026-10-29. **Evidence only; no source-table, solver, production voicing or audio changes.**
The current implementation remains **547 classified / 25 unknown**. This pass supplies
additional original drawing evidence, not revision applicability or hardware validation.
Read alongside `PRESET_FULL_MATRIX_AUDIT.md` and `PRESET_PCB_UPDATE_REVIEW.md`.

## Coverage and outcome

The original **CS-80 Overall Circuit Diagram**, drawing label **002617**, contains
separately drawn T51, T52, T53 and T54 matrices. All four were inspected at native
resolution. Unlike detailed matrix pages31/33/35/37, these renditions do not display
the same triangle/cross-out annotations at the excluded branches. Absence of those
marks does **not** establish a later/earlier assembly, serial range, or an instruction
to remove/retain a branch. No preset-specific revision legend or serial applicability
was established in this bounded pass.

**No excluded cell is ready for promotion.** The most useful result is a much clearer
alternate drawing for T53/B/14, but it adds a source disagreement rather than erasing
one. A separate reader verified all three source hashes, six crop hashes and exact native pixels, including the B14 routing and additional foldout readings. Earlier detailed-page13K/marked1.2K comparisons rely on existing reviewed ledgers. Revision applicability remains unresolved, so this completed source review does not authorize promotion.

## T53/B/14: 75 kΩ corroboration, unresolved disagreement

- Foldout: the second input vertical is **B(92)**. Its **75K** resistor and diode
  connect to the bus terminating at **14(18)**. The adjacent B branches are **8.2K**
  at output13 and **3.9K** at output15; those must not be substituted. The label
  clearly reads **75K**, in kΩ, with no triangle or cross-out.
- Detailed page35: the same B branch between 8.2K and3.9K still appears **25K-looking**
  in native pixels to this reader. Do not re-label this scan75K merely because the
  other drawing agrees with the PCB.
- PCB page36: **75K(39K)** is clearly printed at the candidate footprint, corroborating
  the previous review's glyph observation. No explicit paired-board/value legend was
  found; this pass does not claim to have independently completed the PCB copper trace.

Thus the evidence is **foldout75K / PCB75K(39K) / detailed schematic25K-looking**.
It supports a75 kΩ source candidate but cannot establish the correct physical assembly
or justify overriding the detailed source. Keep T53/B/14 unknown.

## Revision-marked examples and a control conflict

These are readings from the foldout only, not accepted replacement values:

| Address | Foldout observation | Existing detailed-source boundary |
|---|---|---|
| T51/A/20 | 1.2K resistor/diode | 1.2K revision applicability unresolved |
| T51/F/22 | 12K resistor/diode | overwritten3.6K/12K |
| T52/B/9 | 10K resistor/diode | crossed12K/10K |
| T52/C/22 | 12K resistor/diode | overwritten former value/12K |
| T53/C/20 | **1.6K** resistor/diode | marked **1.2K**; preserve disagreement |
| T53/E/23 | 22K resistor/diode | crossed16K/22K |
| T54/C/22 | 12K resistor/diode | crossed8.2K/12K |

A useful control outside the25 exclusions is **T51/C/12**: the foldout visibly
prints **15K**, whereas the independently reviewed detailed page31 entry is **13K**
(`PRESET_NATIVE_MATRIX_REVIEW.md`). This is further evidence against importing the
foldout as an automatically authoritative replacement matrix. The accepted13 kΩ
entry is untouched. These documentary discrepancies do not identify which revision
was physically installed in any particular instrument.

## Reproducible provenance

Original URLs are under
<https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/>:

| Source filename | Bytes / dimensions | SHA256 |
|---|---|---|
| `cs80_foldout_schematic.jpg` | 30388464 /12185×5972 | `23de2fc4ef4bbbc6c171979aa586a2f1c1fb11d7f6116656153a752b31709b38` |
| `35 - T53 Circuit Diagram.jpg` | 5100×3300 | `204b97b6b408bfff051f978dfc9cc0cd70933b8ac80c941e6440f10325c19443` |
| `36 - T53 Circuit Board & Wiring.jpg` | 5100×3300 | `9a906d74022b923842f11462af1931740c847f2f21e0bb53a95a3158411ce4f4` |

Direct `curl -L --fail` retrieval of pages35/36 succeeded. Foldout bytes were already
retrieved in the parallel W2 pass. Crops below preserve native pixels, with PIL boxes
`[left,top,right,bottom]`; no enhancement or OCR is used as evidence. Local crops are
in ignored `artifacts/w9-preset-sourcepass-20261029/`; source foldout is in
`artifacts/w2-keymap-20261029/`. Downloaded primary-source images are not committed.

| Crop | Source | Box | PNG SHA256 |
|---|---|---|---|
| foldout-t51.png | foldout | [5580,3224,6360,4640] | `0d52f6c15111951155a8cd47d1058c72288a9eda95faa68d2440c07efaf89364` |
| foldout-t52.png | foldout | [6482,3254,7067,4640] | `aac47385b7d5235fc1c301a9627722401e1559edb2589f8dfe620e527d7fed45` |
| foldout-t53.png | foldout | [8103,2818,8919,4210] | `2da17bd932435afd58b0098b4ae04788cc701f17f677e3e543bc99e1f84be1ec` |
| foldout-t54.png | foldout | [8943,2866,9528,4228] | `999e08296b29f83c9bd63ca436dcd65e9d591f4c5b81b6125b97b4087440ded0` |
| t53-b14-detail.png | page35 | [2940,1670,3200,1900] | `de72a6d590e35f12d0c55c34263b20ecac87714212940216cdab7b3fe421507a` |
| t53w-b14-detail.png | page36 | [1070,1440,1510,1640] | `539b48406a2fe16043b7f3afc00358731dfaa735ecf58b10b7cd22a12af15cca` |

## Remaining gate / stopping condition

The bounded additional-image pass is complete: native coverage of all four alternate
matrices plus the B14 original schematic and PCB labels. Further rereading of the same
pixels cannot establish assembly applicability. Useful next evidence is an explicit
preset-board revision/assembly legend or bulletin, an independently traced and
identified PCB assembly, or resistor/topology measurements on a documented instrument.
Do not choose a source by majority vote, borrow neighboring values, conflate parentheses
with revisions, or alter factory voicings on this evidence.
