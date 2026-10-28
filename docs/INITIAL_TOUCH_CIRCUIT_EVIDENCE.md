# Initial-touch circuit evidence and destination separation

Status: destination separation established from TWS/TRG schematics; electrical calibration remains incomplete. These are schematic observations, not measurements on a physical CS-80. Do not replace the engine's initial-touch routing with one shared decay merely because the owner's description uses the word pulse.

## Evidence provenance

Original Yamaha CS-80 service manual scans, marked S/#1001 onward, are indexed at:
https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/index.html

The bounded Actions inspection produces source URL, SHA-256, dimensions, crop coordinates and PNG hashes in job logs. No downloaded manual images are committed to this repository.

- Run 40782485920: TKC page9, TSB1 page11, TSB2 page13, all inspected successfully.
- Run 40783878519: SH page7, TWS page17, M page41, Panel1 page62, all inspected successfully.
- Run 40784517382: native TWS/M details and TRG pages19,21,23,25, all inspected successfully.
- These runs are source-retrieval evidence only, not DSP tests or hardware-validation evidence.

Source SHA-256 values:
- TKC9: d71b598a895d6320276a3af82df152c4c407aa9cb44b6e72c316028bccd0bb10
- TSB1p11: e48ed45acce48378e03e2d78bd12bdbac2a9ffb3af5ad3dda0e301f773fb0378
- TSB2p13: 69b7944f79db1db16c2a754c5ec22bbc336ee8eb2fb8a025f091dd7a2bdad0c6
- TWS17: 03d1f071db02d3401b35808af55b9253dcde04d378f5af0e3727436ef51ce890
- SH7: 99ac2cbfe0551bbd0d536f94ab6e92bbf924e5bf21b6c646c07b17459495de0c
- M41: 1278f17ff05cd82b2b84153838bf95109d3f1ea1e864119de0a3f4a4af4f0080
- Panel1p62: da50d3e9c77b63b30e8077799d65f03a3416b567dadb16e5bf0c692ac9a80708

## Directly visible TSB detector components

The repeated DET cell on both TSB pages contains two 1S1555 diodes from XX1 and XX2 toward a common output node. At that output, a capacitor marked 0.047 and a resistor marked 1M connect to E. Interpreting the capacitor in the manual's microfarad convention gives the isolated passive discharge product 0.047e-6 F * 1e6 ohm = 0.047 s.

XX2 also has a capacitor marked 1/25 to E. XX3 has a 22k resistor in parallel with a capacitor marked 0.33/35 to E. Its isolated product is 0.33e-6 F * 22e3 ohm = 0.00726 s.

These products are NOT established audio-envelope constants. The two diodes, their source impedances and conduction states, switched multiplexing and following circuits affect the response. XX1/XX2/XX3 physical sensor identities must be traced rather than inferred from their names.

TSB output multiplexers feed buffered B outputs. TKC9 shows switch-routing and logic that dispatches these signals to the eight output circuits.

## TWS establishes distinct processing paths

Page17 contains eight repeated channels, with inputs numbered1–8 and separate output families a1–a8, b1–b8 and C1–C8. Each channel also has a T trigger input.

Visible first-channel components include:
- D1 and an input hold node with 1M and0.022 to E.
- An amplifier stage with 6.8k feedback and 10k to E (native crop corrects the overview misreading of 100k): ideal gain 1.68.
- The a1 branch uses100k series resistance and1/16 capacitance followed by a voltage follower: nominal isolated100ms low-pass product.
- The b1 branch includes an AC-coupling capacitor marked 1/16, 22k to E, FET1, a 0.33/16 hold capacitor and a unity-gain buffer. The native crop suggests 10M for the high-value resistor, but the handwritten label is not sufficiently secure to rule out the overview's apparent 100M; independent source confirmation is required before a numerical component model. Crucially it connects the held node to the FET gate, NOT directly to E; neither a 33s nor a 3.3s passive decay-to-zero follows from it. The gate also has 1M to the input-side node and a diode connection to the trigger amplifier.
- The T1 trigger path contains0.01 coupling,1M,470k feedback,47k on the other amplifier input and a diode-controlled FET gate.
- The C1 branch is driven by buffered b1 through 1/16 coupling, with 100k shunt to E, 100k input and 100k feedback, plus a diode clamp and 47k at the other amplifier input. With an ideal opamp and clamp nonconducting, the capacitor sees 100k || 100k = 50k, giving a nominal 50ms high-pass pole and high-frequency inverting gain -1. This is a conditional schematic prediction, not a measured pitchbend time or universal touch envelope. A positive b step produces a negative C transient; the clamp changes the opposite-polarity response.

## Direct TRG destination evidence

Page19, TRG1-2: b1 at terminal6 feeds IC1, IC9 and IC17; their outputs are B11 (terminal7), B21 (38), and L11 (39). The respective common controls are IBI (33), IBII (66), ILI (65). Repeated channels b2-b8 have the same topology.

Page23, TRG4: b1 (terminal5) feeds IC1 to L21 (6), controlled by ILII (4). C1 (40) feeds IC9 to P1 (41), controlled by PB (39). The same distinction repeats through voice8.

Thus the initial-brilliance and initial-level VCA banks receive b, whereas the pitchbend bank receives the separately AC-coupled C. A common short decay for all these destinations contradicts the visible topology. Keeping a held initial magnitude for level/brilliance is qualitatively consistent, although the exact acquisition, leakage, gain and MIDI calibration are not established.

Page21, TRG3: a1-a8 feed banks controlled by VCO, VCF and SUB, with O/F outputs. This supports the smoothed ongoing-pressure role of a; full harness/control scaling still requires tracing.

## Owner description versus circuit evidence

The original owner manual describes contact timing as creating an initial-touch pulse and discusses its effects on level, brilliance and pitch. This is a functional explanation, not a complete circuit specification. It does not establish that all destination voltages decay with one identical constant.

A previous proposed interpretation, that the level/brilliance paths must necessarily use the same short decay as pitchbend, is withdrawn: TRG destination tracing now establishes separate b and C receiver families. Do not impose the pitchbend decay on level/brilliance.

The owner's explicit zero-touch-control behavior remains a separate basis for testing neutral velocity response. Removing unconditional velocity scaling when all touch controls are zero does not require guessing any circuit pole.

## SH state retention observation

Page7 routes each KI input through a bilateral switch into a hold buffer K. Its switch control is a NAND of the voice TR signal and common SUS. SUS has a22k pull-up to+8.5V. This directly shows that pitch tracking/holding depends on both trigger and sustain control. It does not establish the panel-to-SUS polarity or justify unconditional pitch freezing on every key release.

## Implementation gate

Before changing initial-touch time laws:
1. Preserve the established b versus C destination separation; complete overall wiring and panel scaling for every receiver.
2. Retain native-resolution component readings above and verify FET/opamp identities before numerical switched-circuit modeling.
3. Derive the switched/FET/diode state sequence, including trigger polarity and acquisition window.
4. Keep isolated passive RC products separate from the complete driven response.
5. Model receiving-stage gain, offsets, clamps and loading before assigning MIDI velocity to electrical units.
6. Add regressions for destination independence, zero-depth neutrality, retrigger, held notes, release and staggered polyphony.
7. Mark nominal schematic predictions separately from measured calibration and instrument tolerances.

No measured audio time constant is certified by this document. The 50ms C-path pole is a conditional ideal-circuit derivation, not a claim of full hardware equivalence.

- trg12 source SHA-256: 649c906dd2069de60e1fffb44bf82a4e64a4f7dc9bb7a7bee528db644041e9b1

- trg3 source SHA-256: eae097fbeba5e911b5d6f87d874f236ffe34ca16e65063e15d757c0aadc368d4

- trg4 source SHA-256: 8c3d8ffab912fc7e9b3817da880ded20f8fcc177a5927d52e5c68ff78a511b26

- trg5 source SHA-256: 29deffa0cfc883bf2bb3f95f2887f5e1df7f99f4ae96efb0c9d8c4693afe6b92
