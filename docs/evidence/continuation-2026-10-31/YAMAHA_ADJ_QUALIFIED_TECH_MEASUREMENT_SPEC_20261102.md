# Yamaha CS-80 M-board ADJ p11: qualified-technician evidence request

**Drafted 2026-11-02. Status: measurement specification only — no hardware
observations collected.** This is an evidence-acquisition brief for a
**qualified synthesizer service technician** working with the instrument
owner's authorization, appropriate isolation/ESD practices and a safe bench
setup. It is **not** a do-it-yourself live-work guide. An unqualified owner
should not open, energize, probe, short, load or adjust exposed circuit
boards on the basis of this document. The owner can request a technician's
report and decide whether to authorize inspection or calibration.

## Source and evidence class

| Document | What it establishes | Original-source identity |
| --- | --- | --- |
| Yamaha **CS-80 adjustment procedures**, printed p11, PDF page 13 | Exact HP/LP panel setup, DVM/scope points, VR7→VR8 and VR9→VR10 sequence, printed Vpp targets | [36-page Yamaha PDF](https://www.therogoffs.com/cs80/CS80%20Adjustment%20Procedures%20and%20updates.PDF), SHA-256 `eea101eb338494f3d1d3fddced2d8aa7f1d877ec9d6155ca44cc77ece88276ba` |
| Yamaha **M circuit diagram**, printed p41, KEC-4223-65 | FET6/FET7 waveform switching, four 150 kΩ mixer branches, HP/LP IG00156 pins, Vfc/Qadj trimmers, CP3-to-VCA continuation | [M41 scan](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/41%20-%20M%20Circuit%20Diagram.jpg), SHA-256 `1278f17ff05cd82b2b84153838bf95109d3f1ea1e864119de0a3f4a4af4f0080` |
| Yamaha **M11/M12 board and wiring**, printed p44, KEP-NA03574-6Y | Same-board CP2(46)↔CP7(53), CP8(56)↔CP3(29), and NI(43) to SUB NO1(39) harness identity | [M44 scan](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/44%20-%20M11-12%20Circuit%20Board%20%26%20Wiring.jpg), SHA-256 `3320dd2f625c79d7cca9b98abd5cade9d1b95477e6d8990a93a296c9cda71256` |
| Yamaha **SUB circuit diagram**, printed p29, KEC-4446-68 | NO1(39) at IC3/2 feedback-buffered noise-source output; no measured voltage or loaded output impedance | [SUB29 scan](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/29%20-%20SUB%20Circuit%20Diagram.jpg), SHA-256 `b81c7e5ad2927ac04d5912bfc455a631237f512a0c7700bda41250a14bd33e60` |
| Yamaha **CS-60 adjustments, M**, printed p38 | Independent cross-model identification of VR7 as HP cutoff and VR8 as HP Q; **not** a CS-80 CP3 voltage specification or a replacement procedure | [CS-60 p38 scan](https://therogoffs.com/cs80/manuals/CS60_Service_Manual/38%20-%20Adjustments%2C%20M.jpg), SHA-256 `3ed81ac23d92872060277cbd6f1534550f2d4f58590b2318f063e274d0506315` |
| Yamaha Service News **E-46**, January 1978, within the adjustment PDF pp33–36 | IG00153 VCO temperature-ranking/IS1555 diode countermeasure; **not** T-board preset resistor applicability | Same original 36-page Yamaha PDF; printed bulletin pp1–4 |

All nominal/model voltages below are **software predictions or printed
manufacturer instructions**, *not* measured voltages from an actual CS-80.
The inspected M41 drawing is labeled for CS-80 serials **1001 onward**;
inspection must record an actual instrument's board/serial/revision rather
than assume all installed boards are identical to the selected drawings.

## Preserve the exact manufacturer ADJ controls and order

The printed p11 procedure specifies the following sequence. This is an
**observation-request transcription** for a qualified technician, not a
directive to a non-technician to manipulate energized equipment.

| ADJ phase | Manufacturer panel/voltage condition | DVM and oscilloscope locations | Printed order and target |
| --- | --- | --- | --- |
| **HP step 1** | FEET **2′**; BRILLIANCE **centered**; master RESONANCE **maximum**; TONE SELECTOR **PANEL**; VCO **pulse ON**; VCF **RESH HIGH, LPF HIGH, RESL HIGH**; HPF set for **+4.0 V at FH** | DVM: **FH** on R51 (channel I) or R52 (channel II); scope: **CP3 on the active M board**; C1 held | **VR7 first**, finding **maximum amplitude at CP3**; **VR8 second**, setting **4.3 ± 1 Vpp at CP3** |
| **LP step 2** | Carry forward preceding settings, except VCF **HPF LOW, RESH LOW, RESL HIGH**; LPF set for **+3.3 V at FL** | DVM: **FL** on R51/R52; scope remains at **CP3**; C1 held | **VR9 first**, finding **maximum CP3 amplitude**; **VR10 second**, setting **2.2 ± 1 Vpp at CP3** |

The original table **does not** specify a NO1(39)/NI(43) voltage, noise
source amplitude, output impedance, source waveform Vpp, the original
VR7/VR8 potentiometer positions, or a quantitative potentiometer-to-gm/Q
transfer. It does not instruct that **CP2 replaces CP3**. The pulse's
3 Vpp 50%-duty C1 square used in the repository's offline fixture is an
**explicit model assumption, not a Yamaha p11 measurement requirement**.
The 25 preset tone-tab hypotheses are irrelevant to this particular
**PANEL**-selected alignment; do not substitute a preset setting.

## Node/test-point identity and interpretation

| Technician observation requested, if safely accessible | Source-grounded electrical identity | Interpretation boundary |
| --- | --- | --- |
| **CP2 (46)** and optionally **CP7 (53)** | M41 IC10 IG00156 HP output pin 15; M44 CP2↔CP7 harness, then LP series path | **HP-stage** voltage, *not* the Yamaha p11 acceptance point |
| **CP3 (29)** and optionally **CP8 (56)** | M41 IC11 IG00156 LP output pin 15 to CP8; M44 CP8↔CP3 harness | **LP-stage** output, the manufacturer's HP and LP **scope acceptance point** |
| Downstream **IC4 VCA +IN pin 3**, if a suitable safe observation exists | CP3 → **1 µF/50 V NP capacitor** → **100 kΩ series** → IG00151 IC4 +IN; separate **LP1(27)** 100 kΩ/0.01 µF control path to IC4 LI pin 2 | VCA input loading/impedance unquantified; **CP4(28) is later VCA output**, not CP3 |
| **NO1 (39)** at SUB and **NI (43)** on M boards | IC3/2 buffered SUB output, shared through M44 NI harness; M41 **another 150 kΩ series resistor** between NI and HP mixer | Noise may be nonzero. Static Vpp or visual wiring alone **cannot establish output impedance** under multi-board load |
| **IC10 Vfc pin 4** and **Qadj pin 6**, via qualified high-impedance/control-safe sensing | VR7 path to **Vfc pin 4**; VR8 path to **Qadj pin 6**, as drawn on M41 | Control pins are **not** themselves FH/CP2/CP3. Input loading may alter cutoff/Q; voltage is not an established gm/Q transfer law |
| FET6/FET7 branch, if independently characterized | SO pin 5/T2/FET6/150 kΩ and PO pin 7/T3/FET7/150 kΩ; fixed grounded 150 kΩ and NI-series 150 kΩ share mixer | On/off FET resistance and pulse width are unknown. PW(40) through 680 kΩ and PWM(41) through 100 kΩ go to WSC width-control paths, **not direct HP mixer shunts** |

## Requested observational snapshots

For the *same identified active voice and service setup*, the technician's
report should keep the raw captures, instrument configuration and any
restorer decisions separate from the source transcription. If a node cannot
be safely observed without altering the circuit, mark it **not acquired**;
do not infer zero voltage or an infinite/zero impedance from absence.

1. **As-found baseline:** document physical board/voice identity, trimmer
   position reference and whether any calibration was previously modified;
   capture CP2 and CP3 in the same timebase, plus FH DC voltage and the
   oscillator/noise settings *as found*. Record CP2/CP3 amplitudes,
   phase/timebase, harmonic shape and whether extra channels/probes change
   the waveform.
2. **After the Yamaha VR7 maximum-amplitude observation at CP3:** record
   the CP3 amplitude-vs-control trend and the CP2/CP3 paired waveforms at
   the observed maximum, with FH still at the documented +4.0 V. Preserve
   actual VR7 reference position and any safely obtained **Vfc pin 4** DC
   voltage/control ripple. **CP2 is diagnostic, not the maximization
   target**. A technician should distinguish a broad peak, multiple peaks
   and a boundary/limit rather than report a guessed continuous maximum.
3. **After the Yamaha VR8 resonance step:** record *both* CP2 and CP3
   waveforms, their ratio, any visible clipping/instability or source
   interaction, and any safely observed **Qadj pin 6** condition. State
   clearly whether the printed CP3 target was reached **in that actual
   instrument**. Do not convert VR8 angle or pin-6 voltage to a Q number
   without a validated electrical law.
4. **LP procedure separately:** under the printed LP step-2 controls,
   retain the same paired CP2/CP3 observations at the VR9 amplitude
   maximum and after VR10, with FL +3.3 V. Do not mix these states with
   HP step 1 in a single inferred response.
5. **Shared NO1/NI path:** if the qualified service plan permits safe
   observation, record NO1(39) at SUB and NI(43) at the active M board,
   using synchronized bandwidth-qualified RMS/time traces and, where
   feasible, spectral content. Identify how many M-board NI paths are
   connected and the exact source/noise controls. A noise waveform's
   *peak-to-peak* reading is capture-window dependent; report acquisition
   time and bandwidth instead of calling one value a source specification.
   Determining finite NO1 output impedance would require a **separately
   authorized, validated loading or equivalent-circuit experiment**, not
   a live short, arbitrary termination, improvised injected signal, or
   an inference from two high-impedance voltage probes.

Only the qualified technician and owner decide whether altering adjustments,
disconnecting a harness, or touching an exposed control node is safe and
authorized. The requested study does **not** call for any such mutation by
default. No assistant has hardware access or has performed these steps.

## Instrument/loading and reproducibility metadata

Every waveform/number should retain enough metadata to quantify
**measurement disturbance**, not just a screenshot of an automatic Vpp
readout:

- Instrument make/model, channel, calibration state, waveform bandwidth
  limit/filter, effective analog bandwidth, sample rate, acquisition length,
  vertical scale, DC/AC coupling, scope ground/reference and Vpp algorithm.
- Each probe's **input resistance and capacitance**, attenuation and
  bandwidth; high-impedance low-capacitance differential/buffered sensing
  where the qualified technician judges it necessary. Explicitly account
  for the fact that adding a second CP2/CP3/NI/control probe can load the
  interstage/filter or alter the nominal source network. No single minimum
  instrument bandwidth or maximum allowable loading is asserted by the
  manual excerpt; do **not** invent one or use a 50 Ω termination merely
  for convenience.
- Ambient and chassis temperature, elapsed warmup and supply observations
  if safely available, instrument serial number, channel, M-board ID and
  revision, component modifications and which voice is active on C1.
- Raw time-series/export (or unprocessed captures) for both test points,
  repeat runs showing within-session variation, time-window start/stop,
  noise RMS/spectral measurement method, and any clipped/saturated samples.
- Clearly distinguish the physical probe location from a schematic label;
  note if a probe was placed on CP8 instead of CP3 or CP7 instead of CP2,
  and whether the paired harness voltages actually agreed. Do not silently
  substitute CP4 for CP3.

Potentially hazardous powered-circuit practices, scope grounding decisions,
isolation, and safe fixture design remain the technician's responsibility.
Do not give an unqualified owner node-by-node live-probing instructions.

## Discriminating observations — questions, not pass/fail criteria

| Competing explanation | Observation that would help separate it | What remains unproven even if observed |
| --- | --- | --- |
| **Printed CP3 probe location vs CP2 restorer anecdote** | Simultaneous same-event CP2 and CP3 waveforms during *actual* VR7/VR8 sequence, with CP8↔CP3 and CP2↔CP7 identity noted | A coincidental CP2 4.3 Vpp does **not** prove a Yamaha erratum; original p11 specifies **CP3** |
| **Original fixture NI open vs low-impedance NO1** | NO1(39)/NI(43) synchronized noise voltage, connected-card count and independently justified finite source impedance if safely obtainable | NO1 may be active; neither noise amplitude nor output Z follows from the M41 150 kΩ resistor alone |
| **Frozen control adapter vs VR7 maximization** | CP3 response while the technician locates the actual VR7 maximum, paired with CP2 and Vfc pin 4 observations | Frequency/gm law and initial Q state are unknown; one peak cannot identify a universal calibration curve |
| **VR8 Q transfer/linear vs compression** | CP2/CP3 waveforms and distortion at the post-VR7 VR8 state, optionally pin-6 Qadj with characterized probe loading | No source-derived Q=9 capability; 11 Vpp CP2 synthetic linear-model result is **not** validated hardware parity |
| **LP interstage/CP3 downstream loading** | Relative CP2/CP3 waveform, CP8/CP3 agreement and probe-attached/detached comparison by a qualified technician | Actual IG00156 output impedance and IG00151 input impedance remain unidentified; changes could reflect probing |

The offline reference is intentionally kept as a **separate column**:
frozen model HP setup **CP2 4.126, CP3 1.692 Vpp (`MISS`)** and LP setup
**CP2 1.038, CP3 1.648 Vpp (within manufacturer band)**. An ideal silent
NO1 termination gives model HP **CP2 3.812, CP3 1.482 Vpp**; a
predeclared *linear surrogate* cutoff/Q scan can report **CP3≈4.1 Vpp**
only at hypothetical Q=9 with **CP2≈11 Vpp** and no validated
VR8-to-Q/large-signal law. None is an observed CS-80 voltage, a new
Yamaha target, or authority to relax the registered gate. Full source/model
caveats and timestamped preregistrations are in
[NATIVE_FILTER_EVIDENCE.md](NATIVE_FILTER_EVIDENCE.md) and the linked
`ADJ_*_PREREG_20261102.md` studies.

## Distinct tone-tab provenance request: 25 cells remain hypotheses

The PANEL service alignment above does not validate the preset resistor
networks. The original Yamaha S31–S38 native-pixel review lists **25**
disputed cells on **T51–T54** (including revised resistors, marked direct
diodes/bypasses and ambiguous struck-through symbols). The clear S36 board
print **`75K(39K)`** at the proposed T53 B14 footprint and nearby paired
values remain *candidate revision evidence*, not installed-value proof.

| Source-board pair | Disputed addresses needing installed-revision evidence | Count |
| --- | --- | ---: |
| S31/S32, T51 | A20, A25, C25, E22, E26, F22, F25, F26 | 8 |
| S33/S34, T52 | A26, B9, C22, E20 | 4 |
| S35/S36, T53 | A20, A25, B14, B20, C20, E23, E26, F22, F26 | 9 |
| S37/S38, T54 | A26, C22, C25, C26 | 4 |

Original Yamaha **Service News E-46, January 1978**, does give CS-80
serial applicability (1169 and later, plus listed earlier units), but
**only for IG00153 VCO temperature-rank/IS1555 diode stabilization**.
It must **never** be applied to T51–T54 revision triangles.

If the owner independently authorizes a **de-energized qualified board
inspection**, request the instrument serial, board identity/revision/date,
high-resolution component-side and solder-side photographs, and a component
inventory keyed by the exact 25 cell addresses in
[TONE_CELL_NATIVE_REVIEW.md](TONE_CELL_NATIVE_REVIEW.md). Continuity and
installed-value checks, where safe and independently warranted, belong to
the qualified technician and should record method, circuit isolation and
uncertainty. A manufacturer bulletin explicitly mapping a T-board revision
to serial ranges would be preferable to reverse inference. Until qualified
installation evidence exists, **all 25 remain hypotheses**. The source
manifest fingerprint **`e5f877a025fc52c6`**, frozen V1 numerics and
registered tolerances must not change.

## Handoff/authority boundary

The owner can supply provenance, approve an authorized professional service
visit, and choose whether to share the technician's signed report and raw
captures. **Only the qualified technician**, under the owner's consent and
their own safety practices, determines safe access, fixture design and
whether any calibration or connected-circuit intervention is warranted.
Neither a schematic inspection nor an offline model authorizes adjustment
of an actual instrument or a production software fix. Do not label any of
the requested observations as measured until the technician supplies them.
