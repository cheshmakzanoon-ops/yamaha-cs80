# OE2 delay, clock and output-branch evidence

Primary-source pass: 2026-10-29. This closes the **OE2 input connector discrepancy** and establishes bounded circuit topology/part identity. It does not establish calibrated delay, modulation or noise, and changes no production DSP.

## 1. Input connector discrepancy resolved

- Detailed OE57 schematic labels the audio input **I(53)**.
- Detailed OE58 board/wiring labels **I(53) ← RA1-OE1-O1(46)**.
- Reciprocal OE56 wiring also sends OE1 O1(46) to OE2 I(53).
- OE2 **pin33 is L3**, connected to OE1 LO3(3), in both detailed OE57/58.

Thus use **OE2 I(53)** for the documented board revision. The overall foldout's I(33) is a conflicting label, unsupported by three detailed drawings; it must not override the detailed reciprocal harness. This is source reconciliation, not a physical continuity measurement. The original conflict is preserved in [the routing evidence](OE1_SIGNAL_ROUTING_EVIDENCE.md) with this resolution linked.

## 2. Explicit parts, not inferred replacements

OE58's parts legend (board LC21253) prints:

| Reference | Printed identity |
|---|---|
| IC1 | BA617 |
| IC2 | TC4049 |
| IC3, IC5, IC6 | HA1452D |
| IC4 | MN3001 |
| IC7–IC10 | IG00151 |

The overall foldout additionally labels IC4 **BBD MN3001 / Analog Delay**, and IC1 **VCO / Clock OSC (of Analog Delay)**. OE57/58 do **not** print MN3001 stage count, rated clock range, delay range, charge-transfer loss, noise or distortion. The manufacturer supplement in section7 separately establishes dual512-stage identity and device-level clock convention; no stage count is imported from recollection or a modern substitute.

## 3. Audio branch order

```text
OE1 O1(46) → OE2 I(53)
  → IC3 input active low-pass → AC coupling / MN3001 input-bias network
  → IC4 MN3001
       upper output pair pins14/13 → two 2.2k summing resistors
         → AC coupling → IC5 low-pass → IC5 amplifier
         ├→ IG00151 IC7 → O1(43) → OE1 I2(48)
         └→ IG00151 IC9 → O2(38) → OE1 I3(49)
       lower output pair pins9/8 → two 2.2k summing resistors
         → AC coupling → IC6 low-pass → IC6 amplifier
         ├→ IG00151 IC8  → O3(8) → OE1 I4(67)
         └→ IG00151 IC10 → O4(3) → OE1 I5(68)
```

Important: IC7/9 are **parallel branches**, as are IC8/10; this is not four independent delay lines or two serial VCA chains. A single MN3001 package has the two drawn output-pair paths. Its internal organization/stage count is now established by the manufacturer supplement below; exact tap transfer remains open. OE1 subsequently combines and switches these four returns with its direct input branch; see the separate routing ledger.

The input low-pass and the two post-BBD low-pass sections show two 10 kΩ resistors, 0.0018 µF (1.8 nF) feedback capacitor and 680 pF shunt capacitor, with op-amp follower feedback. The post-filter amplifiers have 10 kΩ input resistors and visibly different feedback values: IC5 39 kΩ, IC6 100 kΩ. Do not assume equal branch gains. These are printed nominal component observations, not measured system response; a future ideal linear solve must retain coupling, bias, loading and compensation rather than identify a whole circuit with a single cutoff number.

OE57 shows revised VCA output capacitors: crossed-out 0.0056 and marked 0.015 µF; OE58 PCB printing agrees on 0.015. Revision triangles elsewhere likewise preclude treating every crossed-out value as active. Each VCA has its own bias/control trims (VR3–VR10); settings are not determined from the schematic.

## 4. Clock generator: bounded trace

The BA617 IC1 output at pin6 drives TC4049 IC2; the foldout's gate-level depiction shows cascaded inversions providing the complementary clock nets to MN3001. OE57's physical-pin drawing provides the wiring, with test points **TP1(64)** and **TP2(57)**. The drawing is a clock-buffer topology, not evidence of a divider ratio or an absolute frequency.

Visible BA617 local components in `oe57-clock.png`:

- VR1 **B5k** into IC1 pin2; **100 pF** at pin3.
- **100 kΩ** series into pin1, **22 kΩ** into that control node, **0.1 µF** at the node.
- Two **4.7 kΩ** divider resistors; **100 kΩ** at pin4 with **33 µF /16 V** decoupling shown.
- Two additional **100 kΩ** modulation-feed resistors join the clock control node (also explicit in the foldout clock crop).

The source establishes these connections and values, not BA617's frequency-versus-control equation. VR1 position and actual control waveforms are unknown. The 100 pF value alone is insufficient to calculate frequency. MN3001 clock-input wiring and the two output-pair recombinations are available in `oe57-clock-bbd-wires.png`; no installed clock period or duty-cycle tolerance is inferred from the board drawings. The device-level timing convention is separately verified in section7.

OE58 gives reciprocal modulation-control connections:

| OE2 | OE1 source |
|---|---|
| L1(68) | LO1(4) |
| L2(61) | LO2(32) |
| L3(33) | LO3(3) |
| L4(26) | LO4(33) |

The four IG00151 control branches and clock modulation must be solved together with OE1's oscillators/switches before claiming original chorus/tremolo behavior.

## 5. Exact next evidence gates

1. **MN3001 device-to-board validation:** the manufacturer supplement below resolves stage identity and clock-period convention. Full tap transfer, operating/bias conditions and characterized loss/noise/bandwidth still need integration with the actual OE2 circuit and measurements. No substitute MN30xx model.
2. **BA617 quantitative behavior:** [BA617_SOURCE_REVIEW.md](BA617_SOURCE_REVIEW.md) now establishes functional pins and the TP1 alignment period of7–12.5µs under specified no-modulation settings. It does not establish an oscillator equation, control-input limits, installed unit trim, full modulation endpoints or a separate TP2 target. The nominal timing inference is explicitly bounded to that alignment setup.
3. **OE1 control-network analysis and calibration:** operating switch states, control waveforms, phase/depth and IG00151 settings. Circuit revisions and component tolerances must be reconciled.
4. **Physical measurements:** clock and control waveforms across settings, impulse/frequency response, insertion level, distortion/noise and switching transients. Source topology does not close the physical-data gate.

This is the bounded stopping point for this source pass: ordered branches, part identity and the manufacturer stage/clock convention are established; calibrated DSP substitution would still require guesses.

## 6. Reproducibility

Directly downloaded OE57/58 originals are 5100×3300. Local artifacts: `artifacts/w7-oe2-20261029/` (JPGs, native unresized PNG crops and `provenance.json`). Foldout source remains `artifacts/w2-keymap-20261029/cs80_foldout_schematic.jpg`. Coordinates below are native `[left, top, right, bottom]`, top-left origin. The tracked tables retain provenance if research artifacts are not published.

| Original | SHA-256 |
|---|---|
| [oe57](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/57%20-%20OE2%20Circuit%20Diagram.jpg) | `9d3060b05899526469cbff8e65e5abbde116230fb4951580d9abd25902db72a9` |
| [oe58](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/58%20-%20OE2%20Circuit%20Board%20%26%20Wiring.jpg) | `c80fe54e35bb421b1b5a80efa17fecfc3bd1e9e2be3fe227478264b4aa2efcc7` |
| [foldout](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/cs80_foldout_schematic.jpg) | `23de2fc4ef4bbbc6c171979aa586a2f1c1fb11d7f6116656153a752b31709b38` |

| PNG | Source | Crop box | PNG SHA-256 |
|---|---|---|---|
| `oe57-clock.png` | oe57 | `(650, 800, 1800, 2650)` | `edaa73ebb1379dceeeec0e70e1a98288822c7ef65f8bb83f62d4051517707fb9` |
| `oe57-input-bbd.png` | oe57 | `(1550, 650, 2650, 2600)` | `13d85f9b8bb20bd406a1489831f34656c033cc8a4e19557cac82baf4780b0e34` |
| `oe57-filters-vcas.png` | oe57 | `(2650, 650, 4750, 2950)` | `7df1a04b4a38eef619222b011dfc4c0f19a26fbffdb4a87e809d8b7ab22044f2` |
| `oe58-harness.png` | oe58 | `(400, 400, 3150, 2600)` | `cd56b620bf7b5f94c7492771b4995404d6cb05e5fe6e00ccb7b9449d61be1dd1` |
| `oe58-parts.png` | oe58 | `(2400, 2550, 3350, 3030)` | `17a5453112bcbffd5958a6c711ac465ace30d097481d415aa711c91616ffbe0b` |
| `oe57-clock-bbd-wires.png` | oe57 | `(900, 1750, 2450, 2550)` | `05ebc9800aeb35f4d1bc93d9c23ebe2d88481fc04ff9ee75bd0e8cfc35863f28` |
| `foldout-clock.png` | foldout | `(7700, 2100, 9000, 2810)` | `ab9bf62c6fc896858fae54569c6336f7c0bf03f01cdda49603b063330df781fc` |

## 7. Manufacturer supplement: stage identity and clock convention

[Panasonic BBD Series handbook](https://www.ka-electronics.com/images/pdf/Panasonic_BBD.pdf), visually verified Panasonic cover and MN3001 section; third-party hosting, manufacturer-authored scan. Physical PDF pages5–7 correspond to printed pages3–5.

MN3001 contains **two independent 512-stage chains**. The timing page defines **T = 1/fCP** and depicts complementary CP1/CP2 waveforms. The device-level 10–800 kHz / 0.32–25.6 ms endpoints are consistent with nominal **t = 256/fCP seconds** per chain. That relation is an inference from the stated stages, timing convention and endpoints—not a measured CS-80 delay law. The internal drawing includes output-end stages beyond the nominal512 labels; exact output-tap phase/recombination still requires analysis. Do not equate the datasheet range or its typical40 kHz condition with the CS-80 operating range.

This resolves the device stage-count gap. The later [BA617 review](BA617_SOURCE_REVIEW.md) adds pin roles and a manufacturer alignment interval; BA617 control law, installed trim settings and actual CS-80 clock measurements remain required.

PDF SHA-256: `a99dc2cbb7aa735ff2f69a1cc06d863ba6ac25798eada44b6558748aee445dec`. Full-page PNG renders use PyMuPDF scale1.5 (108 dpi), no crop, with page bounds recorded below; these are PDF renders, not native scan-pixel crops.

| Render | Physical / printed page | PDF bounds (points) | PNG SHA-256 |
|---|---|---|---|
| `panasonic-page-1.png` | 1 / cover | `[0.0, 0.0, 612.239990234375, 791.760009765625]` | `81d6cd291ad437117f8f216d1c39548025f70c76b2860d97d52663458ff3268f` |
| `panasonic-page-3.png` | 3 / 1 | `[0.0, 0.0, 612.239990234375, 791.760009765625]` | `c5560633be93daba3a07d38382f1e1bd7111cc746ff0abca64f1e205375eb7a1` |
| `panasonic-page-5.png` | 5 / 3 | `[0.0, 0.0, 612.239990234375, 791.760009765625]` | `504658a4d80a59fd57a76838e6623d14998e129ba6d5f23560e30ad40740a67c` |
| `panasonic-page-6.png` | 6 / 4 | `[0.0, 0.0, 612.239990234375, 791.760009765625]` | `669ab5ac04a94e49f59a774b4f5d254a0f60ef2065d30a995aacff2056f32ffd` |
| `panasonic-page-7.png` | 7 / 5 | `[0.0, 0.0, 612.239990234375, 791.760009765625]` | `a61e2552669eeb916dc47159e7162c20ddfed9db491b8ab906eda1b985b6294c` |

Independent review completed: a separate reader checked board identities, parallel branch wiring, literal values, source/crop hashes and native rectangles, then independently verified the Panasonic cover, stage/clock statements, PDF hash and five page-render hashes. The nominal device delay relation is not an installed CS-80 calibration.
