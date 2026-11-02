# Yamaha M41 IG00151 coupling placement versus the one-stage software VCA

2026-11-02. **Primary-sheet topology comparison only; no physical
measurement and no production DSP change.** This review follows the
click team's source-path attribution to a modeled VCF→VCA note-edge event,
but a software causal localization is **not** proof that an actual
IG00151 stage has the same envelope, offset, or transient law.

## Independently checked original pixels and pin references

| Original Yamaha source | What is verified | Reproducible identity |
| --- | --- | --- |
| [CS-80 M circuit diagram, printed p41](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/41%20-%20M%20Circuit%20Diagram.jpg), KEC-4223-65 | Complete CP3→IC4, CP4→IC5, CP5→IC6→Tr8→O path and all cited 1 µF/100 kΩ parts | Original JPEG 5100×3300, SHA-256 `1278f17ff05cd82b2b84153838bf95109d3f1ea1e864119de0a3f4a4af4f0080`. Native crops: `[2250,370,3750,1320]` SHA `247becd8e3d9b9018198aefde625dfa33825d51d647b4178fd61c373a9d88d4e`; `[2050,900,3750,2150]` SHA `4d69663cc0a9160583dea6fa9f73933dda9db2b0303150948493c902157af5d4`; `[2300,600,3700,2800]` SHA `72288653e44f575190faf3275adefe4f49216da6bfcc5d40396e57623d9d3adf` |
| [CS-80 M-circuit ICs, printed p52](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/52%20-%20M%20Circuit%20ICs.jpg) | IG00151 EI **pin 1** exponential, LI **pin 2** linear, signal +IN **pin 3**, OUT **pin 8**; the description is a functional diagram, not a quantified transient transfer or input impedance | Original JPEG 5100×3300, SHA-256 `387d44e9f891c8af9bcef83e3894077df4f198d1676d4580041c828ba1c7fb51` |
| [CS-80 M11/M12 board wiring, printed p44](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/44%20-%20M11-12%20Circuit%20Board%20%26%20Wiring.jpg) | CP3(29) ↔ CP8(56) on examined board pair; local CP4(28)/CP5(32); reciprocal **O(33)** routing to other voice positions | Original JPEG SHA-256 `3320dd2f625c79d7cca9b98abd5cade9d1b95477e6d8990a93a296c9cda71256` |
| [Panel 2 printed p64](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/64%20-%20Panel%202%20Circuit%20Diagram.jpg), PRA routing and overall foldout | Detailed cross-board [signal routing](../../OE1_SIGNAL_ROUTING_EVIDENCE.md): M18/M28 **O(33)** feed rank MIX controls → PRA MI/MII; eventual output jack AC coupling is **separate** from the M-board stage | Panel 2 JPEG SHA-256 `057550f85d4c3622c95da14bbcaa6d9bec8aced9addfdf00eb3498553053de4a`; foldout SHA `23de2fc4ef4bbbc6c171979aa586a2f1c1fb11d7f6116656153a752b31709b38` |

## Trace only what is actually drawn

| Stage / node | Audio path source pixels (M41) | Control and coupling boundary |
| --- | --- | --- |
| **IC3** IG00151 | **CP1(21)** via 100 kΩ directly to **+IN3**; unlike CP3→IC4, no 1 µF capacitor is drawn in this branch; **OUT8** reaches CP4 sum | **LP2(24)**→100 kΩ→**LI2**, 0.056 µF shunt; EI1 from VR11 trim via 100 kΩ. |
| **IC4** IG00151 | **CP3(29)**→**1 µF/50V NP**→**100 kΩ**→**+IN3**, then OUT8 to **CP4(28)** | **LP1(27)**→100 kΩ→**LI2**, 0.01 µF shunt; EI1 from VR12 trim via 100 kΩ. |
| **CP4** | **IC3 OUT8 + IC4 OUT8** join at CP4, with **27 kΩ to reference** | The same summed node drives **another 1 µF/50V NP +100 kΩ** series path into **IC5 +IN3**. There is **no separate 1 µF capacitor between IC4 OUT8 and CP4 itself**. |
| **IC5** IG00151 | Receives **CP4** through that *second* 1 µF/100 kΩ pair; **OUT8** reaches **CP5(32)**, another **27 kΩ to reference** | **LC(31)**→100 kΩ→**LI2**, 0.01 µF shunt; **EI1** from VR13 trim via 100 kΩ. Yamaha prints **“VCA (for SUB-OSC)”** next to IC5, but its drawn **+IN3 is electrically in series with CP4 main audio**; the label alone does not define an isolated SUB-only input or a verified SUB→EI1 modulation law. |
| **IC6 + Tr8** | **CP5**→100 kΩ→IC6 **+IN3**; IC6 **OUT8**→Tr8 buffer→**4.7 µF/50V NP**→1 kΩ→audio **O(33)**. The shunt and feedback network are drawn separately. | IC6 has its own trim/feedback paths; the 4.7 µF output capacitor's effective low-frequency pole cannot be computed from its capacitance and the 1 kΩ alone without the actual receiver/loading. |
| **Later output system** | O(33) to panel rank MIX/PRA then output processing and preamps | The overall output schematic also depicts **10 µF NP** output-jack coupling. This is a distinct stage and does not establish a voice-VCA 0.1 s pole. |

This establishes **two physically separate 1 µF/100 kΩ interstage
audio paths**: (i) pre-IC4 and (ii) **post-CP4, pre-IC5**. It also
establishes the separate 4.7 µF O(33) output coupling. A nominal
**1 µF × 100 kΩ = 0.1 s** product is just the series-element
product; a true **loaded pole/step response** also depends on the
unknown IG00151 input/output impedances, the 27 kΩ CP4/CP5 shunts,
and the actual voltage/current/OTA law. Do not call either stage an
independently measured 0.1-second high-pass.

## What the shipped software actually computes

At the currently reviewed `src/core/Engine.cpp` circuit-engine rank
boundary, `vcaInput = filtered * rs.vcfLevel + sine` is submitted once
per rank to `r.vca.process(vcaInput, levelCv, subValue * global.subVca *
kIg00151SubOctaves, amp)`. The one-stage
`src/core/Vca.h:Ig00151Vca` computes:

```text
g(t)     = max(0, one_pole_LI(LC)) * envelope(t) * 2^EI(t)
out(t)   = g(t) * (vcaInput(t) + inferred_offset) + inferred_feedthrough*g(t)
coupling = one_pole_0.1s(out)
result   = out - coupling
```

The `Vca.h` comment explicitly classes the **0.1 s post-output
coupling** as **inferred “next-stage AC coupling”, by analogy with
IC5's documented input**. It is **not** a source-transcribed,
all-stages circuit equation; its input offset, feedthrough, SUB→EI
mapping and LI driver impedance are likewise labeled inferred in
[IG00151_VCA_MODEL.md](../../IG00151_VCA_MODEL.md). M41 indeed
provides a **post-IC3/IC4 1 µF/100 kΩ interstage capacitor** into IC5,
so it would be wrong to assert that any after-VCA capacitor is fictitious.
But the physical sequence contains **multiple VCAs and capacitors**,
whereas the software applies one post-**aggregate-gain** high-pass.

The printed **“VCA (for SUB-OSC)”** wording at IC5 is real, but
M41 directly shows **CP4→IC5 +IN3** and **LC31→IC5 LI2**. The schematic
does **not** show a direct SUB waveform at IC5 +IN3; calling IC5 a
disconnected SUB-only branch would contradict the printed wire. On
the other hand, those connections do not establish the complete
SUB modulation or EG distribution through IC3/4/5/6. Neither
the software's one-stage collapse nor its `2^EI` control law can be
declared hardware-correct or hardware-wrong from these sheets alone.

## Falsifiable test-only *placement sensitivity*, not a proposed fix

**Predeclare before running any numeric differential.** The smallest
non-fitted comparison takes the *same* frozen `vcaInput` samples,
`g(t)` samples, sample rate, note-event schedule, and initial
capacitor state on both sides. For only an explicitly opted-in,
diagnostic **signal branch** (zero inferred offset/feedthrough in
both test arms), compare these two endpoint placements:

| Synthetic test arm | Exact operator order | What differs |
| --- | --- | --- |
| **POST** (current placement analogue) | `HP_τ[g(t) · vcaInput(t)]` | A coupling operator after all the collapsed gain. |
| **PRE** (opposite placement bound) | `g(t) · HP_τ[vcaInput(t)]` | The *same* coupling operator before that gain. |

Here `HP_τ` is the **already frozen 0.1 s software one-pole** with
identical discrete recurrence in both arms. This **is not an assertion**
that CP3→IC4, CP4→IC5, IC6→Tr8 or actual O(33) has 0.1 s pole.
The actual circuit contains an *intermediate* capacitor between gain
stages. A real split would need unknown **separate gains and impedances**
for IC3/4 and IC5/6, so neither PRE nor POST is declared the Yamaha
target. Do not add a newly chosen `τ`, saturation curve, arbitrary
fade or source-amplitude normalization to improve a click score.

Predeclare the **exact existing-state/retrigger event fixtures**, including
the click team's Bass 48/96 kHz and Funky 44.1/48/96 kHz cases, and
the same 8 ms event window and smooth-tone negative controls that were
used before the new hypothesis. Preserve:

1. A separately rerun **byte-identical shipped baseline**, with its
   existing exact-state/V1 gates untouched.
2. The full **unselected PRE and POST** derivative/window-energy and
   sustained-note differences, with identical input and control samples,
   regardless of which happens to lower a click score.
3. Stable-gain/steady-signal controls where operator ordering should
   approach the same stationary behavior; explicit note-edge gain
   discontinuity controls; no new pass threshold, no post-hoc event choice.
4. An explicit **inconclusive** result if neither placement changes the
   click residual, or if smooth tones degrade.

A large PRE-vs-POST event transient difference would establish only
**software sensitivity to the location of a memory element relative to
changing gain**. It would **not** prove which analog IG00151 control
node changes at that event, whether the physical capacitor sees the
modeled source resistance, or that a lower residual sounds correct.
The click workstream owns any separate preregistration and opt-in
test harness. This source-review workstream makes **no code edits,
new acceptance tolerances, V1 changes, or hardware claims**.

## What remains independently observable, if a technician were authorized

Hardware discrimination would require independently safe,
same-event physical observations at **CP3 (pre-IC4), CP4 (IC3+IC4
sum), CP5 (IC5 output), and O(33) (post-buffer coupling)**, alongside
LP1, LC31 and relevant EG controls with complete probe R/C/bandwidth,
board revision and timebase provenance. No such captures exist here.
Loading those nodes may itself change the circuit; no live-work
method or probe/termination choice is prescribed to an owner.

The frozen **HP ADJ CP3=1.692 Vpp MISS**, manufacturer p11
**4.3 ± 1 Vpp** acceptance, 25 unresolved tone-cell revisions,
source-factory fingerprint `e5f877a025fc52c6`, production
numerics and all registered audio gates remain unchanged.
