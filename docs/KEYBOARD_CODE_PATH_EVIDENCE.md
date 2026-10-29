# Keyboard-code path evidence — 2026-10-29

Companion to [Keyboard contour circuit evidence](KEYBOARD_CONTOUR_EVIDENCE.md). No production key map is established and no hardware-calibration claim is made. Native images and crops were inspected locally in `artifacts/w2-keymap-20261029/` (untracked research scratch); the hashes and boxes below allow regeneration from the primary source.

Primary source base:
https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/

## Downloads and provenance

curl -sSL --fail succeeded for native images. Python urllib to the same encoded URLs returned HTTP 406; this is a client-specific download failure, not unavailable source evidence.

|Local filename|Original URL filename|SHA256|Dimensions|
|---|---|---|---|
|04.jpg|04 - KAS schematic.jpg|989e4755453bc030e01d329bd5ce58d2ee3461bb173131c7b8f0805c85489cc9|5100×3300|
|05.jpg|05 - KAS Circuit Board & Wiring.jpg|16540f43305c2cc56a96e1e4b4c8c35af724bd4e0b3791fd9adaa33d52f0e548|5100×3300|
|06-kas-ics.jpg|06 - KAS ICs.jpg|89db6cb157f6bb6ec195e66e19739a8231a8bc44828eccbf3548d35607bf0324|2550×3300|
|15.jpg|15 - KBC 1-2 Circuit Diagram.jpg|570762503245163d83850eded3defe9c327db24571ebe7bafae56f05f18e0fdf|5100×3300|
|16.jpg|16 - KBC 1-2 Circuit Board & Wiring.jpg|777935d1cd5bf49fa04be0a5722c1d42e1bcee936b91cd3dddfaeaa6352be524|5100×3300|
|cs80_foldout_schematic.jpg|cs80_foldout_schematic.jpg|23de2fc4ef4bbbc6c171979aa586a2f1c1fb11d7f6116656153a752b31709b38|12185×5972|
|cs80_foldout_block_diagram.jpg|cs80_foldout_block_diagram.jpg|d94f57afc0bba272ed21c91359c65bb8d3ba74667a970692fd83430f125d627d|12176×5972|

## B→Q: independently corroborated polarity-preserving path

Definition: electrical 1=CMOS high +8.5V, electrical0=−6.5V. Printed KAS06 table uses the opposite label convention: table1=−6.5V, table0=+8.5V.

- Bbar1 connector24 → IC1 pin1 → four bits → pin13→pin4 → four bits → pin11 → IC3 pin6 → pin10 → IC4 D1 pin5 → Q1 pin1.
- Bbar2 connector23 → IC1 pin5 → four bits → pin10→pin6 → four bits → pin8 → IC3 pin4 → pin11 → IC4 D2 pin9 → Q2 pin13.
- Bbar3 connector21 → IC2 pin1 → four bits → pin13→pin4 → four bits → pin11 → IC3 pin2 → pin12 → IC5 D1 pin5 → Q1 pin1.
- Direct SCbar→IC3 pin9 gates delayed input; SCbar via IC8 pin11→10→IC3 pin14 gates old-Q feedback at IC3 pins7/5/3.
- Master phi direct→IC1/2 pin3; inverted phi via IC8 pin13→12→4013 clocks.
- No data inversion or bit permutation. At a successfully aligned capture, electrical Q equals delayed electrical B, hence complement of printed negative-logic B triple. This does not prove complete scan timing or physical-key code assignment.

Two independent readers agreed on the pin-by-pin path above. An initial transcription of the select-pin numbers was corrected during review: pin9 is direct SCbar, pin14 is inverted SCbar. The final agreed graph preserves data polarity.

### Conditional table only (KAS06 labels, not established CS80 physical map)

|Printed key-column label|Printed B1B2B3|Electrical delayed Q1Q2Q3|
|---|---|---|
|C2|011|100|
|C2#–C3|101|010|
|C3#–C4|001|110|
|C4#–C5|110|001|
|C5#–C6|010|101|

If those labels are applied literally to the actual CS80 calibration notes, C2 gives Q100, not the required Q110 for VR1 trim-enable from the p15 conditional network. C5# gives Q101, agreeing with VR2 enable. This remains a real unresolved contradiction; no missing sixth code extrapolated. Previously inspected generic servicing-guide OC association may be used only for that 49-key worked example, not asserted as CS80 ROM truth.

## Connector 57 versus59: stronger resolution

KAS04 explicitly prints B3(59). KAS05 wiring prints B3(57), E(59). Two further primary drawings corroborate57:

1. KBC16 reciprocal wiring explicitly RA2-KAS-B3(57) → KBC B3(21), alongside B2(58)→23 and B1(60)→24.
2. Overall foldout schematic explicitly B3(57) at IC5 pin9→10, with B1(60), B2(58).

Best source-supported interpretation: KAS04's59 is a labeling error; wiring destination57 has three agreeing primary locations. Preserve the literal KAS04 discrepancy. No physical continuity measurement has occurred.

## Native crop manifest (x0,y0,x1,y1; source pixels)

|Local PNG|Source|Box|Use|PNG SHA-256|
|---|---|---|---|---|
|kbc-upperleft.png|15.jpg|[0,0,1800,1400]|B wires, shift registers|b3d9248c104ac1ba602603f50bc2d19db492be4e2ec5d662f9e4df04b0529aff|
|kbc-latches.png|15.jpg|[1050,850,2400,1550]|mux and latch polarity|b62e7e77f60a71bf30b2d0bdb546c438e5b19be5726c5f3f243b7f749dee78c5|
|kas-b-buffers.png|04.jpg|[1800,100,3300,1250]|B1/B2/B3 buffers and erroneous59 label|7b2ec3be06ffd1f1be2328946833d2ea5fb9a014ed822125a4bcd9ead3e25d0d|
|kas-b-connector-pcb.png|05.jpg|[400,200,2100,1550]|B3(57), ground59|816762bac15232d8a17fb6650104ae5914a138a9914370cf3466b9636b036915|
|kbc16-b-wiring.png|16.jpg|[3350,1510,4120,1870]|reciprocal KAS57→KBC21|97b7c783e7bd2b4e2621e09209e1eb3b50544f17a2b2f1e480b5d03080f55eae|
|foldout-kas-b.png|cs80_foldout_schematic.jpg|[1950,700,3300,1500]|independent B3(57) schematic|cf7664af27b5f8d03478effce8ca82d556f0252fdf74ee85c9e42bc06068d031|
|foldout-keyboard.png|cs80_foldout_schematic.jpg|[250,1050,2500,1550]|physical C1–C6 keyboard|bc591066211eba618840baf1e5e706f180c2b366f78b0a31b34df4480543b64a|
|kas5-u1-label.png|05.jpg|[3900,850,4450,1030]|U1 K-T1-C1 and C K-M1-C2|7fbe294d47bd8b0dd3957b222c24bd08580bcbabb8e5a593c7d82e15e9d1e264|
|foldout-kbc.png|cs80_foldout_schematic.jpg|[3750,850,5150,1800]|conflicting overall KBC rendition|c40f7e979d9dbb97fc7408f5a8dc23614b284bd64a1c4539df2e52c2c8462b1b|
|foldout-decoder-detail2x.png|cs80_foldout_schematic.jpg|[3860,1010,4480,1600]|same, enlarged2x; not native-size output|93a90d38dc8763598b642b484c568ee7475228c28ca95e7652b9558ef1e51d09|

## New caution: overall foldout KBC disagrees with detailed p15

Visible discrepancies: foldout OC1→OC0 marked100Ω (p15 direct connection); upper trim feed20k (p15 22k); both trim labels appearVR1; inverter trio labeledIC5 (actual part identityIC8); decoder drawn in different ordering with apparently only five visible rails, missing non-inverted Q2 rail. Do NOT import foldout decoder as a fix for the calibration contradiction. It requires separate independent tracing and revision provenance; its visible drafting errors make it weaker for exact decoder truth than detailed p15 plus PCB checks. It does, however, supply a separately drawn B3(57) identifier agreeing with both wiring pages.

## Remaining outcome blocker / next evidence

Source download access is no longer the blocker. The missing fact is actual CS80 YM26600 physical-key output code / matching YM26700 OC decode for 61 keys. Neither p6's five-column 49-key table nor the verified polarity-preserving external path supplies it consistently with C2/VR1 calibration. Useful next evidence: verified board/firmware variant truth table or instrument logic captures at C2/C5#, each adjacent boundary, recording electrical B1/2/3 and aligned Q/OO. Before hardware, independently compare detailed p15 decoder to p16 PCB connectivity and establish provenance of conflicting foldout. Do not select plausible codes, remap physical octaves, alter traced gate wiring or smooth boundaries solely to satisfy calibration.
