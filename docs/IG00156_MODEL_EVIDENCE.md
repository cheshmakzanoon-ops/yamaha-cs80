# IG00156 filter model: evidence, equations and validation

Status: **circuit topology partly established; nonlinear device model and CS-80 calibration incomplete**. This document does not certify the current TPT state-variable implementation or its generic input soft clip as a transistor-level replica. It implements the evidence discipline required by [the analog fidelity requirement](ANALOG_FIDELITY_REQUIREMENT.md).

## Primary evidence recovered

### Yamaha CS-series servicing guide, printed pages 33–34

[Original Yamaha guide](https://www.goltsynth.com/resources/downloads/CS/cs_series_servicing_guide.pdf), also [archived copy](https://rcarchives.com/m/YamahaCSSeriesSynthesizerServicingGuidePOSTED.pdf).

Yamaha identifies a state-variable circuit, two controlled transconductance integrators and HP/BP/LP outputs. Cutoff responds to both key voltage and cutoff-control voltage. Its nominal relationship is:

`fc = 125 * Kv * 2^Vfc`

Here Kv is the key-voltage input and Vfc the cutoff control quantity in the guide's convention. Transconductance is proportional to control current: `gm = K * Icontrol`. This establishes the small-signal control principle, not an unlimited-range device model. The factor 125 belongs to the documented circuit and scaling; it is not universally transferable to arbitrary capacitors or a different input network.

The cited pages do not establish a calibrated saturation law, noise spectrum, slew rate, input impedance or temperature coefficient. Nor do they justify treating ±15 V supplies as exact clipping thresholds.

### Yamaha CS-20M service manual, printed pages 53–54

[Original manual PDF](https://www.polynominal.com/sample-packs/yamaha-cs20m/Yamaha-CS-20m-service-Manual.pdf). PDF pages 51–52 (zero-based indices 50–51) contain figures F-1 through F-4.

The manufacturer describes two series integrators with an op-amp feedback path, and a variable resistance controlling resonance. Its ideal integrator cutoff equation is `fc = 1/(2*pi*C*R)`; replacing resistance with controlled transconductance supplies voltage control. This independently supports the SVF family and the separation of frequency and damping controls.

**Application boundary:** the CS-20M peripheral circuit is not automatically the CS-80 M-card. Use this source for chip principles, then trace the CS-80 wiring, component values, signal levels and loading separately.

### Yamaha CS-15 service manual: grade-dependent components

[Original CS-15 service manual](https://cdn.hackaday.io/files/1953518357897184/YamahaCS15-ServiceManual.pdf), VCF circuit-board notes, note 6. The manufacturer specifies:

| IG00156 rank | R1, R3 | R2, R4 |
| --- | ---: | ---: |
| A | 2.2 kΩ | 470 Ω |
| B | 2.0 kΩ | 430 Ω |
| C | 1.8 kΩ | 390 Ω |

These values are **CS-15 evidence, not verified CS-80 values**. They show why device grade and matching peripheral parts must be recorded together. Do not randomly vary these resistors independently as a substitute for production calibration. The pin description identifies audio input 1, key-voltage input 2, cutoff adjustment 3, cutoff voltage 4, positive supply 5, Q adjustment 6, Q control 7, ground 8, Q feedback 9, LP output 10, second integrator capacitor 11, negative supply 12, BP output 13, first capacitor 14, HP output 15 and feedback input 16.

## Source images still requiring direct inspection

The full [Yamaha IC Guidebook](https://www.goltsynth.com/resources/downloads/CS/yamaha_ic_guidebook.pdf) is available, but the browser reader rejected its 13.6 MB size. No transistor-level drawing or quantitative distortion curve has yet been recovered from it in this research pass.

A [CS-15 reconstruction author's article](https://hygunduz5.wordpress.com/2026/04/04/yamaha-cs-15-clone-part-5-one-pole-is-never-enough/) reproduces the booklet and enlarged circuit:

- [Booklet image](https://hygunduz5.wordpress.com/wp-content/uploads/2025/11/ig00156.png?w=1024)
- [Enlarged circuit image](https://hygunduz5.wordpress.com/wp-content/uploads/2025/11/vcfcircuit.png?w=1024)
- [Third-party analysis redraw](https://hygunduz5.wordpress.com/wp-content/uploads/2025/11/vcfanalysiscircuit.jpg?w=1024)

The article discusses an RC path bypassing the damping element, potentially producing frequency-dependent resonance. This is a **lead to verify visually against the original**, not a recovered CS-80 component derivation. Do not transplant its quoted corner frequencies. The author's nonlinear polynomial and stronger high-frequency resonance reduction are explicitly discretionary choices; they are not measurements of Yamaha silicon and are not recommended as fidelity evidence.

## Small-signal mathematical baseline

The following is our normalized mathematical representation of the established SVF family, not a transcription of hidden transistor internals. Choose state polarity consistently when comparing physical pins:

`h = a*u - d*b - c*l`
`db/dt = w1*h`
`dl/dt = w2*b`

Here u is audio input, h/b/l are normalized HP/BP/LP nodes, a is input gain, d is damping, c is the lowpass feedback coefficient, and w1/w2 represent the two integrator rates. With physical capacitor units, each rate is proportional to the corresponding effective transconductance divided by capacitance, including the actual surrounding gain factors.

For constant coefficients:

`D(s) = s^2 + d*w1*s + c*w1*w2`
`H_HP(s) = a*s^2/D(s)`
`H_BP(s) = a*w1*s/D(s)`
`H_LP(s) = a*w1*w2/D(s)`
`w0 = sqrt(c*w1*w2)`
`Q = w0/(d*w1)`

The familiar equal-integrator, unity-feedback case gives w0=w1=w2 and Q=1/d. This derivation exposes why capacitor mismatch, summing gains and loading cannot all be represented by one random cutoff offset. Output signs and scale factors must be reconciled with measured pin voltages before using this normalization as the electrical model.

If the original has an extra frequency-dependent feedback impedance, replace constant d with its derived dynamic network and add the corresponding state. Do not merely apply an arbitrary Q-versus-cutoff curve: two models can have a similar resonance peak while different phase, transients and modulation response.

## Nonlinear model selection

The available evidence does **not** select a unique nonlinear function. In particular, `softClip(x * 1.1)` before a linear filter cannot be assumed equivalent to nonlinear transconductance, damping and finite-headroom amplifiers within the feedback loop.

Keep these alternatives distinguishable:

1. Linear nominal SVF with measured peripheral frequency dependence.
2. Input/output amplifier limiting with measured asymmetry and finite bandwidth.
3. Nonlinear integrator transconductance, including control-current dependence.
4. Nonlinear damping element and interactions with the bypass network.
5. Leakage, offsets and supply coupling added only at identified circuit nodes.

A differential-pair tanh law is a candidate only after a differential pair and its degeneration/scaling are established. A generic tanh, polynomial or clipping threshold chosen for pleasant sound is not an identified IG00156 model. Preserve the existing approximation explicitly until evidence supports replacement; never silently relabel it.

## Measurement and discrimination plan

Use a qualified technician and documented safe test fixture for internal measurements; do not modify or probe a powered vintage instrument casually. No hardware access or new measurements are claimed here.

Record instrument/card identity, IC rank, actual external parts, supply rails, warm-up, temperature, trims, source impedance, probe loading and audio voltage calibration. Establish the volts-to-digital scaling before fitting distortion.

- At small amplitude, measure complex HP/BP/LP responses over cutoff and Q settings. Fit gain, integrator rates and damping first. Hold back some settings for validation.
- Sweep the same settings at several input amplitudes. Track resonance frequency, peak gain, bandwidth, harmonic magnitudes and phases, DC shift and recovery. Amplitude-dependent Q points to feedback dynamics rather than solely pre-filter clipping.
- Compare tones far below, near and above resonance. Measure at several internal nodes where feasible; identical final THD can conceal different distortion locations.
- Apply two-tone stimuli and compare intermodulation sidebands. Use bursts and steps to test slew, saturation recovery and memory.
- Sweep cutoff/Q control voltages slowly and with sinusoidal modulation, measuring both signal response and feedthrough. Audio-rate modulation tests distinguish static transfer fitting from dynamic circuit equivalence.
- Terminate the input appropriately and measure noise at several gains/cutoffs and temperatures. Do not infer circuit noise from an arbitrary white-noise floor.
- Measure the HP-to-LP interstage signal and final output to establish loading and gain compensation. Validate the serial pair, not merely two isolated filters.
- Repeat selected measurements across calibrated voices and, if available, more than one device grade. Keep repeatability and measurement uncertainty separate from model error.

For software verification, compare the discrete solver with an offline high-accuracy integration of the **same identified equations**, test convergence under smaller time steps, and exercise parameter modulation, silence recovery and saturation. Numerical agreement validates the solver, not the assumed circuit. Run at multiple sample rates and quantify aliasing separately from analog response error. Acceptance limits must be stated against reference uncertainty, not chosen after observing the implementation.

## Concrete next steps

1. Read the original IG00156 circuit illustration pixels; record its exact page and source hash.
2. Trace the CS-80 M-card filter and any grade-dependent component notes, preserving external feedback paths.
3. Derive the full small-signal transfer function including finite impedances and feedback RC state.
4. Add a source-domain offline reference model and compare it with the existing TPT implementation.
5. Obtain or identify suitable calibrated nonlinear measurements before selecting saturation, noise, drift or rail parameters.
6. Mark each quantity as documented, derived, measured or provisional in the implementation. No current result warrants a 1:1 analog-equivalence claim.


## Direct CS-80 M-card image inspection (2026-10-28)

The original [M Circuit Diagram, printed page 41](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/41%20-%20M%20Circuit%20Diagram.jpg) was fetched by [source-inspection run 40783878519, job 123152092199](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/40783878519/job/123152092199). All four emitted image tiles were visually inspected. Source identification:

- Drawing KEC-4223-65; heading CS-80 serial number 1001 onward.
- Original image: 5100 × 3300 pixels, 1,405,965 bytes.
- SHA-256: `1278f17ff05cd82b2b84153838bf95109d3f1ea1e864119de0a3f4a4af4f0080`.
- IC10 is the high-pass IG00156; IC11 is the low-pass IG00156.

### Verified CS-80 grade matching

The drawing explicitly specifies R6 and R7 by IC rank:

| Rank | CS-80 R6 | CS-80 R7 | CS-50/60 R7, comparison only |
| --- | ---: | ---: | ---: |
| A | 2.7 kΩ | 3.3 kΩ | 3.0 kΩ |
| B | 2.2 kΩ | 2.7 kΩ | 2.4 kΩ |
| C | 1.8 kΩ | 2.2 kΩ | 2.0 kΩ |

CS-50/60 R6 matches the CS-80 column. These are now direct CS-80 evidence and supersede the earlier need to infer matching from CS-15. They do **not** supersede the need to derive the receiving circuit. Do not substitute CS-15 or CS-50/60 values into the CS-80 model.

Both filter drawings visibly contain 180 pF and 1.2 MΩ feedback-network components, two capacitors marked 0.0015 (1.5 nF in the drawing convention), 4.7 MΩ resistors and 33 kΩ resistors. Their complete connection graph, including internal IC nodes, must be transcribed from a focused native-resolution view before calculating the transfer function. In particular, merely seeing a 4.7 MΩ resistor beside an integrating capacitor does not establish that it is leakage across that capacitor.

The source therefore strengthens the case for modeling the actual external network, rather than assuming an ideal two-state filter and adding generic distortion. It does not by itself establish transistor saturation, frequency-dependent damping values, calibrated gain, or measured response. Focused component/pin inspection and the chip's internal illustration remain the next gates before an electrical-model implementation.


## Native-resolution external connectivity

A subsequent [inspection run 40784517382, M-card job 123154243112](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/40784517382/job/123154243112) emitted `m-filter-detail`: source box [1180,1080,3250,1850], 2070 × 770 pixels, **not resized**. Its PNG SHA-256 is `5dfe074bc3868f22fe662cbe55d718dbb9479db0d05f36a4089a8aae445cbcc7`; the original full-image hash above is unchanged. Direct visual inspection resolves these external connections for both IC10 and IC11:

| Endpoint A | Component | Endpoint B |
| --- | --- | --- |
| 16 IN | 33 kΩ | 15 HP |
| 15 HP | 4.7 MΩ | 14 C1 |
| 14 C1 | 0.0015 µF = 1.5 nF | 13 BP |
| 13 BP | 4.7 MΩ | 11 C2 |
| 11 C2 | 0.0015 µF = 1.5 nF | 10 LP |
| 10 LP | 33 kΩ | 16 IN |
| 13 BP | 1.2 MΩ in parallel with 180 pF | 9 FB |
| 9 FB | wire | 1 AI |

Consequently, the 4.7 MΩ resistors are **not parallel leakage resistors across the 1.5 nF capacitors**. The chip's internal terminal relationships must be included to interpret them.

The AI/FB node also has a 33 kΩ shunt to ground in each filter; the LP node additionally has a 150 kΩ shunt. The latter two resistors have a passive parallel equivalent of approximately 27.049 kΩ. This is not the complete input impedance, because the connected IC and frequency-dependent feedback remain present.

HP pin 15 is AC-coupled through a nonpolar capacitor marked 1/50 to CP2, connector terminal 46. LP audio input AI1 receives CP7, terminal 53; LP output pin 10 is taken to CP8, terminal 56. This sheet does **not** directly join CP2 to CP7 locally. Trace those connector nets and any intervening circuitry before asserting an unloaded HP-to-LP cascade or assigning interstage gain. The earlier normalized SVF equations remain a mathematical baseline, not yet the full M-card equation set.

### What can already be calculated safely

These are external-network calculations from the observed parts, not hardware measurements. With `s` in radians per second, the BP-to-AI/FB branch admittance is:

`Yfb(s) = 1/(1.2e6) + s * 180e-12`

The resistance-capacitance product is 216 microseconds. Capacitive and resistive branch-current magnitudes are equal at approximately 736.83 Hz. **This is not automatically the complete filter's pole, cutoff, resonance corner or damping law.** Those require the IC's internal response and the rest of the network.

For example, define I16, I14 and I11 as current entering the IC at the specified terminals, and Vn as each pin's voltage. The observed external components imply:

`(V16-V15)/33000 + (V16-V10)/33000 + I16 = 0`

`(V14-V15)/4.7e6 + 1.5e-9 * d(V14-V13)/dt + I14 = 0`

`(V11-V13)/4.7e6 + 1.5e-9 * d(V11-V10)/dt + I11 = 0`

These are useful reference-model constraints. They are **not a closed system** until the device-port equations and source/loading relationships are identified. Setting the unknown currents to zero would be an additional, unjustified chip assumption.

The cutoff-control pin 4 is connected to its grade-selected shunt R6 (HP) or R7 (LP), a capacitor marked 0.01 to ground, and the trim feed through 150 kΩ. The trim pot is 100 kΩ across the supplies. Source f1/f2 and envelope-network connections also load these nodes, so R6/R7 times 0.01 µF alone must not be advertised as the complete control-port time constant.

### Remaining source gates

1. Inspect the original IG00156 booklet illustration and its external/terminal conventions. The exact reproduced image is [ig00156.png](https://hygunduz5.wordpress.com/wp-content/uploads/2025/11/ig00156.png?w=1024); the article's enlarged circuit is [vcfcircuit.png](https://hygunduz5.wordpress.com/wp-content/uploads/2025/11/vcfcircuit.png?w=1024). Fetch original-size image bytes if the host supplies them and retain a hash. These are reproductions to reconcile with the original guidebook, not new measured data.
2. Trace CS-80 connector CP2/CP7/CP8 through the full wiring diagram or harness sheets. Do not confuse connector-net labels with internal IC pin names.
3. Close the linear nodal equations before selecting a discrete solver or adding a frequency-dependent Q correction.
4. Identify nonlinear device behavior through original curves, transistor information or calibrated measurement. The newly recovered passive graph does not identify a saturation function.

## CS-80 pages 42, 52 and 44: calibrated endpoints and resolved interstage wiring

Inspected actual source-image pixels from Actions run **40785572946**, jobs **123157781827** (42), **123157781605** (52) and **123157781892** (44). All three images are 5100 x 3300. These are original Yamaha documentation scans, not hardware measurements.

| Page | Original source | SHA-256 |
| --- | --- | --- |
| 42 | [M Circuit ICs](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/42%20-%20M%20Circuit%20ICs.jpg) | `5698fdfd19600f6b0b8b8f9dfdfc70723248b4b33e8daad39b9f9c0ee11cc24d` |
| 52 | [M Circuit ICs](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/52%20-%20M%20Circuit%20ICs.jpg) | `387d44e9f891c8af9bcef83e3894077df4f198d1676d4580041c828ba1c7fb51` |
| 44 | [M11-12 Circuit Board and Wiring](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/44%20-%20M11-12%20Circuit%20Board%20%26%20Wiring.jpg) | `3320dd2f625c79d7cca9b98abd5cade9d1b95477e6d8990a93a296c9cda71256` |

Four overlapping quadrant crops per page were inspected. Source boxes are [0,0,2614,1714], [2486,0,5100,1714], [0,1586,2614,3300], [2486,1586,5100,3300], each rendered to 1800 x 1180. The text is legible, but future fine transistor or trace inspection must use native crops rather than infer unreadable markings.

### Actual CS-80 IC calibration anchors

Page 42 identifies IG00156 pin 2 as the key-voltage input, with the stated key-voltage span 0.25–4.0 V. At key voltage 0.25 V and cutoff-control voltage 5 V, the stated cutoff is 1 kHz. This agrees with the previously cited family-guide relation:

```
fc = 125 * KV * 2^Vf
125 * 0.25 * 2^5 = 1000 Hz
```

That numerical agreement is a cross-source consistency check, not proof of unlimited voltage range, board-level slider scaling, exact temperature compensation or IC matching.

Page 42 also supplies actual resonance anchors:

- Pin 6 Q-adjustment sets Q to 5 when pin 7 VQ is 0 V.
- Pin 7 control at 0 V gives the maximum stated Q of 5.
- Pin 7 control at 10 V gives the minimum stated Q of 0.5.
- Pin 9 is described as the Q-feedback **output** pin; do not assume it is a high-impedance summing input merely because its label is FB.
- Pin 16 is described as the feedback input.
- Pin 14 and pin 11 are the cutoff-determining capacitor terminals.

**These endpoints do not determine an interpolation curve.** Linear Q, linear damping (1/Q), exponential Q and a transistor-derived response can share endpoints and differ substantially in between. In particular, the tempting law `1/Q = 0.2 + 0.18*VQ` is only an endpoint-interpolating hypothesis, not a transcribed Yamaha equation. Do not ship it as a verified circuit law.

The page's simplified response sketches carry ambiguous dB/voltage annotations; they are not sufficient evidence to replace the two-integrator topology with a one-pole model. Page 52 describes other M-board IC functions, not IG00156 internals. Neither page contains the needed transistor/internal-amplifier graph.

### CP2/CP7/CP8 wiring is now resolved for M11 and M12

Page 44 (drawing KEP-NA03574-6Y, CS-80 serial 1001 onward) explicitly lists:

- M11 terminal 46 CP2 wired to **RA1-M11-CP7 (53)**, color code BE.
- M12 terminal 46 CP2 wired to **RA1-M12-CP7 (53)**, color code BE.
- Reciprocal terminal 53 CP7 entries point to the same-board CP2 (46).
- M11 terminal 56 CP8 wired to **RA1-M11-CP3 (29)**, color code VI.
- M12 terminal 56 CP8 wired to **RA1-M12-CP3 (29)**, color code VI.
- Reciprocal terminal 29 CP3 entries point to the same-board CP8 (56).

Combined with page 41, the first two voice-board instances therefore have this evidenced interstage route:

```
HP IG00156 IC10 pin15
  -> 1 uF nonpolar coupling capacitor
  -> CP2 terminal46
  -> same-board harness wire
  -> CP7 terminal53
  -> LP IG00156 IC11 AI pin1 / FB pin9 external node
```

This supersedes the earlier unresolved CP2/CP7 warning **for M11 and M12**. Other voice-board wiring sheets have not yet been individually checked. The physical harness wire is not an inserted gain stage.

The LP AI/FB node also has its feedback branch and internal IC port attached. Therefore do not compute the entire coupling response using only 1 uF times the parallel 33k/150k ground resistors. A suitable circuit solver must include the capacitor current:

```
iCouple = 1e-6 * d(V_HP15 - V_LP_AI)/dt
```

and the complete KCL of both connected ports, including internal chip currents and external feedback. The passive capacitor relation itself is established; the resulting total transfer function is not yet closed.

### Implementable next DSP improvement and its acceptance gate

The source now justifies a **voltage-domain calibration layer**, independent of any claim to transistor-perfect saturation:

1. Keep KV, Vf and VQ explicit and separate from UI slider positions. Document the unverified UI-to-voltage mapping instead of silently calling a normalized knob a circuit voltage.
2. Add the 0.25 V / 5 V / 1 kHz cutoff anchor as a regression target. For a family-guide exponential control implementation, also check that doubling KV or adding one Vf unit doubles the requested analog cutoff. These are control-law tests, not sampled-output cutoff measurements.
3. Add independently named resonance calibration anchors Q(0 V)=5 and Q(10 V)=0.5, with a source tag. Do not conflate endpoint checks with validation of the existing middle-of-travel curve.
4. Distinguish requested analog cutoff from the digital coefficient mapping; compare the discrete filter's measured complex response against the chosen analog reference at several sample rates. Frequency warping correction is a solver decision, not a Yamaha component.
5. Preserve the current filter implementation until the complete IC graph closes. Then replace the approximation with a coupled linear nodal prototype containing both integrators, the 1.2M/180p feedback branch, the 4.7M paths and the interstage 1 uF capacitor. Verify its small-signal transfer before adding any nonlinear device terms.

A safe immediate code change is to expose and test these calibration anchors and remove unsupported hardware-exact descriptions; it is **not yet justified** to change audio behavior by inserting an arbitrary RC corner, high-frequency resonance taper or generic soft clip. The latter can sound plausible while contradicting the documented loaded graph.

### Precise remaining internal-graph source

The original [Yamaha IC guidebook PDF](https://www.goltsynth.com/resources/downloads/CS/yamaha_ic_guidebook.pdf) remains the preferred next target. A previous web fetch reported 13,608,992 bytes, too large for that web renderer but within the source-inspection workflow's existing 16 MiB download bound. A bounded PDF path can locate IG00156 by extracted text and render only the matching pages. This requires explicit workflow support for PDF decoding and the source host, not removal of bounds.

Alternative booklet reproductions, pending visual/hash verification, are [ig00156.png](https://hygunduz5.wordpress.com/wp-content/uploads/2025/11/ig00156.png) and [vcfcircuit.png](https://hygunduz5.wordpress.com/wp-content/uploads/2025/11/vcfcircuit.png). Omitting the width query avoids intentionally requesting a downscaled image. The third-party analysis redraw is not an original Yamaha schematic and must not be silently promoted to that status.

Until that graph is available, internal current directions, amplifier gains and port impedances remain explicit unknowns. Even a complete functional diagram would not by itself identify silicon saturation, slew, mismatch, temperature drift or rail asymmetry; those need original quantitative curves or calibrated hardware data.

## Recovered IG00156 functional graph: conditional linear model, not transistor equivalence

Actions run **40786132378**, jobs **123159660277** and **123159660319**, supplied these native-resolution reproductions. Both complete images were visually inspected:

- [Booklet reproduction](https://hygunduz5.wordpress.com/wp-content/uploads/2025/11/ig00156.png), 1958 x 842, 270344 bytes, source SHA-256 `f7fa90bcdf64c07260484630f329fd2fa7acc24d25a55c9e38a8fb830e4e01da`; full-native PNG SHA-256 `2fa9424e7c8fafad6cb1781e876a208d5448e1e2bfce14108423bb141e86ad8f`.
- [Circuit enlargement](https://hygunduz5.wordpress.com/wp-content/uploads/2025/11/vcfcircuit.png), 1908 x 1392, 258899 bytes, source SHA-256 `127e293090f97fa4a1f6e9cbd8b5220fe91ce675ae724ae3174256e92e013a99`; full-native PNG SHA-256 `5a5f33032e949937976975839e948fbf2a95bebbe53c717b7c50c73d5bfc12a4`.

The booklet identifies Mitsubishi manufacture. The two images show the same **functional** diagram, not individual transistors. They establish three amplifiers OP1/OP2/OP3, two variable-gm blocks, capacitor feedback around OP2/OP3, and a Q-controlled block labeled only “Variable” linking BP to pin9. They do **not** specify transistor saturation, a passive-resistor law for that block, op-amp gain/bandwidth, or explicit +/- labels on the amplifier inputs. Diagram crossings must not all be made junctions: the LP feedback line crossing the HP/BP output lines is not a short between the three outputs; similarly audio and control crossings are not indiscriminately connected.

### Port graph recovered

- OP1 output is HP pin15. Its feedback-side input is pin16, joined through the two external 33k resistors to HP and LP.
- OP1's other input is AI pin1. The external board connection joins this to pin9, the Q-block output.
- HP drives gm1, whose output feeds the capacitor-side OP2 input. OP2's other input is grounded; its output is BP pin13. External C1 is between its capacitor-side input and output.
- BP drives gm2, whose output feeds the capacitor-side OP3 input. OP3's other input is grounded; output is LP pin10. C2 is its feedback capacitor.
- BP also feeds the Q-controlled “Variable” block returning at pin9.
- Both gm blocks receive the cutoff-control current. The diagram gives control pins, but not a quantitative transfer or range for every internal block.

The amplifier arrangement is consistent with inverting integrators and a noninverting OP1 signal input. Treat those ideal amplifier/polarity assumptions explicitly, since the reproduced drawing omits +/- symbols. Under that interpretation, with equal 33k feedback resistors:

```
h = 2*a - l
```

Here h=HP, b=BP, l=LP, a=AI/FB external node voltage. This is **not** the normalized equation h=u-d*b-l unless the input/feedback network has first been reduced with its actual gain.

### The 4.7M paths are feedforward, not leakage

Each 4.7M resistor connects a gm block's input signal to the corresponding integrator summing node, not across its feedback capacitor. With ideal virtual earth and positive transconductance defined as current entering the summing node:

```
db/dt = -w1*h
dl/dt = -w2*b
w1 = (gm1 + 1/Rff1)/C1
w2 = (gm2 + 1/Rff2)/C2
Rff1 = Rff2 = 4.7e6 ohm
C1 = C2 = 1.5e-9 F
```

The external contribution alone is 141.844 rad/s (22.575 Hz when divided by 2*pi). This is an **ideal-model contribution**, not a measured universal minimum cutoff: internal gm polarity/range, finite amplifier effects and control calibration have not all been established. Adding an exponential leak term proportional to b or l would model a different resistor placement.

### A port-law-explicit prototype

Define Iq(a,b,VQ) as current leaving the AI node into the internal Q block, and keep it unknown until its law is sourced or measured. The external feedback branch is fully known:

```
Rfb = 1.2e6 ohm
Cfb = 180e-12 F
iExternalFeedback = (a-b)/Rfb + Cfb*d(a-b)/dt
```

For a bench prototype driven through an explicitly chosen source resistance Rs, with an explicitly chosen shunt Rg, an idealized node equation is:

```
(a-u)/Rs + a/Rg + (a-b)/Rfb
    + Cfb*d(a-b)/dt + Iq(a,b,VQ) = 0
```

This assumes negligible OP1 input current. Rs is a fixture/model parameter here, **not** an invented CS-80 component. Rg must represent the actual node's ground network: the observed LP ground resistors alone yield 33k||150k, while HP has its own input-source network to trace.

An explicit three-state implementation can use x=a-b:

```
a = x+b
h = 2*a-l
db/dt = -w1*h
dl/dt = -w2*b
dx/dt = ((u-a)/Rs - a/Rg - x/Rfb - Iq(a,b,VQ))/Cfb
```

This closes only after an Iq law is selected. For example, Iq=Gq*(a-b) describes a hypothetical voltage-controlled conductance; Iq=Jq(b,VQ) describes a different controlled-current-output hypothesis. The image's generic “Variable” box alone does not choose between them. Naming such a hypothesis in an offline experiment is acceptable; calling it verified Yamaha hardware is not.

For the actual HP-to-LP connection, replace the fixture source term with the shared 1uF coupling-capacitor current and solve the two filters together. Do not independently process two ideal filters followed by a guessed interstage high-pass and claim the same loaded circuit.

### Practical next implementation gate

The highest-value next DSP work is an **offline conditional circuit prototype**, kept separate from the production sound engine, with ideal-opamp assumptions and Q-port hypotheses visibly labeled. It should:

1. Retain explicit physical units and all recovered passive components.
2. Compare conductance versus controlled-current Q-block hypotheses against the documented Q endpoints and low-level measured complex HP/BP/LP response.
3. Derive the linear state-space matrices symbolically or from MNA, test poles/zeros and passivity where applicable, then compare a high-accuracy continuous-time solution with the intended discrete solver.
4. Check whether the 180p branch's state is actually independent after full constraints are applied; do not assume every capacitor creates an independent state in an ideal-opamp network.
5. Only replace the current audio engine once a selected port model is supported, stability/convergence tested, and gain/cutoff/Q calibration checked. Generic clip insertion is still not a recovered nonlinear law.

Useful discriminating measurements are pin9 output impedance versus VQ, pin9 current at separately controlled AI and BP voltages within safe operating bounds, and full three-output small-signal transfer over frequency/control voltage. A qualified technician should perform any powered-board probing. These observations distinguish a bilateral conductance from a controlled-current output and identify internal current polarity without guessing from a block label.

Remaining primary-source work: inspect the complete original guidebook page (not just this cropped reproduction), and the family service guide's explanation of the Q-control element. Neither newly inspected image supplies quantitative nonlinear operating curves. The EXP/Linear labels under the two cutoff inputs must also be reconciled with the service manual's explicit calibration and external voltage conventions before changing the control law; a block-label adjective is not by itself an equation.

## Primary PDF confirmation and a concrete conditional reference implementation

Actions run **40787065697**, job **123162794733**, rendered [CS20M/40M service manual](https://www.polynominal.com/sample-packs/yamaha-cs20m/Yamaha-CS-20m-service-Manual.pdf) PDF page 52 (printed page 54). PDF SHA-256 `ee319c12286165d2286cce52c9d6f2f8d96ce471fbfb04b41125a7a29425eb14`, 6172162 bytes. The inspected full-page render is 1274 x 1800, PNG SHA-256 `c29e7af837805e4d43fb39d764ab44884701410967141723dc015f8ea0265a45`.

Figures F-3 and F-4 explicitly mark amplifier polarity: OP1's AI input is positive and its feedback input negative; OP2 and OP3 have the gm/capacitor summing node at the negative input and ground at the positive input. This **confirms the signs in the earlier ideal-amplifier equations**, which no longer rest solely on an unmarked booklet symbol. The text and F-4 identify the Q element as a gm converter replacing the conceptual variable resistance. They do not specify its quantitative input/output port law, output resistance or VQ-to-gm relation. The family conceptual topology is corroboration of the same IG00156, not authorization to substitute CS20M board component values for CS80 values.

### Runnable-reference design, isolated from production DSP

A useful first numerical experiment is explicitly named **ideal-opamp, unilateral-Q-current hypothesis**:

```
Iq(a,b,VQ) = -gq * b
```

The minus sign means positive b injects current into a. gq is a supplied nonnegative coefficient in siemens, **not yet a sourced function of VQ**. This sign gives damping with the evidenced inverting integrators. No nonlinear distortion is implied.

For the previously defined resistor-fed bench fixture, let Gs=1/Rs, Gg=1/Rg, Gf=1/Rfb and state z=[x,b,l]. Then dz/dt=A*z+B*u with:

```
A = [
  [-(Gs+Gg+Gf)/Cfb, (gq-Gs-Gg)/Cfb, 0],
  [-2*w1,           -2*w1,           w1],
  [0,               -w2,            0]
]
B = [Gs/Cfb, 0, 0]^T

HP = [2, 2, -1] * z
BP = [0, 1,  0] * z
LP = [0, 0,  1] * z
```

Implement the matrix construction with positive finite Rs/Rg/Rfb/Cfb/C1/C2/Rff validation, nonnegative gm1/gm2/gq validation for this particular hypothesis, and physical-unit names. Expose gq directly in the experimental API; do not hide an arbitrary Q-slider interpolation inside it. This is sufficient for a standalone reference solver without pretending the Q port law has been measured.

For constant coefficients and a time step T, implicit trapezoidal integration is:

```
(I - T*A/2) * zNext
    = (I + T*A/2) * zNow + T*B*(uNow+uNext)/2
```

Use a small pivoted linear solve, not an explicitly formed inverse. A singular/ill-conditioned solve must produce a diagnostic, not silently clamp the state. For time-varying coefficients, the endpoint form uses A_now*zNow and A_next*zNext separately; freezing coefficients is an additional approximation. A high-accuracy continuous-time reference or exact fixed-coefficient state transition should be the comparison target. Explicit Euler is a poor default for this potentially stiff small-capacitance network.

A separate algebraic-limit test sets Cfb to zero **by eliminating x**, not by dividing by zero. In the equal-integrator case this fixture's second-order reduction has:

```
Gtotal = Gs + Gg + Gf
inputGain = 2*Gs/Gtotal
damping = 2*(Gf+gq)/Gtotal
Q = Gtotal / (2*(Gf+gq))
```

This provides a closed-form solver test and a fixture-only calibration equation `gq=Gtotal/(2*Q)-Gf`. It is not a universal CS80 Q law; its dependence on Rs/Rg makes that limitation explicit. For unequal w1/w2, use `Q=sqrt(w1*w2)/(damping*w1)`.

Minimum reference tests:
- Algebraic OP1 residual h+l-2*a is zero to floating-point tolerance.
- Both integrator KCL residuals converge with timestep refinement.
- Zero input and zero state remain zero.
- The Cfb->0 reference approaches the independently derived second-order transfer at frequencies where the eliminated-fast-state approximation applies.
- For representative physically admissible fixture coefficients, check continuous-time poles and discrete-time stability, complex HP/BP/LP transfer, and timestep convergence before any production adoption.
- Compare this hypothesis with a bilateral conductance Q block; matching the two endpoint Q values alone must not be treated as model identification.
- Test the 1uF coupling only in a two-filter coupled reference, not as a guessed independent high-pass stage.

### Original guidebook inspection status

Job **123162794502** successfully retrieved the original [IC guidebook PDF](https://www.goltsynth.com/resources/downloads/CS/yamaha_ic_guidebook.pdf): 164 pages, 13608992 bytes, SHA-256 `ff051aed0687cad4c4b246d145f85896c229e4cc679333328483118a3e7f8cc1`. Text matching found **no IG00156 pages** and fell back to PDF pages 1–3. Actual pixels show the cover, a handwritten scan note and an index page. Therefore this run did **not** inspect the original IG00156 guidebook page and must not be cited as doing so. The next bounded lookup should render index pages 4–6 or use bounded OCR to locate the printed section/page, then render that page. The existing reproduced functional diagram remains the available IG00156 guidebook evidence pending that check.

## Original guidebook page identity confirmed; quantitative Q/nonlinearity still unknown

The original guidebook inspection is now complete for the IG00156 entry and supersedes the preceding pending-page status. PDF page 5's index identifies IG00156 as section **2–5**. Offset samples at PDF pages 12, 20 and 28 established two PDF pages per entry; direct inspection then confirmed **PDF pages 37–38**.

Actions run **40789983782**, job **123172514519**, source key `icguide_156`, rendered both pages from the previously hashed original PDF `ff051aed0687cad4c4b246d145f85896c229e4cc679333328483118a3e7f8cc1`:

- **PDF 37 / printed 2–5:** IG00156 manufacturer/function heading, package drawing, functional block diagram and pin-description table. Full-page render 1800 x 1272, PNG SHA-256 `25d5d6d5a944609bd4f1184d9cfd6dcdbdbbd4451460945afa90473a91137691`.
- **PDF 38 / continuation:** CS-5 PN1 1/2 application circuit illustration. Full-page render 1800 x 1272, PNG SHA-256 `71a9ff9f284ffd43ea04a1dde0dce24834586511f5c03dc2ee953ed4570566fb`.

The original page37 corroborates the previously inspected reproduction. It labels the Q-path block only “Variable”; the table identifies pin9 as Q feedback, without a quantitative current/voltage characteristic. Neither page contains a transistor-level internal schematic, Q-control transfer equation, nonlinear operating curves, output-impedance specification, slew-rate data or saturation parameters. No such data should be inferred from successful retrieval.

The continuation is explicitly **CS-5**, not CS-80. It illustrates a selectable HP/BP/LP output application with different peripheral values. It can corroborate functional usage but must not replace the already recovered CS-80 M41 component graph or its rank-specific values.

This closes the missing-original-page provenance question. It does **not** close the model-identification gate: the offline unilateral-current-Q reference remains a hypothesis rather than a measured silicon model. Further progress on the Q port requires a more quantitative original source or calibrated pin-current/output-impedance and small-signal-transfer measurements. Additional searches should target those missing characteristics, rather than repeatedly render this same two-page entry.
