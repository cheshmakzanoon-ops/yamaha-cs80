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

The independent tasks before calling the HP band closed are to complete the
HP mixer/source-impedance graph, verify the full interstage loading graph
from M41 and harness evidence, and reproduce the documented VR7/VR9
maximum-amplitude alignment operation. Only then should the unchanged bands
be evaluated again. These are source/model uncertainties, distinct from the
top-octave numerical frequency-warp error.
