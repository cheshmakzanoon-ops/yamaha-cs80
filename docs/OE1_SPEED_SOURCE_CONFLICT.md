# OE1 SPEED: detailed panel versus overall foldout

2026-11-03. Supplemental source note for [the endpoint diagnostic](OE1_SPEED_ENDPOINT_DESIGN.md).
No production audio, control law, parameter or test threshold is changed.

## Two drawings disagree

- **Detailed Panel 3, printed p66, drawing KEC-4479-68**, marked CS-80
  S/#1001 onward ([original](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/66%20-%20Panel%203%20Circuit%20Diagram.jpg)):
  SPEED VR3 is B10k; terminal 3 references VR1 terminal 1 (+10.6 V),
  wiper 2 references OE1 SP(28), and terminal 1 has no external wire drawn.
  The separately drawn **C** terminal references VR1 terminal 3 and VR4B
  terminal 1; the latter also references EP. These are separate visible
  contacts. Do not silently identify C with resistor terminal 1 or wiper 2.
- **CS-80 Overall Circuit Diagram, identifier 002617**, OE1/PN3 SPEED area
  ([original foldout](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/cs80_foldout_schematic.jpg)):
  VR3 B10k terminal 3 goes to +10.6 V, wiper 2 goes to SP(28), and
  **terminal 1 is explicitly grounded**. No separate C terminal is drawn.

Thus the detailed sheet supports a variable series resistance feeding SP,
whereas the foldout depicts a three-terminal voltage divider. Both descriptions
are source-specific; neither is a verified continuity measurement. The earlier
endpoint note's panel trace must not be read as resolving this conflict.

The cause is not established. Possibilities include an omitted detail, drafting
error, or hardware revision. The displayed serial label and drawing identifiers
are not enough to choose between them or prove which instrument has which wiring.
No direct hardware wiring check or Yamaha amendment resolving VR3 was obtained.
The separate C marking and signal/chassis reference conventions remain explicit
rather than being assigned an undocumented physical meaning.

## Consequence for a model

Do not combine the foldout's grounded terminal 1 with the detailed sheet's open
terminal 1, or fit one assumed topology to the 56 ms endpoint and call the result
a recovered SPEED law. Preserve two circuit hypotheses until continuity,
manufacturer clarification, or suitable measurements resolve the wiring. Input
loading, the selected tab path and IG00150 quantitative behavior remain separate
unknowns even after that choice.

This particular conflict does not negate the **audio crossover** or **LO inversion
pairs**: both detailed OE55/OE57 and the overall foldout show IC5 feeding IC7/IC10,
IC6 feeding IC8/IC9, and LO1→LO3 / LO2→LO4 inversion. Those observations still do
not establish calibrated gain, a complete analog response, or hardware parity.

## Reproduction

| Source | Dimensions | SHA-256 |
|---|---|---|
| Panel66 | 5100×3300 | `d24815efb81591c6a194020490627d68c6dc83de94f8c39b436fb0eba7bbd64c` |
| Foldout 002617 | 12185×5972 | `23de2fc4ef4bbbc6c171979aa586a2f1c1fb11d7f6116656153a752b31709b38` |

Native rectangles, top-left pixel origin: Panel66 knob block
`(2850,1900,4600,3100)`, rotated 90° clockwise for reading; foldout OE1/PN3 block
`(8380,1150,9850,1900)`. Native identifier crops: Panel66
`(330,3090,1000,3300)`; foldout `(7200,250,8000,550)`.
These are unresampled PNG crops, not redrawn circuits. The Vintagesynthparts
service PDF repeats the detailed drawings at lower resolution; that duplicate
scan is not proof of an independent hardware revision.
