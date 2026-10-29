# Keyboard contour circuit evidence

Research date: 2026-10-28. This is a source-derived investigation, not a completed implementation or a claim of measured hardware equivalence.

## Scope and source chain

The original four global controls are LOW/HIGH BRILLIANCE and LOW/HIGH LEVEL. Owner-manual printed pp9,38,45 describe center-neutral, bipolar adjustment whose effect grows toward the keyboard ends, not a hard split. Brilliance changes both filters; level changes amplitude. The note-position control is generated digitally as a DC quantity. This does not establish a linear-in-semitones law, an exact center note, or equal HIGH/LOW gains.

Primary Yamaha service drawings, mirrored by David Rogoff:
- [KBC1–2 p15](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/15%20-%20KBC%201-2%20Circuit%20Diagram.jpg)
- [R51–52 p39](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/39%20-%20R51-52%20Circuit%20Diagram.jpg)
- [M p41](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/41%20-%20M%20Circuit%20Diagram.jpg)
- [Owner scan part3](https://www.therogoffs.com/cs80/CS80E_3.pdf), [searchable owner scan](https://device.report/m/08eee3fd014ab14d5f15fe50a150900b9c3ed22ad75895b5736f46b2e186f9b0).

Actual source pixels inspected through authorized GitHub Actions run40788149601, source revision d4969ff8454083d3aa6e122015b3a3f444fc2952:
- KBC job123166413041; source SHA256 570762503245163d83850eded3defe9c327db24571ebe7bafae56f05f18e0fdf.
- R51–52 job123166413018; SHA256 b77875e1aee0e092c320f8dea69ec7b9d975d4b3551eaa2cdabc597886c80156.
- All four overlapping quarter crops inspected. Each source is5100×3300; quarter output is downsampled. Native focused crops are required where a junction, resistor or symbol is ambiguous.
- M41 job123154243112 in run40784517382, including native filter-detail crop.
- Panel1p62 and Panel3p66 were inspected and ruled out as the four contour-pot drawing: p62 is the programmable ranks; p66 is the side performance/foot/chorus controls. This correction prevents false source attribution.

## Verified receiving networks

R51–52 p39 carries one KC input per voice. Each KC node feeds the corresponding FH (high-pass cutoff) bus through120k and FL (low-pass cutoff) bus through39k. IB, AB, TF and shared SUB have their own parallel source branches. Shared overall BR uses680k to FH and220k to FL. These are resistor values, NOT standalone dimensionless gains.

The level contour uses a separate per-voice KB source branch through270k to LC. Initial-level TI and after-level TA also enter LC through their own270k branches; AM and LV are separate shared sources. Do not implement keyboard level as an unrelated final master gain.

M41 f1/f2 have rank-dependent grounded R6/R7,0.01uF shunt capacitors,150k trim-feed branches and filter-envelope branches. IG00156 input impedance and complete inter-board connections remain part of the load. Therefore neither120k/39k nor the R6/R7 values alone determines cutoff displacement. The IC's functional voltage law must not be applied directly to a physical loaded pin without establishing its voltage convention.

## Panel2 control mapping, independently verified

[Panel2 p64](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/64%20-%20Panel%202%20Circuit%20Diagram.jpg) was inspected in run40788842580 job123168711867 at source revisionf452586383af06db0b0d8c2284050a0cafdd3f18. VR19 is LOW BRILLIANCE, VR20 HIGH BRILLIANCE, VR21 LOW LEVEL, VR22 HIGH LEVEL. All are marked B10k. Their wipers go directly to KBC1 LI/HI and KBC2 LI/HI respectively. The center taps connect to the ground net and the endpoints to the bipolar supply wiring. Physical lever orientation still requires photo/terminal reconciliation.

**Correction:** the inherited assertion that LOW has a−1 buffer and HIGH a−15/56 buffer is not supported by the inspected KBC schematic. Both IC14 control inputs have56k input and15k feedback. Do not publish the inherited assertion as a source fact. BA27 was also inspected and does not contain these contour input buffers.

### Nominal centered-pot loading (conditional derivation)

Let x be electrical wiper travel from grounded center toward the positive or negative rail, normalized−1..1. For an ideal linear10k track with a true center tap, each half is5k. Its open-circuit voltage is15x and source resistance Rth=5000*abs(x)*(1−abs(x)) ohms. The56k IC14 input terminates at virtual ground, so Vw=15x*56000/(56000+Rth), before bias/input-current corrections. At abs(x)=0.5 the loading ratio is56000/57250=0.97816594. At center and rail endpoints Rth=0 in this nominal model.

Combining with the stage equation below gives nominal reference range0..8.035714V, with neutral4.017857V. These are reference-node bounds, NOT output gain or filter-octave bounds. They require linear B-taper interpretation, ideal rails, verified center contact and unsaturated opamp operation. Physical upward/downward lever polarity is not implied by the definition of x.

## KBC source observations

KBC uses YM26700, receiving note-code inputs and octave-code inputs. The drawing includes shift registers, flip-flops, decoding gates, analog gates, an octave resistor ladder, a note resistor ladder and eight channel outputs. It is not an analog audio-frequency follower.

The printed nominal octave-tap table is OCT0..5 =0.25,0.5,1,2,4,4V under the specified4V reference. The printed note-tap table under OO=4V is C#2.119,D2.245,D#2.378,E2.520,F2.670,F#2.828,G2.997,G#3.175,A3.364,A#3.564,B3.775,C4V. These reference tables are not the finished bipolar contour curve: switched ladder references and HI/LI inputs must be solved.

The HI and LI terminals enter IC14 voltage-slider summing stages through56k branches with15k feedback. Both also receive−15V through56k. Under ideal-opamp assumptions, each reference is therefore Vref =225/56 − (15/56)*Vin =4.017857142857−0.267857142857*Vin volts. This is a source-derived nominal stage equation, not the final contour law.

Native KBC crops in run40788842580 job123168711890 (kbc-decoder, kbc-ladder, kbc-output) independently verify the eight detection channels. Each channel has a0.033uF held node, a voltage follower and a summing inverter with15k signal input,15k feedback (explicit on repeated channels),56k bias from−15V and grounded noninverting input through6.8k. Thus its ideal settled output is225/56−Vsample. Neutral cancellation follows if the switched network supplies225/56; proving that network behavior is a separate obligation.

The note ladder contains eleven1k segments between C# and C taps, with analog switches at both ends. Those ends can select the OO buffer, reference and trim-fed branches (22k plusVR1 B5k and20k plusVR2 B5k). The octave ladder contains unequal1.5k/3k segments. Consequently the printed generic exponential key-voltage tables cannot simply be reused as contour output values. A digital truth table and analog nodal solution are needed.

The KBC POR and PC pins are shown tied through22k to−6.5V on this board: this is evidence against importing active pitch-glide behavior from a generic YM26700 description without considering the actual connection. Final physical note-code mapping remains unresolved.

## Verified decoder wiring; conditional static network

Three readers independently checked the native KBC rail crop in run40795174429, job123189790589, marker `kbc-decoder-rails`, source box [550,1400,1400,1700]. The earlier fifth-gate ambiguity is resolved: its junctions are on rails1,4,5, so IC6 out10 selects !Q1*Q2*!Q3, not Q3. The fourth gate also selects !Q3. Local junction coordinates are (622,105),(645,214),(667,249) for the fifth group; the Q3 rail is below at y283. Switch-control wiring was independently rechecked without changing the table. This verifies the traced gate network, not its mapping to all61 physical keys. Define Q1=IC4 pin1, Q2=IC4 pin13, Q3=IC5 pin1; these are delayed/latched input octave bits, not yet mapped to physical octave numbers. Six rails are !Q1,Q1,!Q2,Q2,!Q3,Q3. Left-to-right decoded outputs are:
- IC7 out9: !Q1*!Q2*Q3
- IC6 out9: !Q1*!Q2*!Q3
- IC7 out6: Q1*!Q2*Q3
- IC6 out6: Q1*Q2*!Q3
- IC6 out10: !Q1*Q2*!Q3
- IC7 out10: !Q2*Q3, with two inputs tied.

Continuity across the native ladder and overlapping lower-left quarter indicates HI/IC14 pin1 drives OC5, LI/IC14 pin7 drives OC0 and OC1, and IC13 reference R=225/56 drives OC3. Native kbc-reference-routing in run40789983782 job123172514658 corroborates the reference paths. With L=IC14 LI output and H=IC14 HI output, ideal unloaded octave taps are L,L,(2L+R)/3,R,(R+2H)/3,H. This follows the1.5k,3k,3k,1.5k ladder segments; it is not the generic printed exponential pitch table.

Let O be buffered YM26700 OO output selected from those taps. Let m=0..11 correspond to the drawing's C#..C taps. Assuming active-high ideal analog switches and settled unbiased nodes, the endpoint-switch solution is:

|Q1Q2Q3|Top C endpoint|Bottom C# endpoint|Sampled voltage V(m)|
|---|---|---|---|
|000|O|open|O|
|001|O|R|R+(m/11)*(O−R)|
|010|R|O|O+(m/11)*(R−O)|
|011|open|O|O|
|100|open|O|O|
|101|O|R through20k+VR2|O−(11−m)*1000*(O−R)/(31000+VR2)|
|110|R through22k+VR1|O|O+m*1000*(R−O)/(33000+VR1)|
|111|open|O|O|

VR1/VR2 are0..5000ohm rheostats as drawn. The final detector output is R−V(m). The open-end rows are settled ideal voltages, not a claim of instantaneous switching or zero capacitive memory. The remaining critical fact is a consistent mapping of physical key code to Q and selected OC tap. KBC16 now identifies the actual parts below; active-high switch behavior must be checked against the manufacturer's matching TC4016 documentation. Without these, this table is a network derivation, not yet an executable keyboard contour lookup. Do not use numerical-code plausibility to substitute for the missing primary code table.

## Latest source corroboration and unresolved code convention

Source revisiond6b2bc5129f46bf0dbae598a9ec8c89bd109709f, run40789983782:
- KAS06 job123172514389.
- KBC16 job123172514694.
- KBC native reference-routing job123172514658.
- Panel2 native contour-controls job123172514604.

[KBC wiring p16](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/16%20-%20KBC%201-2%20Circuit%20Board%20%26%20Wiring.jpg) identifies IC1/2 CD4006AE, IC3 TC4019P, IC4/5 TC4013P, IC6/7 TC4073P, IC8 TC4069P, IC9/10 TC4016P, IC11 YM26700, IC12 LM310, IC13–22 NJM4558. It specifies22k resistors±5% and other resistors±1%; VR1/2 typeV10K4A-5-2. No measured trim position is supplied.

KBC16 wiring confirms KBC1 O1..O8 to R51 KC1..KC8 and KBC2 O1..O8 to R51 KB1..KB8. It independently corroborates VR19/20 to KBC1 LI/HI and VR21/22 to KBC2 LI/HI.

Panel2 native crop confirms terminal3 to+15V, terminal1 to−15V and center5 toground. This is electrical polarity, not yet the physical lever-direction mapping. Ground wire chain and four pot labels were read directly, not inferred from screen styling.

[KAS IC p6](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/06%20-%20KAS%20ICs.jpg) supplies a note-code and octave-code table with the explicit convention1=−6.5V,0=+8.5V. Its printed octave columns list C2, C2#–C3, C3#–C4, C4#–C5, C5#–C6; B1B2B3 tuples are011,101,001,110,010 respectively. This is a49-key-range table within the CS80 service scan, so do not silently extrapolate the missing octave or interpret its octave labels as MIDI numbers. KBC connectors are marked barred B inputs. Physical CMOS high/low states and negative-logic table labels must be reconciled through actual KAS circuitry.

An attempted naive assignment of ascending physical octave columns to successive OC taps, combined with direct bit inversion and the conditional switch table, produces discontinuities at some boundaries whose applicability to the actual keyboard is not established. This is an explicit unresolved consistency check, NOT evidence that the real instrument has those discontinuities. Before production use, resolve whether the discrepancy is code convention, source table reuse, tap selection, or trace interpretation; independently verify the whole mapping. Do not smooth away the inconsistency or choose the version that sounds pleasant.

### Further primary inspection, 2026-10-29

IC guidebook PDF15–18 (run40791735546 retry job123179772116) were read and their identities confirmed: YM26600 front/block15/16 and YM26700 front/block17/18. They contain pin assignments and decoder blocks, not the missing OC truth table.

Original Yamaha CS Series Servicing Guide scans were inspected in run40794512782: page14 job123187579420, page21 job123187579491, page22 job123187579339, page23 job123187579537. [Scan index](https://dragonflyalley.com/synth/jurgenHaiblePages/jh_cs50.html). Page23 directly associates OC0 with C2, OC1 with C2#–C3, OC2 with C3#–C4, OC3 with C4#–C5 and OC4 with C5#–C6 for its49-key example. Page14 repeats the barred B code table. Page21 defines positive voltage logic for timing; that must not silently override KAS06's code-table voltage convention. Page22 describes multiplexed, nearly held outputs rather than instantaneous ideal state.

These pages do not establish the complete CS80 mapping: their worked example explicitly has four voices and49 keys. The later narrow KBC crop resolves the rail-junction dispute and a second reader corroborates the endpoint control wires. Remaining work is the mapping from actual CS80 B codes through delayed/latched Q states to the YM26700 OC selection, including its extra octave and applicable chip mode. Smoothness is a diagnostic expectation, not a primary-source proof; do not alter verified wiring to force continuity. Likewise do not infer the missing octave by extrapolation. The conditional network table is reproducible, but not yet a complete physical-key implementation.


### CS80-specific calibration anchors

Original Yamaha [Adjustment Procedures and updates](https://www.therogoffs.com/cs80/CS80%20Adjustment%20Procedures%20and%20updates.PDF), PDF29/printed27 and PDF30/printed28, were inspected in run40797172086 job123196345195 at source revision5db8a01fcfe6f61ae18596edb91d0f487463b9f5. These are actual CS80 calibration procedures, not the generic49-key IC example.

|Control setting|Physical key label in manual|Board / trimmer|DVM testpoint|Printed target|
|---|---|---|---|---|
|LOW BRILLIANCE minimum|C2|KBC1 / VR1|O1|−2.76±0.14V|
|HIGH BRILLIANCE maximum|C5#|KBC1 / VR2|O1|+2.76±0.14V|
|LOW LEVEL minimum|C2|KBC2 / VR1|O1|−2.76±0.14V|
|HIGH LEVEL maximum|C5#|KBC2 / VR2|O1|+2.76±0.14V|

The following page identifies the physical O1 terminal and both trims. The manual does not provide all61 codes or full contour curves here. Its C2/C5# labels must not silently become MIDI note numbers. The first-hand MFox repair account reports approximately2.66V on KBC2 and questions the2.76V target; importantly2.66V lies INSIDE the original printed2.62–2.90V tolerance interval. That observation alone does not establish a Yamaha erratum.

Conditional nominal check: with the verified circuit, C2 must enable the VR1 branch for that trim adjustment to work, which corresponds to Q110; C5# must enable VR2, corresponding to Q101. If those keys select L and H respectively, and the pot endpoint gives reference displacement225/56V, the nominal target magnitude2.76V requires VR1≈2136.29ohm and VR2≈4136.29ohm, both within the drawn0–5000ohm range. Nominal VR1 magnitude range is2.67857–2.85479V; VR2 range2.59217–2.79018V. These are consistency calculations under stated assumptions, not measured trimmer positions or proof of the intervening code map.

This supplies an exact remaining verification target: establish latched Q1/Q2/Q3 and OO selection at the manual's physical C2 and C5#, then each adjacent octave boundary, on the CS80 variant. Measure O1 separately on KBC1 and KBC2 at center and both endpoint settings. The unmodified49-key example's naive direct mapping does not satisfy the C2 trim-enable requirement. Do not change the confirmed wiring or invent middle-key codes to repair that mismatch.


### Actual KAS-to-KBC output path: no recoder found

The original [KAS04 schematic](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/04%20-%20KAS%20schematic.jpg) and [KAS05 wiring](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/05%20-%20KAS%20Circuit%20Board%20%26%20Wiring.jpg) were inspected at native resolution in run40798346402, jobs123200150121/123200150293, revision7ad43d470898b5a9854d884d98df9b228d021ae7. Two readers independently traced KAS04 upper-left/upper-middle native tiles:
- YM26600 pin29 B1 → IC4 pin11→12 buffer → exported B1 connector60.
- YM26600 pin30 B2 → IC4 pin14→15 buffer → exported B2 connector58.
- YM26600 pin31 B3 → IC5 pin9→10 buffer → exported B3 connector59.
- The same source lines branch directly to the local YM26700 pins9/10/11 respectively.

KAS05 independently labels the B3 connector57, whereas KAS04 prints59; KAS05 labels59 as ground E. Preserve this source discrepancy. The named wiring paths are B1(60)→KBC1 B1(24), B2(58)→KBC1 B2(23), B3(57)→KBC1 B3(21), not a bit permutation.

The buffer symbols have no inversion bubbles. No external Boolean recoding or bit permutation appears on these paths. KAS04 marks these B signals barred, unlike the unbarred octave-output labels in KAS06 prose. A missing external recoder is therefore not supported by the inspected schematic. KAS05 octave input wires are U1→keyboard T1-C1, U2→T1-C#2, U3→T1-C#3, U4→T1-C#4, U5→T1-C#5; CL connects keyboard M1-C1 and C connects M1-C2. These are physical wiring labels, not a complete YM26600 output-code table.

A proposed six-state Johnson-code hypothesis was checked against fresh KAS06 pixels and independently rejected as a transcription of that table: the actual printed triples remain011,101,001,110,010. A hypothesis fitting calibration endpoints does not establish the other physical-key codes. The remaining discrepancy is real evidence uncertainty, not permission to replace the printed table or verified gate graph with a convenient sequence.

## Implementation boundary

No arbitrary linear half-keyboard ramp, hard split at MIDI60, symmetric endpoint assumption, fixed ±octave range, or fixed ±dB range is justified by the evidence above. Do not silently add one and label it circuit-derived.

First transcribe the KBC digital decoder and switched resistor network; solve its nominal DC output for all61 original physical keys and each control combination. Preserve separate brilliance/level KBC board outputs and the receiving networks. Identify original keyboard-code mapping rather than relying on MIDI octave naming. Then validate physical lever polarity, center grounding, pot taper and source impedance from the actual panel/buffer schematics.

## Stable parameter plan (not implemented)

Against core123-parameter/host229-parameter baseline:
- Preserve core0..122 and host0..228 exactly.
- Append core123..126, host229..232: kbd_brilliance_low, kbd_brilliance_high, kbd_level_low, kbd_level_high.
- Use dimensionless bipolar−1..1, default0; do not expose unverified dB/octave values.
- Preserve sub_at_vcf AU version hint3; give these new controls version4. The current blanket extension loop using version3 must be replaced by explicit version metadata/boundaries, not by reordering old parameters.
- Defaults for missing IDs in older state must be neutral, and bank/preset selection must not erase global controls.
- Original cap categories: brilliance green; level gray. Place paired LOW/HIGH under their original KEYBOARD CONTROL subgroup; exact placement and lever direction require original panel/owner photo verification rather than guessing from available free space.

## Acceptance plan

1. Circuit oracle: all61 note codes, each slider at−1,−0.5,0,+0.5,+1; record pot voltage, buffer voltage, selected DAC nodes, pervoice output, loaded FH/FL/LC nodes. Test LOW/HIGH simultaneously, not only one-at-a-time.
2. Neutral identity: all four center values leave existing audio/control behavior unchanged, including held and releasing notes.
3. Key dependence: compare every adjacent physical key and octave boundary against derived tables; do not make monotonicity or symmetry tests a substitute for those tables.
4. Destination independence: brilliance affects HP and LP through their respective paths, not VCO tuning; level affects amplitude path, not filters. Verify both ranks and all eight voice allocations.
5. Controller interactions: determine from wiring whether pitch bend, ribbon, footage, master tune and portamento affect contour input. Test the derived behavior; do not assume oscillator pitch and physical key position are interchangeable.
6. Automation/state: historical parameter index and AU ordering snapshots; old-session migration; new-state round-trip; neutral defaults; global controls survive preset and memory changes.
7. Signal behavior: positive/negative extrema, silent patches, maximum envelopes and combined touch/modulation; finite values at supported sample rates and no unexplained discontinuity on slider automation. Any smoothing must match identified circuitry or be labeled a software adaptation.
8. Hardware validation: identify instrument/board revision/calibration, sweep key/control grid, measure circuit-node CV and resulting cutoff/gain independently, publish residuals and uncertainty. Passing numerical regressions is not1:1analog proof.

## Open evidence

- Physical positive-lever orientation; B-taper ideal linear interpretation versus real tolerance/loading. Native Panel2 electrical terminals and rails are now corroborated.
- Mapping the verified decoder graph to the actual CS80's61 physical keys, including B-to-Q timing/order and YM26700 OC selection; matching TC4016 control-level documentation; contact/reference loading, offsets, trim conditions and actual breakpoint note(s).
- All rank fanouts and their loading. KBC1-to-KC and KBC2-to-KB wiring are now corroborated.
- Physical FH/FL/LC loaded-node laws, internal IC input laws and calibrated audible endpoint ranges.
- Scan, hold, settling and leakage behavior; tolerances, thermal drift and measured unit variation.
