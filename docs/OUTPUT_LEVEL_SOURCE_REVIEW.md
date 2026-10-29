# HIGH/LOW output level: bounded source review

2026-10-29. **Source audit / conditional passive-network proposal, not a production calibration. Independently reviewed.** The existing `kProvisionalLowOutputGain = 0.1` remains a placeholder; this review does not change it.

## What the switch actually selects

PN5 page67 draws SW1 as three mechanically linked changeover poles. T1 feeds LEFT J3 tip2, T2 GENERAL J2 tip2, and T3 RIGHT J1 tip2. The M contacts select unprimed PRA L(38), G(42), R(45); the B contacts select primed L′(37), G′(43), R′(46). The drawing associates the former with HIGH and latter with LOW. Jack terminal1 is the sleeve/shield reference, not an alternative audio tap. PRA54's repeated PN5-Jn-1 harness annotations must not be interpreted as shorting the two audio conductors: the PN5 drawing separates shield and switched signal conductors.

The foldout supplies the missing output-stage schematic: each preamp output feeds a **10 µF NP coupling capacitor** to a common tap node X. One branch passes through a series resistor to the unprimed HIGH terminal. A separate branch passes through **4.7 kΩ** to the primed LOW terminal, with a shunt to reference. The LOW divider remains connected when HIGH is selected; the switch selects the jack connection, not the existence of that divider.

PRA53's available image depicts wah, ring and expression sections, **not the left/right/general output section**. Consequently this review uses the foldout for the output schematic and PRA54/PN5 for board values and reciprocal wiring; it does not misattribute the output diagram to PRA53.

## Values and unresolved source discrepancies

| Item | Foldout reading | PRA54 native board reading | Status |
|---|---|---|---|
| LOW series branch, all three channels | 4.7 kΩ | 4.7K at each output area | Corroborated value/location |
| LEFT LOW shunt | 560 Ω | 560 beside L′ area | Consistent |
| RIGHT LOW shunt | 560 Ω | 560 beside output earth area | Consistent |
| GENERAL LOW shunt | 560 Ω | **560K** beside E above G area | Conflict: retain both; exact pad/trace reconciliation and preferably another source/hardware check needed |
| LEFT / GENERAL HIGH series | 560 Ω | 560 beside L/G areas | Consistent |
| RIGHT HIGH series | **56K** as printed/read in foldout | **560** beside R area | Conflict: do not silently normalize the foldout to the other channels |
| Preamp feedback fixed resistance | 220K | 150K in each relevant amplifier area | Active-stage source discrepancy |
| VR6 / VR7 / VR8 | B1M | B500K | Active-stage source discrepancy |

The board values are positionally corroborating evidence, not a claim that every copper connection has been independently reverse-engineered. In particular, the surprising GENERAL `560K` label is not corrected to `560` merely because another channel uses 560. The conflicting interpretations must stay explicit in any reference fixture. The drawing itself does not supply measured tolerances or installed trim positions.

## Conditional ratio—not an installed calibration

With an ideal zero-impedance preamp output, effectively shorted coupling capacitor (midband limit), ideal switch, open output load, and the **560 Ω shunt interpretation**, HIGH is unity relative to the preamp output and:

```text
LOW/HIGH = 560 / (4700 + 560)
         = 0.10646387832699619
         = −19.455954342950776 dB
```

For LEFT and RIGHT, the 4.7 kΩ / 560 Ω LOW divider values are consistent across the foldout and board. Their no-load ratio is therefore source-supported **within this conditional branch graph**, independently of the disputed RIGHT HIGH series value: with an open jack there is no current or drop in that series resistor. In both switch positions the driver sees the same permanently connected LOW divider, so finite linear source impedance and coupling-capacitor transfer also cancel in the ratio at frequencies where the response is nonzero. This assumes an ideal selector with no parasitic loading, the same stationary driver signal/state in both positions, no additional unshown tap loads, and truly open measurement load. Headphone loading is upstream and unchanged by that ideal selector; it is not another load on the primed/unprimed taps. GENERAL remains source-conflicted.

This is a nominal resistor-divider result, not a measured HIGH/LOW specification, not a universal −20 dB law, and not justification to replace the production scalar. Taken literally as that same shunt, GENERAL's board `560K` alternative would instead give `560000/(4700+560000) ≈ 0.991677` (−0.072595 dB). That large disagreement is an evidence gate, not an invitation to choose the more plausible-sounding value.

The HIGH series-resistor discrepancy does not affect the open-load limit, but strongly affects loaded output. Each external jack requires its own supplied impedance; a cable or amplifier input cannot silently be assumed infinite. A full response also needs the preamp's finite output impedance/current limit and the shared load of the headphone input.

## Bounded offline passive AC reference

The isolated reference starts at one explicit Thevenin preamp boundary `(Vs,Zs)`, then the 10 µF ideal coupling capacitor, the two output branches, an ideal HIGH/LOW switch and an explicit load `ZL`. Require the HIGH series resistance `Rh`, LOW series resistance `Ra` and shunt `Rb` explicitly where source conflicts remain. No default unknown source/load/trim should become ideal or open implicitly. This is a conditional graph, not an installed-component decision.

For nonzero frequency, `Zc=1/(jωC)` and `A || B = AB/(A+B)`:

```text
HIGH: Zp = (Ra+Rb) || (Rh+ZL)
      Vjack/Vs = Zp/(Zs+Zc+Zp) × ZL/(Rh+ZL)

LOW:  Q = Rb || ZL
      Zp = Ra+Q
      Vjack/Vs = Q/(Zs+Zc+Ra+Q)
```

Here the unused jack branch is open; the LOW resistor branch itself stays installed in HIGH mode. Open/short loads and DC require their explicit limits, not division by zero. For the ideal open-load fixture the common capacitor/source loading is the same in both positions, so the divider ratio also cancels those common factors away from a zero transfer. Finite loads generally remove that cancellation. A 10 µF capacitor alone does not establish a fixed 10 Hz pole.

Verification must cover this branch graph and conflicts with a separately stamped nodal oracle, both switch states, source and load current conservation, explicit open/short limits, complex loads and loss of representability. A synthetic all-560 fixture may test arithmetic but must not be labeled measured hardware. Transient switching, contact resistance/bounce and saturation are outside this proposal.

## Stereo, GENERAL and headphone boundaries

- The foldout LEFT amplifier is IC4 half1, RIGHT IC3 half2, GENERAL IC4 half2. LI(39)/RI(49) originate at PN2 stereo VOLUME wipers. The GENERAL amplifier is a separate summing path with contributions from both input-side nodes, not a proven arithmetic average of already loaded LEFT/RIGHT jacks. The 100 kΩ input/shared summing branches, 47 kΩ paths, feedback trims and muting Tr1/Tr2 require an independent active-stage derivation before claiming a full three-output gain matrix.
- HL(40) and HR(48) leave the preamp outputs **before** the output coupling capacitors and HIGH/LOW switch. PRA54 explicitly sends HL to SUB-LI(67), HR to SUB-RI(32). Thus HIGH/LOW does not directly select the headphone audio path. Supply/output-impedance interaction is not ruled out by that routing statement.
- SUB29 shows two IC11 feedback amplifiers, 100 kΩ inputs and 150 kΩ feedback, with discrete complementary output sections Tr7–10 / Tr11–14, diode biasing and 22 Ω output resistors. Their outputs are RO(33)/LO(68). PN5 page67 carries these to the headphone jack and depicts alternative direct-wired and capacitor-coupled arrangements; a delta mark appears beside the direct-wired drawing. The capacitor-coupled arrangement contains paired 470 µF/25 V capacitors and 10 kΩ bias resistors. This review does not determine the applicability/serial range of those alternatives or derive the headphone's nonlinear transfer/load response.
- Device frequency response, supply headroom, clipping/current limiting, startup muting, noise, trim calibration, component tolerances and actual external loads remain missing. No ear-tuned gain or generic `softClip` is source-validated by this audit.

## Provenance

All inputs were existing local Yamaha service scans, inspected natively; no new network download or software installation was needed. Source artifacts remain untracked research inputs, not publication payloads. Crop boxes are original pixel `[left,top,right,bottom]`; PNG crops are unresized. Original and regenerated-crop SHA-256 values were checked locally. Local manifest: `artifacts/w8-output-20261029/provenance.json`.

### Originals

| Source | Dimensions | SHA-256 |
|---|---|---|
| [pra54.jpg](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/54%20-%20PRA%20Circuit%20Board%20%26%20Wiring.jpg) | 5100×3300 | `88703d2b43189003e3276ca7837556ef2ac5205dd057d8f5b0c6b320e1042c69` |
| [67.jpg](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/67%20-%20Panel%205%20(EJ,%20EXP,%20PFS)%20Circuit%20Diagram.jpg) | 5100×3300 | `0a81a6812213e84993f3ab9d3cdf5cd070914832bec1b94f5374a4916375d4b2` |
| [cs80_foldout_schematic.jpg](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/cs80_foldout_schematic.jpg) | 12185×5972 | `23de2fc4ef4bbbc6c171979aa586a2f1c1fb11d7f6116656153a752b31709b38` |
| [29.jpg](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/29%20-%20SUB%20Circuit%20Diagram.jpg) | 5100×3300 | `b81c7e5ad2927ac04d5912bfc455a631237f512a0c7700bda41250a14bd33e60` |

### Native crops

| PNG | Source | Box | SHA-256 |
|---|---|---|---|
| `pra54-output-board-native.png` | pra54.jpg | `[1080, 370, 1740, 1260]` | `551514d8d15fe6910aff81eef3b19021335b2188f25a371d479f6df1861109b5` |
| `pra54-output-harness-native.png` | pra54.jpg | `[350, 390, 1140, 1280]` | `8676106335fce9ee271c6d235e72c41fc518ddf053b39456a4bd79416cb16123` |
| `panel67-output-switch-native.png` | 67.jpg | `[2760, 670, 3890, 1890]` | `00c1c0f9d0165d56000e7a014c0372d7864936c29741fdf229b90e156a404e6a` |
| `panel67-headphone-native.png` | 67.jpg | `[3960, 2250, 4900, 2930]` | `16074f5b320608d2aa8fba46fda9577e8b5ed6c23e44767ee2ed01151649ff5b` |
| `foldout-output-native.png` | cs80_foldout_schematic.jpg | `[10000, 1800, 11650, 2950]` | `f990aa037912b7aa2bceeae6e85e87bce9300a3170e85c967eb6d1a88ae71a51` |
| `sub29-headphone-native.png` | 29.jpg | `[3400, 2140, 4740, 2960]` | `c02fd0bd2638aece25eccc06c7a568270ed9db827875d07ca4b6df16b46ddf8e` |

Independent review checked the native switch/branch topology, printed conflicts, conditional loaded formulas and no-load ratios, headphone boundary, original hashes and all six regenerated crop hashes. Approval is for this bounded source account, not an installed all-channel calibration or production substitution.

## Implemented API and numerical domain

`src/reference/OutputTaps.h`, namespace `cs80::reference::output_taps`, implements only the conditional single-channel passive network above. It is not included by production audio. `Inputs` requires a mode, frequency, complex source voltage, finite source impedance with nonnegative real part, all three strictly positive branch resistances, and an explicit load kind (`open`, `shorted`, or `finite`). A finite load must have finite nonzero impedance with nonnegative real part; a zero load is expressed by `shorted`, not a substituted tiny resistor. Signed reactance is allowed. Mode/load defaults are unknown sentinels and all required numeric defaults are NaN. The impedance member is deliberately ignored for explicitly open/shorted loads. Nothing selects an installed component revision implicitly.

The sole fixed circuit value is the documented ideal 10 µF coupling capacitor. `Result` exposes capacitor input/drop, common and both tap voltages, selected jack voltage, source/capacitor current, HIGH/LOW branch currents, shunt current and external-load current. Current signs are toward the taps from the source and toward reference through the shunt/load. DC returns the exact settled limit: source voltage across the capacitor, zero downstream voltage and current. The HIGH branch has exactly zero current when unselected or explicitly unloaded, while its voltage remains the common-node voltage. SHORT has zero jack voltage but generally nonzero load current.

Arithmetic uses long-double intermediates where available, but does not depend on a wider exponent range. Capacitor impedance is calculated reciprocal-first to avoid overflowing `2*pi*f` for a representable high-frequency capacitor drop. Intermediate products/divisions are checked for nonfinite values and whole-complex underflow when the graph requires a nonzero result. Final values exceeding the double range or nonzero components lost on narrowing are rejected. Invalid input raises `invalid_argument`; unresolved numerical arithmetic raises `overflow_error`.

The source-series denominator also rejects `(abs(Zs)+abs(Zc)+abs(Zp))/abs(Zs+Zc+Zp) > 1e8`. This is a conservative cancellation-domain policy, **not** an impedance specification, a measured resonance limit, an exact condition number or a guarantee of uniform relative accuracy for every phasor component. Finite input alone does not guarantee acceptance; especially tiny currents or near-cancelling reactive impedances may be rejected rather than silently modeled as zero. Exact zeros from zero excitation, open branches, a shorted jack or DC remain valid.

## Verification record

`tests/OutputTapsTests.cpp` independently stamps four physical nodes (pre-capacitor, common, HIGH tap, LOW tap), with separate complex capacitor/resistor stamps and explicit ideal-source/short constraints. It does not reuse the implementation's impedance reduction. Tests compare node ports to that oracle and verify source/capacitor/branch/load KCL and KVL. They include 600 deterministic randomized unequal fixtures, reactive sources/loads, both modes, open/short/finite loads and their limiting cases, exact DC, complex linearity, no-load cancellation and finite-load ratio changes, and explicit missing/invalid/numerical cases.

Final standalone strict C++20 compilation (`-O2 -Wall -Wextra -Wpedantic -Werror`) passes **11,016 checks**. A temporary header variant replacing long-double arithmetic with double passes the same **11,016 checks**, including the `f=1e308` nonzero capacitor-drop regression and rejection of a mathematically nonzero but unrepresentable source current (`Vs=1e-300`, `Zs=1e100`). These are arithmetic fixtures, not hardware values.

Eleven isolated mutations are detected: changing coupling capacitance; omitting the permanently installed LOW branch in HIGH mode; changing each of the three resistor values; ignoring source impedance; swapping switch modes; treating finite loads as open; removing cancellation rejection; removing intermediate-underflow rejection (forced-double variant); and restoring the overflowing capacitor-frequency expression (forced-double variant). A valid fixture's unexpected rejection also fails the suite. Temporary report: `/tmp/output-taps-mutation-results.json`. This establishes sensitivity to the declared topology/numerical policies, not physical calibration. Full CI status belongs to the published commit.

Standalone AddressSanitizer + UndefinedBehaviorSanitizer also passes 11,016 checks with leak detection disabled. The default LeakSanitizer launch reports an environment/ptrace incompatibility; this run does not verify leak detection. This limitation is separate from the passing address/undefined-behavior checks.
