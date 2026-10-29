# Preset receiver ports: 26-output rank-I source ledger

2026-10-29. W9/M2 **partial topology evidence**, not a loaded preset solve, calibrated control mapping, or production voicing change. No hardware was measured. First-hop wiring below is newly inspected at native resolution; existing independently reviewed AL and cutoff evidence is reconciled, not reclassified as a new independent review. An independent reader verified all26 first-hop rows, M pin/passive traces, three original hashes and six regenerated crop hashes; no substantive discrepancy was found. That bounded source review does not establish missing fanout or IC laws.

## Scope and notation

Output names/numbers follow `HardwareControl.h`. `7(27)` means output 7 at physical terminal 27; a prime is significant. `T51` and `T52` denote rank I only. Every row below has an explicit reciprocal T51–T52 unprimed bus connection on pages32/34. This establishes companion continuity, not a second shunt: the source matrix has one T51 10k-to-E shunt per analog row and none on switch rows4/5; T52 has no local shunts. Alternate primed/PANEL and memory diode networks are not collapsed into permanent resistive loads.

**All 26 first-hop destinations are identified.** Full assembled receiver counts are established here only for the previously reviewed AL chain and downstream cutoff branches. Other rows' “M11” means a demonstrated first voice, **not one total load**. The remaining seven harness links and memory endpoint must be checked individually. Rank-II equivalence is not inferred.

Source keys: **W32/W34** = native terminal crops below; **M-EG/M-AA/M-LP** = M41 crops below; **AL** = [PRESET_AL_RECEIVER_GRAPH.md](PRESET_AL_RECEIVER_GRAPH.md); **CUT** = [LOADED_CONTROL_EVIDENCE.md](LOADED_CONTROL_EVIDENCE.md). E/ground means drawn circuit reference, not proof of an ideal or noiseless reference. “Direct” below refers to a wire into the indicated pin, not infinite impedance or unity functional gain.

## The 26-output table

All first-hop entries use W32 and W34. PN1-MBK-M3 names are printed harness destinations, **not proof that a memory pot permanently loads the factory bus**. Internal SUB/TRG/BA transfer and additional loads remain unresolved unless explicitly stated.

| Out / control | T51/T52 terminal and observed external destinations | Receiver path, fanout, reference / passive elements | Exact remaining trace or electrical boundary |
|---|---|---|---|
| 1 Speed | 1(33) → SUB-VC1(12); MBK-M3-VR1 | SUB input; count/current/reference/gain/caps not established | SUB VC1 circuitry and memory diode/selection network |
| 2 PWM | 2(32) → SUB-D1(47); MBK-M3-VR2 | SUB input; not a direct M PWM connection | SUB D1 output path to M PWM and loaded transfer |
| 3 PW | 3(31) → M11-PW(40) | First M receiver identified; full chain unverified | M PW40 passive/IC path and M12–18 chain |
| 4 Square switch | 4(30) → M11-G1(45) | Switch bus, no matrix shunt | M G1 transistor/FET network, switching threshold/current, harness |
| 5 Saw switch | 5(29) → M11-G2(44) | Switch bus, no matrix shunt | M G2 transistor/FET network, switching threshold/current, harness |
| 6 Noise | 6(28) → SUB-C1(37); MBK-M3-VR4 | SUB input; no direct M gain assumed | SUB C1 VCA/noise network, memory isolation |
| 7 HPF | 7(27) → R51-FH(55) **and BA-f05(22′)** | CUT: FH common bus has eight39k branches to M11–18 F1(51), IC10 IG00156 pin4. Each pin has graded grounded shunt and10nF capacitor; see below | BA-f05 branch is additional bus loading; BA source/selection graph and IC10 current laws remain unknown |
| 8 RESH | 8(25) → R51-QH(57) | R51 destination identified; not a direct Q-pin voltage | R39 QH resistor fanout, M Q1 path and IC10 Q-input law |
| 9 LPF | 9(24) → R51-FL(55′) **and BA-f06(24′)** | CUT: FL common bus has eight39k branches to M11–18 F2(55), IC11 IG00156 pin4; graded grounded shunt and10nF capacitor | BA-f06 additional load; BA source/selection graph and IC11 current laws |
| 10 RESL | 10(23) → R51-QL(15′) | Preserve printed15′ (not55′) | R39 QL resistor fanout, M Q2 path and IC11 Q-input law |
| 11 IL | 11(22) → M11-IL(58) | M-EG: direct to IC12 IG00152 IL pin16; no external divider/cap on this wire | IL input-current and level law; M12–18 chain; no AL law reused |
| 12 AL | 12(21) → M11-AL(59) | AL: verified M11–M18 chain, direct IC12 AL14 on each; SL15 grounded; one source-bus10k shunt | Eight separate AL current laws; panel/memory/BA operating states |
| 13 VCF attack | 13(19) → M11-AF(62) | M-EG: direct IC12 AT9; no external series R/cap on AF wire | AT current/control-to-time law and remaining harness |
| 14 VCF decay | 14(18) → M11-DF(61) | M-EG: direct IC12 1DT10; no external series R/cap on DF wire | 1DT current/control-to-time law and remaining harness |
| 15 VCF release | 15(17) → SUB-RF(55); MBK-M3-VR13-4 | **Not established as direct bus→M RF60**. M-EG independently shows M RF60→IC12 2DT11 | SUB RF input/output and mode switching to M RF60; then harness and IC law |
| 16 VCF level | 16(16) → M11-LP1(27) | M-LP:100k series to IC4 IG00151 LI2,0.01µF to ground at pin; first voice only | Remaining harness; LI pin-current law; actual gain law, rails and trim state |
| 17 Sine | 17(15) → M11-LP2(24) | M-LP:100k series to IC3 IG00151 LI2,0.056µF to ground at pin; first voice only | Remaining harness and LI pin-current/gain law; do not equate bus17 to signal amplitude |
| 18 VCA attack | 18(13) → M11-AA(67) | M-AA: direct IC13 IG00159 AT9, no external series R/cap on AA wire | AT current/time law and remaining harness |
| 19 VCA decay | 19(12) → M11 terminal66 (W34 lettering appears `1A`; M41 names it DA) | M-AA: DA66 direct IC13 1DT10 | Reciprocal wiring-page lettering check; 1DT current/time law; remaining harness |
| 20 VCA sustain | 20(11) → M11-SL(64) | M-AA: direct IC13 SL15; IL16 grounded; no external divider/cap on SL wire | SL current/level law and remaining harness |
| 21 VCA release | 21(10) → SUB-RA(56); MBK-M3-VR19-4 | **Not established as direct bus→M RA65**. M-AA independently shows M RA65→IC13 2DT11 | SUB RA switching/output route to M RA65, harness and IC law |
| 22 VCA level | 22(9) → R51-LV(93′) | R51 level network, not direct IG00151 LI voltage | R39 LV distribution/mixing and M LC path; source/receiver laws |
| 23 Initial brilliance | 23(7) → TRG1-IBI(33); MBK-M3-VR21 | Touch-control input; downstream per-voice IB is not the preset bus itself | TRG1 IBI sampling/gain/reference/cap network and R51 IB destinations |
| 24 Initial level | 24(6) → TRG1-ILI(65); MBK-M3-VR22 | Touch-control input | TRG1 ILI sampling/gain/reference/cap network and level destinations |
| 25 After brilliance | 25(5) → TRG2-ABI(33); MBK-M3-VR23 | Touch-control input | TRG2 ABI gain/reference/cap network and R51 AB destinations |
| 26 After level | 26(4) → TRG2-ALI(65); MBK-M3-VR24 | Touch-control input | TRG2 ALI gain/reference/cap network and level destinations |

## What the verified receiver subgraphs do and do not provide

### Cutoff outputs7/9

CUT establishes the R51 eight-voice FH/FL fanout and all reciprocal M11–18 F1/F2 links, including primed connector distinctions. At each IG00156 pin4, HP shunt R6 is2.7/2.2/1.8k and LP shunt R7 is3.3/2.7/2.2k for IC grades A/B/C (not synthesis ranks). Both have0.01µF to ground. Other summing inputs, trim branches and the unequal100k/47k local envelope paths remain part of the node equation. There is **no single fixed bus-to-pin gain**: it depends on all loaded sources and receiver currents.

W32 newly makes explicit that these preset buses also join BA-f05/f06. Therefore the conditional CUT common-source boundary cannot be promoted to a complete factory-preset bus without tracing those BA paths. This ledger establishes their presence, not their electrical direction or impedance.

### Envelope outputs11–15 and18–21

M41 identifies IC12 as IG00152 and IC13 as IG00159. The pin wiring above is directly visible. Their timing-control terminals are not grounded resistors, and the82k plus50k trim around the EG's separate current/integrator circuitry does not supply an input impedance or an output13/14/18/19 time constant. No capacitor is drawn on the listed direct control wires; that is **not** a zero total pin capacitance claim. Release controls require the intervening SUB circuitry before identifying an effective M-pin voltage.

AL's0–10V input /0..+5V attack-level endpoints and unvalued internal resistor are already documented in AL. Those ranges are not a numeric input-current law, nor a linear interpolation guarantee. They cannot be copied to IL, AT,1DT,2DT or IG00159 SL. AL's guidebook part-number discrepancy remains open.

### Level outputs16/17

The documented series resistors and grounded caps permit a port equation, not a calibrated gain:

`(Vbus − VLI)/100000 = C*dVLI/dt + I_LI(VLI, state, rails, temperature)`.

Use C=10nF for LP1/IC4 and56nF for LP2/IC3. At settled DC the capacitor current is zero, not the chip current. `VLI=Vbus` follows only under an explicitly assumed zero DC pin current. Neither100k×C is the complete instrument settling time without upstream bus loading and the chip law. IC3/4 have separate EI1 trim paths through100k from100k pots spanning+15/−15V; their settings/current laws and signal paths must remain separate from the LI control network.

### Fanout/current bookkeeping

For every missing receiver, retain an unresolved port rather than deleting it. Once a chain is verified, sum currents for the actual individual ports; do not assert identical state, bias or current across eight voices. The source KCL must include the single sourced matrix shunt, all actual receiver/intermediate-board branches, BA/PANEL/memory networks and the coupled selected-column driver. This pass neither supplies a diode law nor an ideal BA voltage.

## Reproducible primary provenance

All originals are Yamaha service scans,5100×3300 pixels, obtained from the original index's named pages. W32/W34 were directly downloaded2026-10-29; M41's existing original was byte-hash verified. Crops use Pillow `Image.open(source).crop(box).save(path)` without conversion/resizing; boxes are `[left,top,right,bottom]`, right/bottom exclusive. Local artifacts: `artifacts/w9-receivers-20261029/`; primary scans are evidence inputs, not proposed repository payloads.

| Source | Primary URL | SHA-256 original bytes |
|---|---|---|
| W32 | [p32 T51 wiring](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/32%20-%20T51%20Circuit%20Board%20%26%20Wiring.jpg) | `8b95a93a2466865b01fea466b7613bd91f34714cccee49b381f819aff7c79f13` |
| W34 | [p34 T52 wiring](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/34%20-%20T52%20Circuit%20Board%20%26%20Wiring.jpg) | `604751efb4a4732d0ab15e47de1d6a9e22c8770a45887913323568feba2be8f5` |
| M41 | [p41 M circuit](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/41%20-%20M%20Circuit%20Diagram.jpg) | `1278f17ff05cd82b2b84153838bf95109d3f1ea1e864119de0a3f4a4af4f0080` |

| Native crop / key | Source box | PNG SHA-256 |
|---|---|---|
| t51w-terminals | `[1690, 560, 2460, 2500]` | `8dabb411304d7ee39020cfceb32114d5e64b1d2e427dfe1d99d17ee741dfeced` |
| t52w-terminals | `[1750, 650, 2600, 3000]` | `78847f865ed722c7de8e0116b106c936c386c922aedb8352035013c6a6f756e9` |
| m41-vcf-eg | `[1550, 2000, 2430, 2870]` | `4dc098b0695e917b067e4f93ee587708351f87acd83b0d004f247e867f1f55d3` |
| m41-vca-eg | `[3200, 1920, 4030, 2850]` | `32ca6d5d05d41a0e46326c0e6e4e95adb6536d08a37de7795aac28f77f4cbc7f` |
| m41-lp-controls | `[2300, 500, 3250, 1150]` | `0f65d38cf8b2feb1007bfd1172751c3993ae8a0054f4584a497aec11a2ed4838` |
| m41-ic-list | `[3900, 2120, 4750, 2540]` | `b0c6d111e616ad6b266b9d27758dd06955b044c6153b3393f1efccdf17fcd01a` |


## Next bounded evidence gate

1. Completed: independent W32/W34 first-hop and M pin/capacitor review, including the preserved terminal66 lettering conflict and QL15′.
2. Inspect BA27/28 f05/f06 and SUB release RF/RA graphs before treating cutoff/release buses as direct controls.
3. Close the M11–18 non-AL envelope and LP1/LP2 harness chains, then R51 Q/LV fanout. Carry references and each branch, not a presumed eightfold load.
4. Trace TRG touch-control and SUB speed/PWM/noise receivers; separately verify rank-II wiring and panel/memory selection.
5. Obtain explicit IC current laws or retain unresolved ports. Only a narrow offline graph/inventory helper with unknown-port rejection is supportable before these laws; no helper was implemented in this source pass.

The25 unreadable/revision-dependent matrix cells were not searched again or reclassified. No existing source, voicing, test or build file was edited by this pass.
