# PRA / OE1 / OE2 signal-routing evidence

Source verification: 2026-10-29. **The coarse EXP-WAH placement gap is closed; analog transfer and dynamics are not.** No production DSP is changed by this note.

## Verified order

```text
rank-I / rank-II outputs → panel MIX VR12A/B
  → PRA MI(16)/MII(17) summing buffer
  → MO(14) dry / ring-modulated RMO(13)
  → panel MODULATION VR7 blend, wiper → PRA WI(23)
  → dedicated PRA wah / bypass switching → WO(65)
  → OE1 I1 buffer, OE1/OE2 send-return and mixing network
  → OE1 O2(55)/O3(60) → PRA I2(64)/I3(66)
  → optical expression networks → VL1(57)/VL2(54)
  → panel stereo VOLUME VR23A/B → PRA LI(39)/RI(49)
  → PRA left / right / summed-general preamps
  → HIGH/LOW switch → LEFT / RIGHT / GENERAL output jacks
```

The arrows describe electrical stage order, not a claim that each stage is a single serial block: the ring blend has two inputs; OE1 has a direct buffered branch as well as four OE2 returns. A stand-in ordered ring → wah → chorus → expression has the correct **coarse placement**, not verified hardware-equivalent transfer.

## Pin-level evidence

| Trace | Current primary evidence / crop |
|---|---|
| M18-O(33), M28-O(33) → MIX VR12A/B; respective wipers → PRA MI(16), MII(17) | Panel2 `panel2-mix`; PRA54 `pra54-harness` confirms VR12A-2 / VR12B-2 destinations |
| MI/MII through two 10 kΩ branches to IC7 summing half (2/2); MO dry and IC8/IC7 ring branch RMO | PRA53 `pra53-ring`; complete labels and outgoing routing also visible in `foldout-routing` |
| PRA MO(14) → VR7 terminal1; RMO(13) → terminal3; terminal2 → WI(23) | Panel2 `panel2-ring`; reciprocal PRA54 wiring. VR7 is **MODULATION**, distinct from VR5 DEPTH |
| WI(23) → IC1/IC2 active RC/photoresistor network and separate dry path; FET1/FET2 branches join WO(65) | PRA53 `pra53-wah`; GW/GW′ switching controls. This is audio processing, not a per-voice cutoff CV |
| PRA WO(65) → OE1 I1(38) | OE56 `oe56-harness`, reciprocal PRA54 |
| OE1 I1 → first half IC1 buffer → O1(46) | OE55 `oe55-audio`; feedback to inverting input without ground-leg gain resistor |
| OE1 O1(46) → OE2 I(53) | OE56 harness; **foldout prints I(33); detailed OE2 review resolves I(53)**, see below |
| OE2 O1(43)/O2(38) → OE1 I2(48)/I3(49) | OE56 harness |
| OE2 O3(8)/O4(3) → OE1 I4(67)/I5(68) | OE56 harness |
| OE1 O2(55)/O3(60) → PRA I2(64)/I3(66) | OE56 and reciprocal PRA54 |
| I2/I3 → VR11/VR12 and optical networks → VL1(57)/VL2(54) | PRA53 `pra53-expression`; EI(52) drives the optical control path, not the audio input |
| VL1/VL2 → VR23A/B terminal3; wipers → LI(39)/RI(49) | Panel2 `panel2-volume`, reciprocal PRA54 |
| LI/RI → left/right preamps, both also feed general summing preamp; output switch and jacks | `foldout-output`; PRA54 confirms L(38)/L′(37)→PN5-J3, G(42)/G′(43)→PN5-J2, R(45)/R′(46)→PN5-J1 |

OE55 shows I2/I3 and I4/I5 return pairs entering separate summing amplifiers through 100 kΩ input branches and AC coupling. FET/diode switching and the direct buffered-I1 branch participate in the two output channels. The returns are not established as interchangeable; OE1 is not itself a standalone delay line.

## Explicit limits and discrepancies

- Detailed OE56 wiring prints OE2 input **I(53)**; the overall foldout prints **I(33)**. Both connect OE1 O1 to the OE2 audio input. Subsequent [detailed OE2 review](OE2_CIRCUIT_EVIDENCE.md) resolves the documented connector as I(53): OE57 and reciprocal OE58 agree with OE56; pin33 is L3. Preserve the conflicting foldout label rather than silently rewriting it. The foldout has other known drafting discrepancies ([keyboard evidence](KEYBOARD_CODE_PATH_EVIDENCE.md)); it is corroboration of architecture, not a substitute for detailed-board reconciliation.
- The foldout visibly labels an OE2 analog-delay block, BBD MN3001, clock oscillator and filters/VCAs. That is **printed topology/part-label evidence only**, not a verified OE2 internal model. This routing pass did not derive OE2 dynamics. Subsequent [OE2 evidence](OE2_CIRCUIT_EVIDENCE.md) traces pp57–58 and manufacturer stage identity; installed clock law/range, modulation, level, distortion/noise and complete transfer remain open.
- PRA optical resistance versus pedal travel/EI, LED current versus resistance, hysteresis, attack/release and temperature dependence remain unmeasured. The pedal sensor/lamp and PRA optocouplers are distinct elements. No optical resistance curve is guessed here.
- Wah sweep, Q, gain, dry/processed insertion level and complete loaded RC transfer remain unsolved/unmeasured; the production swept-bandpass is still a provisional stand-in.
- Expression loading/taper, ring transfer and blend law, OE1 switch truth table/transients, output calibration and loaded HIGH/LOW levels are not established by routing. Printed component values are not a calibrated response.

## Reproducibility and provenance

Historical commit `ed8b49efb49771dfc56a77cfe0e8e9b5bc2e8525` was fetched read-only and its earlier `docs/OE1_SIGNAL_ROUTING_EVIDENCE.md` treated as a lead, not authority. This pass downloaded PRA53/54, OE55/56 and Panel2 directly and inspected the native crops below. OE55/56 original hashes match the historical note. No branch reset or push was performed for this retrieval.

Source JPGs and derived PNG crops are local, untracked research artifacts in `artifacts/w7-routing-20261029/`; the existing foldout source is in `artifacts/w2-keymap-20261029/`. The JSON `provenance.json` in the new artifact directory records all source URLs, dimensions, hashes and crop boxes. The following tables preserve the same provenance in this tracked document even when artifacts are not published. Coordinates are native pixels `[left, top, right, bottom]`, origin top-left; listed crops are unresized PNGs. Overview thumbnails are navigation aids only.

### Original sources

| Source | Dimensions | SHA-256 |
|---|---|---|
| [pra53](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/53%20-%20PRA%20Circuit%20Diagram.jpg) | 5100×3300 | `a691e710b59deecace01a62b57587e6b939e1084bfd71003efe71929bbe42c61` |
| [pra54](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/54%20-%20PRA%20Circuit%20Board%20%26%20Wiring.jpg) | 5100×3300 | `88703d2b43189003e3276ca7837556ef2ac5205dd057d8f5b0c6b320e1042c69` |
| [oe55](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/55%20-%20OE1%20Circuit%20Diagram.jpg) | 5100×3300 | `524aaba53696004178b00f653ee2b69c736d957c589bb7641f67274859c76ef4` |
| [oe56](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/56%20-%20OE1%20Circuit%20Board%20%26%20Wiring.jpg) | 5100×3300 | `79f58431e27434dacc6c55b664bc41cf4f5102bf00b84ed475ae17cc39308bfe` |
| [panel2](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/64%20-%20Panel%202%20Circuit%20Diagram.jpg) | 5100×3300 | `057550f85d4c3622c95da14bbcaa6d9bec8aced9addfdf00eb3498553053de4a` |
| [foldout](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/cs80_foldout_schematic.jpg) | 12185×5972 | `23de2fc4ef4bbbc6c171979aa586a2f1c1fb11d7f6116656153a752b31709b38` |

### Native inspection crops

| PNG | Source | Crop box | PNG SHA-256 |
|---|---|---|---|
| `pra53-wah.png` | pra53 | `[1700, 850, 3850, 1600]` | `bce607e38f33ac0eba77f361e22b3d3e5edd2b56162857cd10469ff524771767` |
| `pra53-expression.png` | pra53 | `[3800, 800, 4550, 1950]` | `f7465866a88501aa7b9bd386c0589b5728575c870b113095e7cdf5d8bcf2442b` |
| `pra53-ring.png` | pra53 | `[1700, 1550, 4050, 2550]` | `e845862cc8e398ad0a040fb283790b54092a48876ec96f81411438a9a2a7fecc` |
| `pra54-harness.png` | pra54 | `[350, 350, 2900, 2500]` | `b5892cadf3ee503178d2da0792b34b8a68db2378cb7e39ecc4128482b3b6a3f7` |
| `panel2-ring.png` | panel2 | `[1200, 250, 1750, 1700]` | `047bdb74548a3df3a742357cd3b28b4f0c840f90b1a76fb143f8dffea04e4b87` |
| `panel2-mix.png` | panel2 | `[2000, 1650, 2550, 2900]` | `74892a4eb2af6c4f15bbcdff339ae3f156b080a39c8697f58325a6387c79288a` |
| `panel2-volume.png` | panel2 | `[4050, 1650, 4650, 2950]` | `a27b7f3bc1d30dd4da5273e0c7e39bde2cb794e744e0e6e171d5bad74466639e` |
| `oe56-harness.png` | oe56 | `[350, 400, 1150, 2500]` | `8eec71da43b2abb908d380de61f15ab58a163975eb2f7d0b3e15cec94bf3d343` |
| `oe55-audio.png` | oe55 | `[800, 500, 3050, 2600]` | `abedb9d2d2536479289154cb515256d39bd0256dc4e068b5647b4b86ccfc4336` |
| `foldout-routing.png` | foldout | `[7450, 620, 11600, 2900]` | `40967f827d6b87cdb6b7aaa39ee42a8e3b82a11403ebc81cfa085365c755bd08` |
| `foldout-output.png` | foldout | `[10000, 1800, 11650, 2950]` | `f990aa037912b7aa2bceeae6e85e87bce9300a3170e85c967eb6d1a88ae71a51` |

Independent review completed: a second reader checked the coarse route, source/crop hashes and native rectangles, and the OE2 connector-label discrepancy. The summing half label was corrected to IC7 2/2. This is source review, not measured transfer validation.
