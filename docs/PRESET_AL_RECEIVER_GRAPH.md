# Rank-I preset AL receiver graph and reference-model gate

2026-10-29. Source analysis only. No production conversion or new preset values. This is a bounded trace of output12 (VCF attack level), not a completed electrical clone.

## Observed connectivity

| Edge | Direct source evidence |
|---|---|
| T51/12(21) ↔ T52/12(21) | PCB/wiring pages32 and34, native r0c1 |
| T52/12(21) ↔ M11/AL(59) | page34 native r0c1; reciprocal page44 |
| M11/AL(59) ↔ M12/AL(59) ↔ M13/AL(59) | page44 r1c0; reciprocal M11/M12 entries |
| PN1 VR10 wiper2 → T51/12′(20′) → isolation diode → output12 bus | Panel1 page62 and T51 schematic/PCB wiring |
| Memory VR10 diode-output network → M18/AL(59) | MBK page63 native r0c1; memory-bank interconnections in r1c1 |
| M-card terminal59 AL → IC12 AL pin14 | M41 r1c0; direct drawn wire, no external divider on this connection |

The separate memory and panel paths must not be collapsed into an ideal single control source. The memory drawing visibly includes isolation diodes. Do not count pot resistance as a permanent ground load through a reverse-biased diode.

The complete M13–M18 daisy chain has not been individually traced in this pass. M18 memory injection and the verified M11–M13 edges support a shared rank bus, but the number and identity of all connected IC loads must remain an explicit topology input until the remaining wiring sheets are checked. Eight voices alone is not sufficient wiring proof.

On T51 this analog row has its documented local10k shunt to E. T52 has no additional local shunt. The wire joining their unprimed output12 buses is explicit; therefore this particular assembled graph contains the T51 shunt once, including when a T52 tone sources the bus. This does not change board-local metadata or resolve crossed T52 primed-terminal applicability generally.

## Receiver identity and quantitative boundary

M42 identifies the VCF-EG as IG00152. Its pin14 AL description specifies an input span0–10V and attack-level span0..+5V. Pin15 SL is normally fixed at0V; pin16 IL controls the negative initial level separately. These are pin-function/range evidence, not a supplied input-resistance, input-bias-current specification, linear interpolation equation, or a guarantee that every voice produces an identical endpoint.

M41 shows the AL terminal directly entering that port. It is therefore inappropriate to invent an external AL divider or assume the receiver has infinite impedance because the manual calls it a buffer-voltage input. The next missing quantity is the IG00152 AL port current versus voltage, gate/state, supply and temperature.

The envelope's subsequent LC/time-control/integrator/buffer and unequal HP100k/LP47k feed paths are downstream stages; bus12 is neither final envelope output nor filter cutoff voltage. No0.5×bus-to-octaves conversion follows from the endpoint spans.

## Explicit circuit graph

Use analyst-defined node names, not invented Yamaha resistor designators:

- E: circuit reference.
- B: joined T51/12,T52/12 and verified AL harness nodes, assuming ideal harness wires only for the isolated reference.
- Cj: each T51/T52 column voltage supplied by its BA output stage.
- Aj: junction between that column's printed resistor (if present) and isolation diode anode; diode cathode at B.
- P: Panel VR10 wiper, connected through the T51 alternate-input diode to B.
- Mk: memory VR10 wiper for each bank, with its own drawn isolation diode to the memory output/harness network.
- Uq: each verified IG00152 AL pin14, joined to B only when its wiring edge is verified.

Each source branch can be open, direct-diode, resistor-plus-diode, or unresolved. Direct never means a short across the diode. Include inactive branches if a diode model admits leakage.

For current positive into B from sources, the DC KCL is:

```
Σj Ibranch_j + Ipanel + Σk Imemory_k
  = (VB-VE)/10000 + Σq IAL_q(VB, state_q, rails, temperature)
```

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

## Next bounded evidence steps

- Native M41 focus around IC12/AL: original box[1600,1950,2450,2900] is a proposed focus from the inspected overview, not already inspected native evidence.
- Inspect remaining M13–M18 harness sheets to close AL fanout and the M18 memory junction.
- Inspect original IG00152 entry for an AL input-impedance/port-current specification or internal input circuit. If absent, retain port-law unknown and seek calibrated input-current measurements rather than selecting arbitrary resistance.
- Reconcile panel/memory enable rails and BA loaded output for the selected mode. Only after these gates can a loaded AL bus voltage be calculated as a hardware model.

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
