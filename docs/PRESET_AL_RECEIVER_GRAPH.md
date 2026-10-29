# Rank-I preset AL receiver graph and reference-model gate

> **Policy precedence (2026-10-29):** Under [the current requirement](ANALOG_FIDELITY_REQUIREMENT.md) and [roadmap](PARITY_ROADMAP.md), missing physical measurements do not block development or reviewed integration. Use public evidence plus explicitly labeled engineering inference, with assumptions, uncertainty and tests. Earlier measurement/promotion gates below remain limits on claims of physical verification, not a prohibition on inferred models; recorded offline status and source conflicts remain unchanged.

2026-10-29. Source analysis only. No production conversion or new preset values. This is a bounded trace of output12 (VCF attack level), not a completed electrical clone.

## Observed connectivity

| Edge | Direct source evidence |
|---|---|
| T51/12(21) ↔ T52/12(21) | PCB/wiring pages32 and34, native r0c1 |
| T52/12(21) ↔ M11/AL(59) | page34 native r0c1; reciprocal page44 |
| M11/AL(59) ↔ M12/AL(59) ↔ M13/AL(59) | page44 r1c0; reciprocal M11/M12 entries |
| PN1 VR10 wiper2 → T51/12′(20′) → isolation diode → output12 bus | Panel1 page62 and T51 schematic/PCB wiring |
| Memory VR10 diode-output network → M18/AL(59) | MBK page63 native r0c1; memory-bank interconnections in r1c1 |
| M-card terminal59 AL → IC12 AL pin14 | M41 native `m-al-receiver`; direct drawn wire, no external divider on this connection |

The separate memory and panel paths must not be collapsed into an ideal single control source. The memory drawing visibly includes isolation diodes. Do not count pot resistance as a permanent ground load through a reverse-biased diode.

Native pages45–47 close the remaining rank-I AL daisy chain. The following terminal59 entries each show two pink wires (PK×2); labels below retain the source's MBK spelling at the memory endpoint.

| Board | Printed AL(59) destinations | Native crop |
|---|---|---|
| M13 | RA1-M12-AL(59), RA1-M14-AL(59) | mwiring45-native-r0-c0 |
| M14 | RA1-M13-AL(59), RA1-M15-AL(59) | mwiring45-native-r0-c1 |
| M15 | RA1-M14-AL(59), RA1-M16-AL(59) | mwiring46-native-r0-c0 |
| M16 | RA1-M15-AL(59), RA1-M17-AL(59) | mwiring46-native-r0-c1 |
| M17 | RA1-M16-AL(59), RA1-M18-AL(59) | mwiring47-native-r0-c0 |
| M18 | RA1-M17-AL(59), PN1-MBK-M3-VR10 | mwiring47-native-r0-c1 |

Together with page44 this establishes eight connected rank-I receiver ports, M11 through M18, from wiring evidence rather than voice-count assumption. It does not establish equal port currents, negligible harness impedance, or rank-II connectivity.

On T51 this analog row has its documented local10k shunt to E. T52 has no additional local shunt. The wire joining their unprimed output12 buses is explicit; therefore this particular assembled graph contains the T51 shunt once, including when a T52 tone sources the bus. This does not change board-local metadata or resolve crossed T52 primed-terminal applicability generally.

## Receiver identity and quantitative boundary

M42 identifies the VCF-EG as IG00152. Its pin14 AL description specifies an input span0–10V and attack-level span0..+5V. Pin15 SL is normally fixed at0V; pin16 IL controls the negative initial level separately. These are pin-function/range evidence, not a supplied input-resistance, input-bias-current specification, linear interpolation equation, or a guarantee that every voice produces an identical endpoint.

Native M41 confirms terminal59 directly entering AL pin14 and SL pin15 grounded. It is inappropriate to invent an external AL divider or assume infinite receiver impedance.

The original Yamaha IC guidebook PDF page33, headed iG00152 and footered iG00152 2-3, draws an **unvalued internal series resistor** from AL14. Its following node taps the sequence-control circuit and a switched path toward LC13. The separate BA IN2 / BA OUT3 buffer is not AL14: generic buffer wording cannot establish an ideal AL input. No resistor value, input-current law, impedance specification or transistor-level input circuit is supplied here. Preserve a source identity discrepancy: the same page's Part No. box reads iG00231. The title, footer and pin graph support comparison, but do not authorize silently correcting that box or deriving a quantitative law from it. PDF page34 is explicitly a CS-30 SEQ application, not a CS-80 circuit; its external parts must not be transplanted.

The next missing quantity remains each IG00152 AL port current versus voltage, gate/state, supply and temperature. The functional resistor/switch drawing alone does not identify that law.

The envelope's subsequent LC/time-control/integrator/buffer and unequal HP100k/LP47k feed paths are downstream stages; bus12 is neither final envelope output nor filter cutoff voltage. No0.5×bus-to-octaves conversion follows from the endpoint spans.

## Explicit circuit graph

Use analyst-defined node names, not invented Yamaha resistor designators:

- E: circuit reference.
- B: joined T51/12,T52/12 and verified AL harness nodes, assuming ideal harness wires only for the isolated reference.
- Cj: each T51/T52 column voltage supplied by its BA output stage.
- Aj: junction between that column's printed resistor (if present) and isolation diode anode; diode cathode at B.
- P: Panel VR10 wiper, connected through the T51 alternate-input diode to B.
- Mk: memory VR10 wiper for each bank, with its own drawn isolation diode to the memory output/harness network.
- Uq: AL pin14 for each verified M11–M18 receiver, joined to B under the isolated ideal-wire assumption. Each port retains its own unknown current law.

Each source branch can be open, direct-diode, resistor-plus-diode, or unresolved. Direct never means a short across the diode. Include inactive branches if a diode model admits leakage.

For current positive into B from sources, the DC KCL is:

```
Σj Ibranch_j + Ipanel + Σk Imemory_k
  = (VB-VE)/10000 + Σq IAL_q(VB, state_q, rails, temperature)
```

Here q runs over the eight verified M11–M18 ports. Do not replace the sum by eight times one port current without an explicit identical-device/state assumption.

The10k is a sourced component of this assembled T51/T52 graph, not a universal solver default. Any unverified harness continuation adds an explicit unknown port current, not zero.

For a resistor branch:

```
VCj - VAj = Rj * Ibranch_j
Ibranch_j = Dj(VAj - VB, temperature, history)
```

For a direct branch, VAj=VCj but the diode relation remains. Dj is an explicitly supplied device-port law; this document supplies no default0.6V or guessed saturation current. Panel and memory pot networks require their own loaded-wiper KCL plus their rail/selection-state equations. Their source resistances are position-dependent and their rails are not all presumed ideal or continuously active.

BA's +10.6V rail is not Cj. A BA column drives many output rows, so its load is the sum of its branch currents across the complete selected column. A finite BA model must solve those currents jointly; solving AL alone requires an explicitly prescribed/measured Cj boundary and cannot authenticate the actual BA voltage.

## Isolated mathematical reference proposal

First implement a test-only node residual/equation builder, separated from HardwareControl's illustrative divider and all production audio. It should accept:

1. An explicit topology and component inventory, including exactly one sourced shunt for this graph.
2. Supplied source-boundary voltages or BA port equations, with the choice recorded.
3. Explicit diode port laws and derivatives, without a hidden default.
4. Each receiver port law (or unresolved marker); missing laws return unresolved rather than dropping loads.
5. Selection states and panel/memory source networks; no invented mutual-exclusion behavior.

Outputs should be electrical nodes, branch currents, KCL residuals and model-assumption tags, never normalized controls or patch values. For nonlinear solving require convergence diagnostics and bounded residual tolerances; do not silently clamp a failed solve. This proposal does not authorize adopting a particular diode or IG00152 law.

An independently derivable **synthetic fixture** is useful for tests. Under explicitly assumed ideal source Vs, constant diode drop Vd, series R, one shunt Rp, all alternate sources disabled, and a prescribed total receiver current IL:

```
VB = ((Vs-Vd)/R - IL) / (1/R + 1/Rp)
ΔVB = -(R || Rp) * IL
```

This formula applies only while the assumed diode stays conducting and R>0. Direct-diode and off-state cases need separate constraints. A supplied linear receiver G to ground instead gives VB=(Vs-Vd)/(1+R*(1/Rp+G)). These identities verify the equation builder; they do not identify Yamaha's receiver. No numerical bias-current bound is asserted.

Minimum tests: exact single-shunt counting across companion boards; no-load fixture equivalence; explicit non-ground-reference load; duplicate receiver currents summing once each; unknown receiver refusing a voltage; active/off diode cases; alternate-source injection; residual/convergence diagnostics; finite BA source impedance coupled to a second synthetic output. Every numerical fixture is labeled synthetic/conditional.

## Remaining quantitative gates

- Native M41 tracing, M13–M18 AL fanout and the original IG00152 guide entry have now been inspected. These close topology gaps, not the receiver-current law.
- Resolve the guide's iG00152/iG00231 Part No. discrepancy against another original printing or manufacturer data before stronger part-specific conclusions.
- Seek a numeric AL port specification or calibrated input-current measurements across operating voltage, state, supply and temperature. The unvalued internal resistor is not a usable numeric load.
- Reconcile panel/memory enable rails and BA loaded output for the selected mode. Only after these gates can a loaded AL bus voltage be calculated as a hardware model.
- An isolated symbolic/residual reference with explicit unresolved ports is possible now; production conversion remains blocked. No default diode drop, infinite receiver impedance or guessed resistance is authorized.

## Reproducibility

Source jobs: Panel1/MKB run40790769942 jobs123175135484/123175135174; T51/T52 wiring run40801679102 jobs123210997016/123210997022; M41 run40784517382 job123154243112; M42/M44 run40785572946 jobs123157781827/123157781892; BA run40788842580 job123168711760.

Panel/MBK/T51/T52 crops below are native. M41/M42/M44/BA crops are previously emitted1800×1180 overview crops, explicitly resized; simple labels/edges were inspected, but no fine unreadable component identification is asserted. M52 was inspected and found to cover VCA-related ICs rather than the AL receiver; no AL claim relies on it.

```json
{"page": "panel1", "url": "https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/62%20-%20Panel%201%20Circuit%20Diagram.jpg", "sha256": "da50d3e9c77b63b30e8077799d65f03a3416b567dadb16e5bf0c692ac9a80708", "bytes": 1871278, "width": 5100, "height": 3300, "purpose": "Original Yamaha schematic analysis; not measured hardware evidence"}
{"id": "panel1-native-r0-c1", "source_box": [2136, 0, 4464, 2264], "width": 2328, "height": 2264, "resized": false, "png_sha256": "5eb1208f1a9cb2e74d1fb2a6aed90569b9ff24326c79438f15869edf1e8b6894", "base64_length": 1537516}
```

```json
{"page": "mkb", "url": "https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/63%20-%20Panel%201%20%28MKB%29%20Circuit%20Diagram.jpg", "sha256": "6050a23f85823be719aab033fd90ed3d4f1038409e38c089a64c96b658a08fcb", "bytes": 2047158, "width": 5100, "height": 3300, "purpose": "Original Yamaha schematic analysis; not measured hardware evidence"}
{"id": "mkb-native-r0-c1", "source_box": [2136, 0, 4464, 2264], "width": 2328, "height": 2264, "resized": false, "png_sha256": "3f5268b259ec434de1d073a9ea843fbc7ea1bba83ea7441fec0b978825d04de1", "base64_length": 1793028}
{"id": "mkb-native-r1-c1", "source_box": [2136, 2136, 4464, 3300], "width": 2328, "height": 1164, "resized": false, "png_sha256": "d07e852f1f4d5e0447007bb3317ca7b8edbe60f1a4be8a819a1d8c162d0a67bf", "base64_length": 790380}
```

```json
{"page": "t51w", "url": "https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/32%20-%20T51%20Circuit%20Board%20%26%20Wiring.jpg", "sha256": "8b95a93a2466865b01fea466b7613bd91f34714cccee49b381f819aff7c79f13", "bytes": 1569691, "width": 5100, "height": 3300, "purpose": "Original Yamaha schematic analysis; not measured hardware evidence"}
{"id": "t51w-native-r0-c1", "source_box": [2136, 0, 4464, 2264], "width": 2328, "height": 2264, "resized": false, "png_sha256": "2930c2dc7d6954943059376441566a9c0bb79577dba5927760103d30c58d3540", "base64_length": 636452}
```

```json
{"page": "t52w", "url": "https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/34%20-%20T52%20Circuit%20Board%20%26%20Wiring.jpg", "sha256": "604751efb4a4732d0ab15e47de1d6a9e22c8770a45887913323568feba2be8f5", "bytes": 1399081, "width": 5100, "height": 3300, "purpose": "Original Yamaha schematic analysis; not measured hardware evidence"}
{"id": "t52w-native-r0-c1", "source_box": [2136, 0, 4464, 2264], "width": 2328, "height": 2264, "resized": false, "png_sha256": "fc2b7e10b9c44e05451b5c41934abcbaf5ecf8f40d56721447f544e3a75343e7", "base64_length": 444260}
```

```json
{"page": "m", "url": "https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/41%20-%20M%20Circuit%20Diagram.jpg", "sha256": "1278f17ff05cd82b2b84153838bf95109d3f1ea1e864119de0a3f4a4af4f0080", "bytes": 1405965, "width": 5100, "height": 3300, "purpose": "Original Yamaha schematic analysis; not measured hardware evidence"}
{"id": "m-r1-c0", "source_box": [0, 1586, 2614, 3300], "width": 1800, "height": 1180, "resized": true, "png_sha256": "717ac5c00b36468880d9c938fb80d8ce35dd1bb16c5e994c6258c37d8705855a", "base64_length": 325364}
```

```json
{"page": "mics42", "url": "https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/42%20-%20M%20Circuit%20ICs.jpg", "sha256": "5698fdfd19600f6b0b8b8f9dfdfc70723248b4b33e8daad39b9f9c0ee11cc24d", "bytes": 1820648, "width": 5100, "height": 3300, "purpose": "Original Yamaha schematic analysis; not measured hardware evidence"}
{"id": "mics42-r0-c1", "source_box": [2486, 0, 5100, 1714], "width": 1800, "height": 1180, "resized": true, "png_sha256": "fe187ec45ab2e9a1be187f3f9ee441338e4be036ec6240cfffe02c78d07777ce", "base64_length": 466492}
{"id": "mics42-r1-c1", "source_box": [2486, 1586, 5100, 3300], "width": 1800, "height": 1180, "resized": true, "png_sha256": "bc92583d56eea46baa0d3ce08527027894010e6fb29419100045acde9058beea", "base64_length": 420596}
```

```json
{"page": "mwiring44", "url": "https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/44%20-%20M11-12%20Circuit%20Board%20%26%20Wiring.jpg", "sha256": "3320dd2f625c79d7cca9b98abd5cade9d1b95477e6d8990a93a296c9cda71256", "bytes": 2101353, "width": 5100, "height": 3300, "purpose": "Original Yamaha schematic analysis; not measured hardware evidence"}
{"id": "mwiring44-r0-c0", "source_box": [0, 0, 2614, 1714], "width": 1800, "height": 1180, "resized": true, "png_sha256": "693f348640d2922288caf1fcd0a6164e6e4edbeda7074b6571daa76d7bc76135", "base64_length": 569556}
{"id": "mwiring44-r1-c0", "source_box": [0, 1586, 2614, 3300], "width": 1800, "height": 1180, "resized": true, "png_sha256": "0f7b8fef1cf41d32911c4996d328b2bc8759f576b2c64ae34b2259f8e522d35b", "base64_length": 608204}
```

```json
{"page": "ba", "url": "https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/27%20-%20BA%20Circuit%20Diagram.jpg", "sha256": "3a6cc81d8ac8895f97e85c3e5ccb0a2b978879f53a5804753d5993d77684204e", "bytes": 1005863, "width": 5100, "height": 3300, "purpose": "Original Yamaha schematic analysis; not measured hardware evidence"}
{"id": "ba-r0-c0", "source_box": [0, 0, 2614, 1714], "width": 1800, "height": 1180, "resized": true, "png_sha256": "435bc57819428b9e7332ec0a776e5c46b9532e1d7c4a8731b101e5850f4943db", "base64_length": 336252}
```

## Native AL follow-up provenance

Inspection commit 15ddb0b9cf600fe8fe236c886d674e22d0c8fe66; run40806676119. Jobs: M41 123227194365; M45 123227194634; M46 123227194497; M47 123227194715; guide 123227194565. All completed successfully. Native M41 focus supersedes the earlier resized overview for the direct AL/SL edges. Guide images are 1800×1272 PDF renders: the marker “full-native” means no further crop rescaling, **not native scan resolution**.

```json
{"page": "mwiring45", "url": "https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/45%20-%20M13-14%20Circuit%20Board%20%26%20Wiring.jpg", "sha256": "32545b5c15ecc7bd038901af9880da20905103dd7413a34b3e1db5183ccf8627", "bytes": 1403499, "width": 5100, "height": 3300, "purpose": "Original Yamaha schematic analysis; not measured hardware evidence"}
{"id": "mwiring45-native-r0-c0", "source_box": [0, 0, 2264, 2264], "width": 2264, "height": 2264, "resized": false, "png_sha256": "e86386407df062e3ef54ee9398cf0672bce91329be218f168070fed95765eb15", "base64_length": 702880}
{"id": "mwiring45-native-r0-c1", "source_box": [2136, 0, 4464, 2264], "width": 2328, "height": 2264, "resized": false, "png_sha256": "4bfcbe9744b1a616311f5dcc3ad4ed4524f2e5a056c2d1ea0c3f760ed158cc19", "base64_length": 1036732}
```

```json
{"page": "mwiring46", "url": "https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/46%20-%20M15-16%20Circuit%20Board%20%26%20Wiring.jpg", "sha256": "197cc76404db321de26f06a8e187e6b33b0690a14afe45eedf56744180c6632c", "bytes": 1337488, "width": 5100, "height": 3300, "purpose": "Original Yamaha schematic analysis; not measured hardware evidence"}
{"id": "mwiring46-native-r0-c0", "source_box": [0, 0, 2264, 2264], "width": 2264, "height": 2264, "resized": false, "png_sha256": "e9d338beb544153e258664b1de6b77943e3f25ea5d568f537f4b63f060181144", "base64_length": 869440}
{"id": "mwiring46-native-r0-c1", "source_box": [2136, 0, 4464, 2264], "width": 2328, "height": 2264, "resized": false, "png_sha256": "54825797b1e43dcbb73e8fe59b8292540ab8189a3831435218f82e045846d562", "base64_length": 929816}
```

```json
{"page": "mwiring47", "url": "https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/47%20-%20M17-18%20Circuit%20Board%20%26%20Wiring.jpg", "sha256": "2824471b66f408e70c6ef479f2d11bcda01d965bc3b0e34fea5fbfba9b62f56e", "bytes": 1294887, "width": 5100, "height": 3300, "purpose": "Original Yamaha schematic analysis; not measured hardware evidence"}
{"id": "mwiring47-native-r0-c0", "source_box": [0, 0, 2264, 2264], "width": 2264, "height": 2264, "resized": false, "png_sha256": "1581086a4a0d088aba998b5a133f4f36bdcd79523b652b8c3a38147eb6986c6d", "base64_length": 669744}
{"id": "mwiring47-native-r0-c1", "source_box": [2136, 0, 4464, 2264], "width": 2328, "height": 2264, "resized": false, "png_sha256": "2c4d88146a0432e1baf1ed92c84f4726230ee5c670fa95311a35d55f965873c7", "base64_length": 901492}
```

```json
{"page": "m", "url": "https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/41%20-%20M%20Circuit%20Diagram.jpg", "sha256": "1278f17ff05cd82b2b84153838bf95109d3f1ea1e864119de0a3f4a4af4f0080", "bytes": 1405965, "width": 5100, "height": 3300, "purpose": "Original Yamaha schematic analysis; not measured hardware evidence"}
{"id": "m-al-receiver", "source_box": [1600, 1950, 2450, 2900], "width": 850, "height": 950, "resized": false, "png_sha256": "8a4274e6fbf9afd5e6949151418a93b0a5b5a278931878cc67f2a595e1c95a48", "base64_length": 144860}
```

```json
{"page": "icguide_152-page33", "url": "https://www.goltsynth.com/resources/downloads/CS/yamaha_ic_guidebook.pdf", "sha256": "ff051aed0687cad4c4b246d145f85896c229e4cc679333328483118a3e7f8cc1", "bytes": 13608992, "width": 1800, "height": 1272, "purpose": "Original Yamaha schematic analysis; not measured hardware evidence"}
{"id": "icguide_152-page33-full-native", "source_box": [0, 0, 1800, 1272], "width": 1800, "height": 1272, "resized": false, "png_sha256": "025bed171b5e43d6ef8ddb813334354a621ed17f1b689bcd997016a25403ee87", "base64_length": 129428}
{"page": "icguide_152-page34", "url": "https://www.goltsynth.com/resources/downloads/CS/yamaha_ic_guidebook.pdf", "sha256": "ff051aed0687cad4c4b246d145f85896c229e4cc679333328483118a3e7f8cc1", "bytes": 13608992, "width": 1800, "height": 1272, "purpose": "Original Yamaha schematic analysis; not measured hardware evidence"}
{"id": "icguide_152-page34-full-native", "source_box": [0, 0, 1800, 1272], "width": 1800, "height": 1272, "resized": false, "png_sha256": "921ff0c8e97c1ce5de98fd547ec79450882b3febf85965f3809d24f2241c6bf6", "base64_length": 61640}
```
