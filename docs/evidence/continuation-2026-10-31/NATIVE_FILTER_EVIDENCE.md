# Native filter-source review — 2026-10-31

This is inspection of Yamaha documentation pixels, not hardware measurement.
The evidence was fetched with `circuit-inspection.yml` in
[run 41224106605](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41224106605).
The existing ADJ acceptance bands and V1 numerical model are unchanged.

## Source identity

| Source | Page and crop | Source SHA-256 / crop SHA-256 |
| --- | --- | --- |
| CS-80 M Circuit Diagram | Printed p41, drawing KEC-4223-65, serial 1001 onward; `m-filter-detail`, native box [1180,1080,3250,1850], 2070×770 | `1278f17ff05cd82b2b84153838bf95109d3f1ea1e864119de0a3f4a4af4f0080` / `5dfe074bc3868f22fe662cbe55d718dbb9479db0d05f36a4089a8aae445cbcc7` |
| CS-80 Adjustment Procedures and updates | Printed p11, PDF page 13; `cs80adjust_filter-page13-full-native`, 1391×1800 full-page PDF render | `eea101eb338494f3d1d3fddced2d8aa7f1d877ec9d6155ca44cc77ece88276ba` / `e784350d6a6ca086e40fc68444a6ac3433f026b0811ef4830eb6c89b5ce3af35` |

M41 is in [job 124604536319](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41224106605/job/124604536319).
ADJ is in [job 124604536342](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41224106605/job/124604536342).
The adjacent PDF pages 12 and 14 (printed pp10 and 12) were also rendered.
The workflow uses `pdftoppm -scale-to 1800`; the `full-native` identifier means
the complete rendered page was emitted without a further crop resize, not
that an embedded PDF scan was extracted at its original resolution. Printed
p10 identifies the M-board trimmers and CP3 probe location.

## ADJ p11 setup transcription

The HP row specifies feet 2′, brilliance centered, master resonance at maximum,
tone selector PANEL, VCO pulse ON, and VCF RESH, LPF and RESL HIGH. Set HPF for
**+4.0 V at FH** on R51 (channel I) or R52 (channel II). Connect the DVM to FH
and the oscilloscope to **CP3 on the M board**. Hold C1, adjust **VR7 for
maximum amplitude**, then **VR8 for 4.3 ± 1 Vpp**.

The LP row retains the preceding settings except HPF LOW, RESH LOW, RESL HIGH
and LPF set for **+3.3 V at FL**. The DVM moves to FL; the scope remains at
CP3. Hold C1, adjust **VR9 for maximum amplitude**, then **VR10 for
2.2 ± 1 Vpp**. These are setup and alignment instructions, not a standalone
fixed-cutoff gain specification of an isolated chip.

## Unresolved mismatch and bounded conclusions

The existing source-model fixture predicts approximately **1.69 Vpp** for the
HP setup, outside the unchanged **3.3–5.3 Vpp** band. It predicts a value in
the LP band. This remains a reported miss. No resistor, capacitor, Q law,
normalization or tolerance was adjusted to force the HP result into range.

### 2026-10-31 correction of the LP input reading

The first version of this review repeated the older ledger's interpretation
of **33 kΩ and 150 kΩ both shunting AI1 to ground**, and incorrectly claimed
the native crop confirmed it. Independent visual reinspection of the
hash-verified `m-filter-detail` crop establishes the opposite connection:
**CP7, terminal 53, connects through 150 kΩ to IC11 AI pin 1; a separate
33 kΩ resistor connects that AI/FB junction to ground**.

The decisive endpoint is the open terminal circle labelled `(53) CP7`,
approximately crop coordinate `(645, 700)`. Its vertical lead runs upward
through the 150 kΩ resistor to the AI1 junction near `(640, 423)`. It does
not terminate at a ground symbol. The neighbouring 33 kΩ branch does end
at its own ground symbol. This is M41, printed p41, drawing KEC-4223-65,
in the source/crop hashes and native box recorded above.

The claim that the two resistors form a passive 27.049 kΩ ground shunt is
withdrawn. The observed series connection agrees with the existing
`kIg00156LowPassNetwork { 150.0e3, 33.0e3, 1.0e-6 }`; no component value,
source code, numerical model or acceptance band changes with this factual
correction. The older conflicting reading in `IG00156_MODEL_EVIDENCE.md`
is historical evidence of the misinterpretation, not a remaining conflict
in the visible CP7-to-AI1 path. The complete switched input network, chip
laws, interstage loading and service alignment remain separate questions.

### 2026-11-01 correction of the HP switched-mixer reading

A second independent inspection of the same hash-verified M41 source resolves
the origin of the **fixed 150 kΩ mixer shunt**, which the older discussion
implicitly attributed to an inactive waveform branch. On printed p41,
drawing KEC-4223-65, **IC9 SO pin 5 -> T2 -> FET6 -> 150 kΩ series** and
**IC9 PO pin 7 -> T3 -> FET7 -> 150 kΩ series** are the two switched waveform
paths. Their resistor ends meet the common mixer node, which also has a
**separate, permanently connected 150 kΩ resistor to ground**. The common node
continues through the 1 µF NP input capacitor toward IC10 (HPF); it also has
other documented controls/connections. This topology is visible in the
5100×3300 original drawing at native box `[370,1950,1620,2900]` (1250×950):
the FET6 resistor and grounded shunt meet near original pixel `(695,2374)`,
and FET7's resistor joins the same horizontal node near `(890,2374)`. The
original source SHA-256 remains
`1278f17ff05cd82b2b84153838bf95109d3f1ea1e864119de0a3f4a4af4f0080`;
the unresized native-box PNG crop SHA-256 is
`11c63adf6a47c23d43eeed6fe5705dbee2a542c5494e3b53c27c10a141e3e23a`.

For **one ideal conducting waveform gate, the other gate open**, and with the
other attached sources/loads ignored, the active 150 kΩ series resistor and
the **fixed** 150 kΩ ground shunt have a 75 kΩ Thevenin resistance and halve
that active waveform's voltage at the mixer node. The off FET is **not**
postulated to become a grounded source. Thus the existing HP 75 kΩ/half-pulse
fixture has a conditionally corroborated *resistor topology*, but not a
source-verified effective drive or absolute voltage: FET impedances,
waveform-driver levels, noise/PW/PWM connections, the interstage/CP3 loading,
and the VR7/VR8 procedure have not been modeled end to end. In particular,
this correction neither explains the **1.692 Vpp HP miss** nor changes any
model value, audio normalization, acceptance band or test. This switched
WSC mixer is distinct from the **LP CP7 -> 150 kΩ series -> AI1, 33 kΩ AI1
shunt** endpoint correction above.

### 2026-11-02 four-branch NI trace (clarifies 2026-11-01 ideal case)

The **entire** HP mixer region, inspected again at enlarged original pixels,
has **four separate 150 kΩ branches** meeting at one mixer node:

1. IC9 `SO` (pin 5) -> T2 -> FET6 -> 150 kΩ -> mixer (saw, gated).
2. IC9 `PO` (pin 7) -> T3 -> FET7 -> 150 kΩ -> mixer (pulse, gated).
3. Mixer -> **fixed** 150 kΩ -> ground.
4. Mixer -> **additional** 150 kΩ -> `NI(43)` (separate external source).

The *fourth* resistor is immediately below the FET7/mixer junction in Yamaha
M41 printed p41 KEC-4223-65. It was overlooked in the 2026-11-01 simplified
two-resistor calculation: **NI is not a direct short to the mixer**. Original
5100×3300 native box `[620,2160,1000,2560]`, enlarged 5× for legibility,
shows the FET7 150 kΩ *above* the junction and distinct NI 150 kΩ *below*.
M41 source image: [Yamaha M circuit p41](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/41%20-%20M%20Circuit%20Diagram.jpg),
SHA-256 `1278f17ff05cd82b2b84153838bf95109d3f1ea1e864119de0a3f4a4af4f0080`;
enlarged detail PNG SHA-256
`849fc31f2f9255df59fa95b973b71f6f2aaef015cb02c77dc866af03e2e933f8`.

On [Yamaha M11/M12 wiring p44](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/44%20-%20M11-12%20Circuit%20Board%20%26%20Wiring.jpg)
(KEP-NA03574-6Y, SHA-256
`3320dd2f625c79d7cca9b98abd5cade9d1b95477e6d8990a93a296c9cda71256`),
`NI(43)` is wired to SUB `NO1(39)`. The [original SUB circuit p29](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/29%20-%20SUB%20Circuit%20Diagram.jpg)
(KEC-4446-68, SHA-256
`b81c7e5ad2927ac04d5912bfc455a631237f512a0c7700bda41250a14bd33e60`)
traces `NO1(39)` to a buffered output of IC3/2. Its **actual output waveform
and source impedance under ADJ p11 setup are unmeasured**. A noise source
cannot be treated as a short, ground, or silent just from its `NI` label.

Two other M41 terminal traces cross this region but do **not** join the HP
audio mixer node at the crossing: `PW(40)` via 680 kΩ and `PWM(41)` via
100 kΩ go to the waveform-shaping circuit, IC9 WSC width-control pins
`PWI`/`PWO` (pins 10/11), and can affect pulse timing **upstream**. They are
not additional direct 680 kΩ/100 kΩ audio mixer loads. The old 75 kΩ source,
half-pulse fixture assumes both the inactive FET and the **NI source are
open**; it remains an intentionally conditional, ideal-switch calculation.
If NO1 were an ideal zero-volt low-impedance source, its series 150 kΩ
would add a third equal conductance, yielding 50 kΩ and pulse gain 1/3.
Neither idealization is established as the actual hardware condition. See
the [independently pre-registered offline comparison](ADJ_NOISE_LOAD_PREREG_20261102.md).
This is a further **HP input** reading, not the distinct resolved
**LP CP7 -> 150 kΩ -> AI1, with 33 kΩ shunt** reading. No documented service
test has been fixed by tracing the NI branch.

### 2026-11-01 ADJ probe and trimmer cross-check (still unresolved)

The original CS-80 **M11/M12 Circuit Board & Wiring** sheet, printed p44,
drawing KEP-NA03574-6Y, explicitly connects **CP2 (46) to CP7 (53)** and,
*separately*, **CP8 (56) to CP3 (29)** on each same-numbered M board. The
CP2/CP7 pair carries the HP-to-LP interstage connection through the already
verified LP series resistor. The CP8/CP3 pair exposes the later LP-side
signal. These are wires, not extra gain stages. M44 source:
`https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/44%20-%20M11-12%20Circuit%20Board%20%26%20Wiring.jpg`,
SHA-256 `3320dd2f625c79d7cca9b98abd5cade9d1b95477e6d8990a93a296c9cda71256`.
Other voice-board revision sheets require their own checks.

M41 places **VR7**, via a 150 kΩ resistor, at IC10 **Vfc pin 4** and
**VR8** at IC10 **Qadj pin 6**; the analogous IC11 adjustments are VR9/VR10.
An independent manufacturer sheet, **Yamaha CS-60 Service Manual printed
p38**, expressly assigns VR7 to HP cutoff and VR8 to HP Q, consistent with
those M41 connections. The CS-60 sheet does *not* define the CS-80 p11
4.3 Vpp specification or authorize changing it. Source:
`https://therogoffs.com/cs80/manuals/CS60_Service_Manual/38%20-%20Adjustments%2C%20M.jpg`,
SHA-256 `3ed81ac23d92872060277cbd6f1534550f2d4f58590b2318f063e274d0506315`.

A [2016 first-person CS-80 calibration account](https://mfoxhd.blogspot.com/2016/01/calibration-3-m-boards.html)
reports that its author could not reach the printed HP **4.3 Vpp at CP3**
setting, proposed **CP2** as an HP-only probe, and described **CP8/CP3** as
the same LP-side bus. The author reports substituting 7.6 Vpp at CP2 using
a claimed CS-60 adjustment precedent. This is **unreplicated field testimony**:
the account does not supply controlled waveform captures, a matched serial
number/revision qualification, or the cited CS-60 *adjustment* source. It
must not be promoted to a new CS-80 measurement, a specification erratum,
or permission to replace the registered **3.3–5.3 Vpp at CP3** acceptance
band. The ADJ fixture still reads **1.692 Vpp** at its LP-stage endpoint.
With *unchanged* physical component values, source drive and frozen control
settings, an added **model-only** readout in `tests/Ig00156NodalTests.cpp`
evaluates both ideal stage taps. The HP service setup predicts **4.126 Vpp
at CP2 (HP stage)** versus **1.692 Vpp at CP3/CP8 (LP stage)**. The LP
setup predicts **1.038 Vpp at CP2** and **1.648 Vpp at CP3/CP8**. Thus the
modeled HP-only CP2 output happens to be within the printed **3.3–5.3 Vpp**
band specified for *CP3*, but the actual registered CP3 test **still fails**.
This correspondence is a discriminating lead, **not** an independent
validation of a corrected service probe, a 4.126 Vpp hardware reading, or
license to choose a terminal for a good result. The reason for the discrepancy
remains unproven, including whether the documented VR7 cutoff maximization
followed by VR8 Q adjustment, the WSC source, or actual CP3 loading accounts
for it.

The independent tasks before calling the HP band closed are to complete the
HP mixer/source-impedance graph, verify the full interstage loading graph
from M41 and harness evidence, and reproduce the documented VR7/VR9
maximum-amplitude alignment operation. Only then should the unchanged bands
be evaluated again. These are source/model uncertainties, distinct from the
top-octave numerical frequency-warp error.
