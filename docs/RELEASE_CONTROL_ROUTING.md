# Release-control routing: SUB selector and first M receivers

2026-10-29. W5/W9 **partial Class A source evidence**, not a calibrated envelope model. No hardware measurement, production DSP change or complete eight-voice fanout solve is claimed. A second reader has checked all eight native crops, the four source hashes and regenerated crop hashes, including diode directions, revised resistors and the Tr6 correction.

## Result and scope

Factory release outputs are **inputs to SUB**, not direct M timing-pin voltages. Native SUB29/30 closes the intervening rank-I route:

- T51 output15 physical17 → SUB RF55 → 100k / diode-clamp / 1k / Tr2 → RFO60 → M11 RF60 → IC12 IG00152 2DT11.
- T51 output21 physical10 → SUB RA56 → corresponding Tr3 branch → RAO61 → M11 RA65 → IC13 IG00159 2DT11.

The last M-pin hops and T51 first hops are reused from the independently reviewed [receiver ledger](PRESET_RECEIVER_PORTS.md), not newly independently reviewed here. SUB's first-receiver wiring is newly read from native pixels. **M11 is a demonstrated first receiver, not the total load.** M12–18 release-chain links remain to be traced individually.

There are four release branches, sharing a global diode-cathode bus but retaining separate input resistors, base resistors, transistors and outputs. This is a switched clamp/selector topology, not an additive global-release mixer or an isolated passive divider. A numerical minimum is an explicitly idealized abstraction, not a solved physical voltage.

## Native SUB branch inventory

SUB29 shows the following repeated structure. For each input Vj, define Xj as its junction after100k, Bj as the transistor base after1k, Yj as its emitter/output, and S as the shared diode-cathode rail.

| Input | Diode from Xj (anode) to S (cathode) | Emitter follower | Output | SUB30 printed destination |
|---|---|---|---|---|
| RF55 | D9 | Tr2 | RFO60 | RA1-M11-RF(60) |
| RA56 | D10 | Tr3 | RAO61 | RA1-M11-RA(65) |
| RF′57 | D11 | Tr4 | RFO′62 | **RA1-M21-RP(60)** as printed/read; do not silently change RP to RF |
| RA′58 | D12 | Tr5 | RAO′63 | RA1-M21-RA(65) |

For all four branches, Vj→100k→Xj→1k→Bj. Collectors connect to+15V; emitter/output has100k to−15V. The100k emitter resistor annotations replace crossed-out values; retain the revision rather than reintroducing the deleted value. SUB30's board drawing also labels100k at these branches. No explicit local capacitor is drawn on these release nodes; that does not establish zero device, harness, or load capacitance.

SUB30 identifies RF55/RA56 inputs as T51-15(17)/T51-21(10), and RF′57/RA′58 as T53-15(17)/T53-21(10). These are source labels, not a completed rank-II matrix/harness audit. The RP/RF distinction at M21 requires reciprocal verification before asserting rank-II endpoint identity.

SUB30 parts key: Tr1–5 are2SC828; Tr6 is2SA561; FET1–3 are2SK30; diodes1–22 are1S1555. Device identities alone do not supply forward drops, beta, leakage, temperature behavior, FET on-resistance or threshold bounds.

## Global sustain and footswitch selection

### Analog selection path

SUB TV1(22) enters one FET1 channel terminal; the other terminal is S, the shared D9–12 cathode bus. The FET gate network includes a10M connection from S to the gate/D21 junction. D21's anode is at that gate junction and its cathode at Tr1's collector node; preserve this polarity rather than substituting a generic logic switch. Tr1 collector has10k to+15V, emitter−15V, and base is fed through100k from TS1(21). This is an active switching network, not a second global timing input to each EG IC.

Panel66 establishes VR1 **C10k**, labelled SUSTAIN: endpoint1 is BA +10.6(20); endpoint3 is tied to PN3-VR3-3; wiper2 connects SUB TV1(22), also confirmed by SUB30. A wire additionally names PN3-VR3-C at the wiper. This pass does **not** newly resolve the whole VR3 reference network or BA supply impedance. C10k does not establish a linear normalized-control-to-voltage law. Do not replace the reference with an ideal ground or+10V merely because those simplify a model.

### Discrete assigner path

Panel66 SW1 SUSTAIN: T1→SUB TS1(21). SW2 PORTAMENTO/GLISSANDO: T1→SUB TS2(26). Their B1 contacts share KBC1−15(69); their M1 contacts share SUB TS4(24). SUB30 reciprocally identifies TS1→PN3-SW1-T1, TS2→PN3-SW2-T1, TS4→PN3-SW2-M1, TS3(25)→PN5-J5-2.

SUB29: TS3 has100k to+15V and a separate100k to Tr6 base; Tr6’s arrowed upper terminal is the emitter at+15V; its lower collector connects TS4, with10k from TS4 to−15V. Tr6’s parts-key identity is PNP2SA561, consistent with that symbol. **This corrects the collector/emitter wording in the older foot-control evidence note**, which called the upper terminal collector and TS4 emitter. The discrete owner-manual truth table is unaffected; the electrical transistor graph must use the native symbol.

Panel67 J5 shows a normalled jack contact, TS3 connection, and revised/deleted small protection-component annotations. The PFS drawing shows the external contact across the plug conductors. This pass does not resolve the revised small-component values or derive a numeric switching threshold.

The already owner-manual-supported semantic truth table in [FOOT_CONTROL_EVIDENCE.md](FOOT_CONTROL_EVIDENCE.md) remains:

`effectEnabled = !assignerEnabled || !plugPresent || physicalContactPressed`.

Apply independently to sustain and P/G. Native wiring corroborates the two selectors sharing one pedal signal; it does not measure cancellation transients, bounce or propagation delay. An absent plug and an inserted open plug are not interchangeable.

### SUSTAIN I/II is a separate route

Panel66 SW3 SUSTAIN I–II T1 goes to RA2-SH-SUS, with a revised terminal annotation (appears17 beside a crossed-out older number); M1 goes to PN3-VR2-3, and the alternate drawn contact is open. This is **not** the SUB TS1/TV1 selector. The SH destination, precise revised terminal number and downstream mode behavior require a separate reciprocal SH trace. No mode-II tail-kill transient or gate reset law is established here.

## Permitted circuit reasoning, with explicit assumptions

With conventional current directions, the settled branch equations are:

```
(Vj - Xj)/100000 = I_Dj(Xj - S) + (Xj - Bj)/1000
(Xj - Bj)/1000 = I_Bj(Bj, Yj, +15, temperature)
I_Ej = (Yj + 15)/100000 + I_downstream,j
```

The four diode currents also load the shared S/FET/VR1 network. The preset-source load is `(Vj-Xj)/100000`, **not** simply Vj/100k to ground, and not an independently fixed resistance. A complete solve must include source matrix loading, FET/gate network, pot references, all M receiver currents and rails. The equations above are an inventory of missing laws, not a completed nonlinear model.

Under an explicit ideal-switch, ideal-diode, zero-base-current abstraction with stiff Vj and sustain source Vs, and with gate-bias/leakage paths neglected, enabled sustain would clamp each Xj to `min(Vj, Vs)`; disabled sustain leaves the individual control dominant in that limit. The actual 10M gate-bias path and switching devices are not an ideal open circuit; their residual loading must remain in a physical solve. This explains the lower-control-voltage selection reported by the servicing-guide discussion reused in [CIRCUIT_MODEL_SPEC.md](CIRCUIT_MODEL_SPEC.md). It is **not** evidence that physical Yj equals that minimum: base/emitter drops, finite100k/1k loading, diode/FET behavior and common-source loading remain. No arbitrary0.6/0.7V drop is introduced.

Given the separately documented qualitative monotonic fact that higher timing CV shortens release, an ideal lower-CV selector corresponds to the longer of the two associated durations. That does not prove a common calibrated voltage-to-time curve across IG00152/159, nor any exponential/logarithmic curve, slider taper, exact10-second endpoint, bend factor, instantaneous tail shortening or diode-drop cancellation. The existing production `max` time policy remains a behavioral approximation; this source pass does not validate it electrically.

## Provenance and reproducibility

All originals were downloaded directly with `curl -L --fail` on2026-10-29 from the original Yamaha service-scan index. Each is5100×3300. Local evidence directory: `artifacts/w5-release-20261029/`; scans/crops are inputs, not proposed repository payloads. Initial wrong Panel3 basename returned404; the index supplied the correct filename, subsequently retrieved successfully. No access or permission denial blocked this pass.

| Local original | Primary URL | SHA-256 original bytes |
|---|---|---|
|29.jpg|[SUB circuit](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/29%20-%20SUB%20Circuit%20Diagram.jpg)|`b81c7e5ad2927ac04d5912bfc455a631237f512a0c7700bda41250a14bd33e60`|
|30.jpg|[SUB board/wiring](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/30%20-%20SUB%20Circuit%20Board%20%26%20Wiring.jpg)|`c909c703f261110f3893410af9d5b639e9b693941823342f14924baca948fc65`|
|66.jpg|[Panel3 circuit](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/66%20-%20Panel%203%20Circuit%20Diagram.jpg)|`d24815efb81591c6a194020490627d68c6dc83de94f8c39b436fb0eba7bbd64c`|
|67.jpg|[Panel5 circuit](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/67%20-%20Panel%205%20(EJ,%20EXP,%20PFS)%20Circuit%20Diagram.jpg)|`0a81a6812213e84993f3ab9d3cdf5cd070914832bec1b94f5374a4916375d4b2`|

Crops are native `PIL.Image.open(source).crop((left,top,right,bottom)).save(path)`, no resize/conversion, right/bottom exclusive. Overview reductions were navigational only.

| PNG stem | Source / box | PNG SHA-256 |
|---|---|---|
|sub29-release|29 / `[1910,1520,3610,2460]`|`5d6959a35f7a6e3dc7d693019fd396326f09584153faae61a6700882c9ca2e37`|
|sub30-release-wiring|30 / `[590,1510,1450,2130]`|`614347dde061b95e2e6dd7b33d2d396f762b358a45281975012a560ccce9e089`|
|sub30-control-wiring|30 / `[2620,1550,3410,2040]`|`096e40501a90b681b1837b8785e93e64f0378bf414c9fde203649b69b898df9e`|
|sub30-parts|30 / `[2490,2680,3710,3090]`|`d88dbd4b8e32e0e92a8ba313148f374f341d2b8671c2125537c9c2819673e31f`|
|panel66-assign-mode|66 / `[1160,400,2380,1450]`|`7444b1d5e4c20fb53248f0491dafac8a0fa602cfdbde3d52e86131c8946f4173`|
|panel66-sustain-pot|66 / `[3120,2000,3660,3080]`|`22d6c99526e12232440e1491b640a3b101de03368e926cf661e7c5960290791a`|
|panel67-j5|67 / `[4060,1020,4820,1260]`|`e5348c908d6ddf76c0ac721f97e49c596d4026f915bfde7ee605c55d798e0cd8`|
|panel67-pfs|67 / `[2820,2520,3450,2870]`|`71b0e0d6cc62d76d72b2f5da1da46cde29c6dc3aad2559d581a3f0c965859d6a`|

## Remaining gates

1. Completed: second native read of diode/FET polarities, revised values and preserved RP/SH annotations. Still required: reciprocal M11–18 release fanout and M21/rank-II wiring.
2. Resolve VR3/BA references and full source impedances; trace panel/memory isolation under every selection state.
3. Trace SH mode-I/II behavior separately; resolve revised J5 protection values if building an analog switch model.
4. Obtain actual diode/BJT/JFET laws and EG2DT input currents before a loaded nonlinear solve; obtain timing curves/measurements before production envelope calibration.

Only this new document was authored. Existing files, build configuration and production code were not edited; no commit or push was made. The coverage boundary is the SUB selector, its panel/jack connections and first rank-I M receivers, not a complete release system.
