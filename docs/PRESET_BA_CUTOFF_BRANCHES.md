# BA cutoff branches: memory followers and selected-source drivers

2026-10-29. Bounded W9 source pass following [the26-port ledger](PRESET_RECEIVER_PORTS.md). **Native-source transcription independently reviewed: diode polarity, feedback, routing, driver topology, component labels, three source hashes and ten regenerated crop hashes corroborated.** No production parameter, circuit solver or existing document is changed by this pass. No hardware was measured.

## Main finding

BA-f05/f06 are **diode-isolated memory follower outputs injecting into the rank-I preset cutoff buses**, not demonstrated passive shunts to ground. A second memory follower joins each bus through f09/f010. The op-amp feedback connection is **before the output diode**, so this is not a diode-drop-compensating precision rectifier. Do not replace these branches with fixed grounded loads, ideal sources, an assumed0.6V drop, or a permanently disconnected inactive memory.

## Native path reconstruction

Names retain the printed `fI` input / `fo` output distinction; the scanned letter `o` looks like zero. Pin numbers below refer to the dual op-amp, connector numbers are parenthesized.

| Rank-I source | BA input / follower | Isolation and bus destination | Evidence |
|---|---|---|---|
| Memory M1 HP cutoff VR5 wiper2 | fI5(23) → IC7 first half +input3; output1 fed directly to −input2 | output1 → diode anode; cathode → fo5(22′) → T51 output7(27) | BA27 `f05-f06`; BA28 `cutoff-right`; reciprocal T51 p32 |
| Memory M1 LP cutoff VR7 wiper2 | fI6(25 in wiring) → IC7 second half +input5; output7 fed to −input6 | output7 → diode anode; cathode → fo6(24′) → T51 output9(24) | Same; terminal discrepancy below |
| Memory M3 HP cutoff VR5 wiper2 | fI9(53) → IC9 first half +input3; output1 fed to −input2 | diode cathode → fo9(53′) → fo5(22′), same HP bus | BA27 `f09-f010`; BA28 `cutoff-left/right` |
| Memory M3 LP cutoff VR7 wiper2 | fI10(56) → IC9 second half +input5; output7 fed to −input6 | diode cathode → fo10(55′) → fo6(24′), same LP bus | Same |

BA28 notes identify IC3–10 as **NJM4558**, and diodes as **1S1555**. BA27 shows the follower bank powered from+15V on pin8 and−15V on pin4. There is no external series resistor or capacitor in the four follower-output-to-bus branches. This absence does not imply zero output resistance, zero parasitics, zero input bias, or a known diode I–V curve.

The two NJM4558 halves share a package and supply rails; the two memory controls share their selected memory rail and reference endpoint. Consequently four independent ideal Thevenin sources are not an established physical model.

### PANEL follows a different drawn route

BA28 additionally identifies PN1 VR5-2 → fI1(61), PN1 VR7-2 → fI2(63). BA27 IC5 followers have **no output diode drawn on BA**: fo1(61′) goes to T51 7′(26′), and fo2(63′) to T51 9′(23′). These are the primed alternate inputs, distinct from the unprimed factory bus; the matrix isolation diode belongs to the following T51 network. Thus the memory and PANEL paths must not be merged merely because both use followers. This paragraph establishes BA wiring; it does not re-audit all T51 primed-terminal revisions.

## Selection and driver coupling

BA27's source-driver bank is separate from the NJM4558 cutoff followers:

- Each selector line enters the base of its transistor through **10k**; each base has **100k to the drawn ground/reference**.
- Collectors share the printed **+10.6V** rail; each emitter is the corresponding selected output. BA28 lists Tr1–29 as **2SC458**. The drawn source-driver topology is an NPN emitter follower, not a wire from+10.6V to the selected column.
- Rank-I inputs1…14 connect to PN2-TS-1…14. Outputs I1…I6 go to T51 A…F; I7…I11 to T52 A…E; **I12(16) → M1 VR24-3**, **I13(18) → M3 VR24-3**, **I14(19) → PANEL VR24-3**. BA28 gives all these wiring destinations directly.
- MBK63 M1 shows its VR24 terminal3 on the common selected endpoint rail, the same drawn rail extending through the pot bank; VR5/VR7 wipers go to BA fI5/fI6 rather than through local output diodes. This closes the M1 memory-input dependence on the selected rail. The opposite endpoint continues into panel/memory reference wiring; this pass does not assume an ideal ground there.

These observations establish **where selection acts**: on the factory-column or memory/PANEL supply endpoint, not by a drawn switch in each fo5/fo6/fo9/fo10 output wire. They do **not** establish selector contact truth tables, input-line voltages in every mode, overlap during switching, inactive-rail discharge, or which diode conducts at a particular instant. Full PN2 tone-selector and reference/rail traces remain required before assigning numerical active/off states. In particular, the100k base pulldown is not a100k memory-output shunt, and a disconnected/unselected emitter is not proved to be clamped to0V.

For a factory column, all selected matrix branches draw from the same emitter; base current and shared collector-supply behavior can couple output rows. For a memory selection, the selected endpoint rail drives the entire pot bank and input loads. The+10.6V label alone is neither emitter voltage nor a source-impedance specification. A fixed VBE, transistor beta, output resistance or inactive-state law has not been supplied.

## Source discrepancies retained

- BA27 labels fI6's connector as **(23)** in the scanned circuit drawing, duplicating fI5. BA28 explicitly labels **fI6(25)** and MBK63's M1 VR7 destination likewise reads fI6(25). Use the symbolic fI6/IC7 second-half path in analysis and preserve this connector-number conflict rather than silently correcting the schematic.
- BA28's right-hand fo5 destination reads `RA1-BA-fo9(53)` without a visible prime, whereas its left-hand fo9 terminal is **(53′)** and BA27 also gives fo9(53′). Both retain the same symbolic fo9 link. Preserve the inconsistent parenthetical label; do not confuse fI9(53) with the diode output.
- The legible first-half feedback path identifies output1 and −input2; no inference from the blurry second connector label is needed to establish follower topology.

These are source annotation conflicts, not permission to change any of the25 matrix-cell exclusions.

## Explicit model boundary, no new implementation

For each memory branch let W be the loaded memory wiper voltage, O the op-amp output **before the diode**, and B the joined cutoff bus. A legitimate future port model must retain:

`Ibranch = I_diode(O − B, temperature, history)`

with the follower's negative input at O and positive input at W. The relation between W, O and output current must come from an explicit op-amp model/boundary; `O=W` is only a declared ideal-feedback experiment. Diode voltage is outside that feedback loop. W itself needs the loaded pot and selected-emitter/reference equations.

For HP, the fo5 and fo9 branch currents enter the same T51/T52 output7 bus; LP similarly receives fo6 and fo10 at output9. The existing conditional loaded-control references cover downstream passive resistor/capacitor networks but do not supply these upstream current laws. Neither branch is represented adequately by blindly adding a ground conductance.

**No new offline numeric helper is proposed yet:** the topology supports a symbolic graph inventory with explicit unknown ports, but provides no defensible numeric BA port law. Highest-value next document gate is the PN2 selector truth table plus memory/PANEL reference-endpoint trace, followed by appropriate NJM4558/1S1555/2SC458 device evidence and explicit operating-state assumptions. Component identity alone is not instrument-calibrated behavior.

## Primary provenance

Originals downloaded directly2026-10-29. BA28 filename was verified from the [original service index](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/index.html), not guessed. All originals5100×3300; native PNG crops use Pillow `Image.open(source).crop(box).save(path)`, no rescaling/conversion. Local source/crops/manifests are under `artifacts/w9-ba-cutoff-20261029/`; scans are evidence inputs, not a proposed repository payload.

| Source | Primary URL | Original SHA-256 |
|---|---|---|
| BA27 | [BA circuit](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/27%20-%20BA%20Circuit%20Diagram.jpg) | `3a6cc81d8ac8895f97e85c3e5ccb0a2b978879f53a5804753d5993d77684204e` |
| BA28 | [BA board/wiring](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/28%20-%20BA%20Circuit%20Board%20%26%20Wiring.jpg) | `88614905fecf5b41a290a108586ee2519ea08990d125169bc51fbc3be61c1884` |
| MBK63 | [Memory circuit](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/63%20-%20Panel%201%20%28MKB%29%20Circuit%20Diagram.jpg) | `6050a23f85823be719aab033fd90ed3d4f1038409e38c089a64c96b658a08fcb` |

| Native crop | Box `[left,top,right,bottom]` | PNG SHA-256 |
|---|---|---|
| ba27-f05-f06 | `[1300, 2320, 2350, 2780]` | `dee9bf0c5c0f02345815e2ffb02cf17e22a625c5b6727e08dbe7c3836d088ec2` |
| ba27-f09-f010 | `[2590, 2310, 3180, 2780]` | `9e114c2112433fa453b3a7b24e538e56ba753bac2d28436fb64a5eb080fcd0bb` |
| ba27-follower-supplies | `[2600, 1950, 3660, 2780]` | `2bc64c4e46558d46d8cb6bc914a2400271c1006dc7e1113735fed64b54eaa93c` |
| ba27-column-bank | `[1150, 780, 2630, 1870]` | `b0853f1ae6024c2f3ecef5f4408140b295aa98b71d6191a0331ce90b8ccc2406` |
| ba28-cutoff-right | `[3430, 1720, 4980, 2000]` | `be4b51338d60eb376d78ad3406e788456324ab0e9a22736ed51dba51f6c24855` |
| ba28-cutoff-left | `[500, 1340, 2150, 2220]` | `742e1996547da02c73c76e2a5a4fd8f34f6692adc68211d3408dcfe34139d8e8` |
| ba28-notes | `[3320, 2620, 4400, 3220]` | `44de143c54ff8ee2d46bc2a35aa23c112b9b65d613eb36bca3654fba115a72e9` |
| ba28-select-rank1 | `[3320, 670, 4800, 1730]` | `02a7fc037722cdf950596df0a7bc4354f37236d38c20e99169e02ab5107a82a5` |
| mbk63-m1-pots | `[950, 2530, 2200, 3020]` | `90041b20c2c7c51162c1fce769396c4115317a12f55413bf06971785e78355f7` |
| mbk63-m1-rail | `[4100, 2530, 4800, 3020]` | `8b167fd9e5f9123f82cf87dba0ffa5902cd97500290a73ccde24d55aaed032f4` |
