# Live envelope targets: state-path evidence and remaining transition law

> **Development policy updated 2026-10-29:** [The current requirement](ANALOG_FIDELITY_REQUIREMENT.md) and [roadmap](PARITY_ROADMAP.md) permit implementation and reviewed integration using public evidence plus explicitly labeled engineering inference; unavailable hardware is not a development prerequisite. In this dated evidence record, measurement-required/open gates limit claims of measured fidelity, not continued modeling. Preserve source conflicts and uncertainty; offline-only statements describe the implementation at the recorded checkpoint, not a permanent integration prohibition.

2026-10-29. **Bounded W5/W10 source review; no production change or calibrated transition model. Independent source/state-path review passed.** This asks a narrower question than [live timing continuity](LIVE_ENVELOPE_TIMING.md): does changing VCA sustain or filter IL/AL directly replace envelope state, or change the drive/selection of a separately stored state?

## New evidence: level selection is not the stored envelope

The Yamaha IC guide's IG00152 block diagram (PDF page33, printed2-3) separates three things:

- AL14, SL15 and IL16 feed level-selection switches G1-1/G2-1/G3-1 whose common output is LC13. AL has an unvalued series resistor before its switch/sequence-control tap.
- AT9, 1DT10 and 2DT11 feed paired time-selection switches G1-2/G2-2/G3-2 to TC12, labeled as VCA resistance control.
- BA IN2 feeds a unity-feedback buffer with BA OUT3. The buffer output also returns to the sequence-control block; the level inputs and trigger inputs G1/G2 also enter that block.

The IG00159 guide entry (PDF page41, printed2-7) shows the corresponding architecture, with SL15 and IL1 and a fixed attack reference derived from the drawn30k/60k supply divider, rather than externally adjustable AL14. The level output remains LC13 and timing output TC12; the buffered evolving envelope is a different signal. This is functional block evidence, not an internal transistor schematic or a supplied comparator truth table.

**Source identity cautions:** page33 is headed/footed IG00152 but its Part No. box says IG00231; preserve the discrepancy documented in the [AL ledger](PRESET_AL_RECEIVER_GRAPH.md). The IG00159 page41 table calls pin16 NC and pin1 IL, while M41 labels both top IC13 positions IL, with both drawn grounded. Do not silently relabel that service drawing or use it to establish a second programmable initial-level input. SL15 routing does not depend on resolving that discrepancy. Guide pages34 and42 are CS-30 SEQ and CS-10 VCA applications respectively, not CS-80 component authority.

## M41 identifies the separately stored state

The native common M circuit shows two matched architectural paths, not simply a target wire connected to an envelope output:

| Role | Filter envelope | Amplitude envelope |
|---|---|---|
| Target selection / buffer | IC12 IG00152 | IC13 IG00159 |
| External active element | IC1 IG00151, diagram label Buffer | IC2 IG00151, diagram label VCA |
| Capacitor/state node | IC1 OUT8 → IC12 BI2, 0.22µF to reference | IC2 OUT8 → IC13 BI2, 0.22µF to reference |
| Buffered output | IC12 OUT3 → TP1(22) | IC13 OUT3 → TP2(68) |
| Buffered-output feedback | TP1 through100k to IC1 −IN4 | TP2 through100k to IC2 −IN4 |
| Selected-level drive | IC12 LC13 through100k to IC1 +IN3 /470k trim node | IC13 LC13 through100k to IC2 +IN3 /470k trim node |
| Timing-control path | IC12 pin12 through82k + VR19 B50k to IC1 EI1 | IC13 pin12 through82k + VR20 B50k to IC2 EI1 |

The selected LC drive and buffered-output feedback therefore enter opposing +IN3/−IN4 paths, not a same-input summing node. This does not supply a numerical closed-loop gain without the active-element law.

The buffered TP nodes also have0.01µF shunts and downstream loads. Positive-input and linear-control trims remain part of the active network; the table is not a complete replacement netlist. M41 labels pin12 `Ic`, while the guide/IC description calls it TC: preserve source terminology rather than interpreting it as a directly specified output current.

M41 separately confirms AL terminal59→IC12 AL14, IL58→IL16, grounded filter SL15, and VCA SL64→IC13 SL15. Those connections alone were already known. The additional result here is the complete **LC drive → external active element/capacitor → buffer → feedback/sequence** relationship.

For the explicitly idealized capacitor, its state obeys `C·dV/dt = net current`. With bounded net current its voltage cannot jump merely because a selected level changes. That is a conditional circuit consequence, not a measured slew limit. It does not imply every observable or comparator state is continuous, or that one software envelope variable represents all electrical states. Target changes can alter the drive and sequence decisions without resetting capacitor charge.

## What follows for each live-control question

| Control change / current phase | Source-supported conclusion | Not established by these sources |
|---|---|---|
| VCA SL while held at sustain | SL is a level-selection/sequence input; the evolving envelope is on the separate capacitor/buffer path. A direct software assignment `value = newSustain` is not derived from that graph. | Whether the sequence stays in its prior selection, which timing control remains effective, upward/downward trajectory, deadband, settling time, overshoot or saturation. In particular, “reuse decay” is not proved. |
| Filter AL during attack | AL supplies the selected attack-level path and reaches sequence control, while capacitor/buffer state is separately retained. | What happens when the new AL crosses the current envelope, exact completion threshold, comparator hysteresis/delay, re-entry or stage transition. |
| Filter IL during release | IL is the initial/release-side level selection; the capacitor/feedback path remains the evolving state. | Exact retargeted release curve, endpoint tolerance, completion rule and response when the new target crosses the current value. |
| Filter IL/AL during held sustain | Filter SL15 is physically grounded. Changing IL/AL does not turn the sustain reference into either changed target. Both other level inputs also reach sequence control. | Whether a particular level change leaves the selector state invariant or changes a comparator/sequence decision; absence of an audible effect cannot be assumed from the grounded SL wire alone. |
| Changes in an unselected phase | The block diagram distinguishes selected LC drive from level-input taps into sequence control. | A blanket rule that every control immediately changes output, or that every unselected level is ignored until the next trigger. |

IC42's fixed-control waveform illustrations establish level polarity, phase ordering and nominal control ranges. They do not depict a control step during an already active phase. Its LC and TC plots are control outputs alongside a changing envelope, not evidence that the envelope itself takes the same discontinuous shape.

## Implementation decision and useful next boundary

An **idealized state architecture** is structurally supportable: explicit capacitor state, separately selected level/time drive, buffer/feedback and a sequence state. A unique numerical live-target transition is **not** derivable from the inspected sources. Missing quantities include the loaded IG00151 current/transconductance law and trims, LC/TC port behavior, buffer loading/dynamics, and sequence comparison/transition rules. The external82k/50k and0.22µF do not supply a standalone target-slew time constant.

Therefore retain the existing bounded timing-only correction and its openly recorded target-latching gap. This review does not validate latching as hardware behavior, authorize arbitrary smoothing, silently reuse a decay coefficient, or impose preserve/reset rules on every state. The next useful work is a separately scoped external-integrator graph with supplied active-element/sequence laws, or primary data/measurements that identify those laws. Any synthetic model must expose its assumptions and compare target steps upward/downward, target crossing current value, held sustain, attack and release separately. No production target-transition change is justified merely by finding a connected input wire.

## Provenance and reproducibility

Inspected original Yamaha sources already present locally; no new downloads. Research images remain untracked. Native crop boxes use original pixels, unresized. PDF images below are **renders**, not claimed native embedded-raster extracts. Local manifest: `artifacts/w5-target-transition-20261029/provenance.json`.

| Original | SHA-256 |
|---|---|
| [M41,5100×3300](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/41%20-%20M%20Circuit%20Diagram.jpg) | `1278f17ff05cd82b2b84153838bf95109d3f1ea1e864119de0a3f4a4af4f0080` |
| [IC42,5100×3300](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/42%20-%20M%20Circuit%20ICs.jpg) | `5698fdfd19600f6b0b8b8f9dfdfc70723248b4b33e8daad39b9f9c0ee11cc24d` |
| [Yamaha IC guide PDF](https://www.goltsynth.com/resources/downloads/CS/yamaha_ic_guidebook.pdf), pages33/41 | `ff051aed0687cad4c4b246d145f85896c229e4cc679333328483118a3e7f8cc1` |

| Crop/render | Box or rendering | SHA-256 |
|---|---|---|
| `m41-eg-state-native.png` | M41 `[1100,1750,3650,2900]` | `cb111f7a15b78f040fadfe48aae13ecc602c33c2c6b726350276db89b023127c` |
| `ic42-eg-native.png` | IC42 `[3000,750,5050,3290]` | `4283558b926bacc53db5dabdad7b58626e70e19e5d04c42ed2c997218a982b33` |
| `guide33-render.png` | PDF33,2482×1754 | `2373664d77718e8cc7b334776acf7e07f862b0829cd99355efce31876933f572` |
| `guide41-render.png` | PDF41,2482×1754 | `4c60603531ae1c81300f55db5783fbdd19487b77416c7b2f44f4cb8e4704987f` |
| `guide33-block-crop.png` | page33 render `[1350,350,2180,970]` | `5063f4e41b6303b09e4724eb818ef9257316b0f96bf8b6077b80b4ce28966267` |
| `guide41-block-crop.png` | page41 render `[1250,380,2050,920]` | `5dc01486af91fca5339aa1efedb9cc78225d80aa719012d7e70b6b69ccac9624` |

PDF render command: `pdftoppm -f PAGE -l PAGE -scale-to 2482 -singlefile -png guide.pdf PREFIX`. Crop hashes can be regenerated with Pillow `Image.open(source).crop(box).save(destination)`. This review adds source/state-path evidence, not a hardware transition measurement or software test result.

Independent review rechecked M41/IC42/guide33/41, original hashes, all four regenerated crop hashes and freshly rerendered PDF33/41 hashes. The selected-level connection was corrected during review to +IN3, distinct from TP feedback at −IN4. Approval covers the bounded state-path evidence, not a unique target-transition law or production change.
