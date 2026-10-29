# Circuit-informed reduced-order model specification

> **Current policy (2026-10-29):** Implement the closest reconstruction supported by public knowledge and explicitly labeled engineering inference under [the requirement](ANALOG_FIDELITY_REQUIREMENT.md) and [roadmap](PARITY_ROADMAP.md). Missing hardware must not stop model development or reviewed integration. Hardware measurements can refine or validate a model later; software tests do not establish measured equivalence.

Research: 2026-10-28. Scope: original CS-80 VCO/VCF, with supporting control circuits. **No physical instrument was measured.** This is a proposed next implementation specification, not a report that the current DSP meets it. No undocumented transistor saturation curve is asserted.

## Source key and evidence hierarchy

- **M41**: [CS-80 service p41, M voice circuit](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/41%20-%20M%20Circuit%20Diagram.jpg).
- **IC42**: [service p42, VCO/WSC/VCF/VCF-EG](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/42%20-%20M%20Circuit%20ICs.jpg).
- **IC52**: [service p52, VCA/VCO-II/VCA-EG](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/52%20-%20M%20Circuit%20ICs.jpg).
- **R39**: [service p39, R51–52 control-voltage summing](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/39%20-%20R51-52%20Circuit%20Diagram.jpg).
- **SG**: [Yamaha CS-series servicing guide](https://www.goltsynth.com/resources/downloads/CS/cs_series_servicing_guide.pdf). Printed p34 = PDF43; printed p61 = PDF70. Primarily CS-50 family examples: apply shared IC principles, not different models' resistor values blindly.
- **IG**: [Yamaha IC Guidebook](https://www.goltsynth.com/resources/downloads/CS/yamaha_ic_guidebook.pdf). IG00156 section 2-5 = PDF37; its following application example is CS-5, **not** CS-80.
- **ADJ**: [CS-80 adjustment procedure](https://www.therogoffs.com/cs80/CS80%20Adjustment%20Procedures%20and%20updates.PDF). Printed p11 = PDF13, complete filter alignment; printed p19 = PDF21, ring calibration.

Direct board connectivity outranks a generalized block diagram; documented calibration outranks an invented taper. SG and IC42 give useful nominal laws but do not fully specify loaded pin impedances and nonlinearities.

## Immediate implementation decision

Keep a numerically stable **two-integrator state-variable structure per filter**, with separate HP and LP instances in series. A state-variable filter is not merely a convenient generic substitute: Yamaha identifies that topology explicitly (SG pp33–34; IG 2-5). The next useful improvement is circuit-derived parameter mapping and calibrated gain, not replacing it with a Moog ladder or a guessed transistor network.

### 1. VCO pitch domain (documented; derived conversion)

IC42 IG00153 key input pin2 gives normal-footage points .25V/130.8Hz, .5V/261.6Hz, 1V/523.2Hz, 2V/1046Hz, 4V/2093Hz. Within rounding:

```text
f_vco_hz = (523.25 Hz/V) * K_v_volts * footage_multiplier
footage_multiplier = [0.5, 1, 1.5, 2, 3, 4]
```

The factor1.5/3 is the exact harmonic-footage ratio; equal-temperament +7/+19 semitones differs by about1.955 cents. Decide/document whether software footage follows hardware resistor ratios or equal-tempered intervals. M41 transposition network uses CS-80 resistors10k/20k/40k/80k and associated trim networks, supporting ratio-derived footage, but full switch-network calibration is still needed.

A MIDI conversion may use `K_v = .5 V * 2^((m-60)/12)` if MIDI60 is defined as261.626Hz. This is an interface convention, not Yamaha's historical octave naming. Oscillator main control is **Hz/V**, not1V/oct. Oscillator vibrato pin15 has separate circuitry: do not automatically equate its physical modulation transfer to arbitrary exponential semitone FM.

M41 IC8 is IG00153; IC9 is IG00158 WSC. Saw nominal3.5Vpp, shaped sine3Vpp and triangle3.2Vpp (IC42) are voltage-domain targets, not final output levels. Implement bandlimited outputs, but keep per-wave scaling explicit; equal unit-amplitude waveforms are a software normalization, not a measured hardware ratio.

### 2. Nominal filter cutoff law (documented equation)

SG printed p34 visually states:

```text
f_c [Hz] = 125 [Hz/V] * K_v [V] * 2^(V_fc / 1 V)
```

IC42 independently states the1kHz reference at `K_v=.25V, V_fc=5V`, which this equation satisfies. Consequently, **in the documented nominal control domain**: doubling key voltage doubles cutoff; +1V cutoff control doubles cutoff; +5V multiplies cutoff32 times. Keeping `K_v` independent from oscillator footage must be checked against M41's transposition/key wiring; do not indiscriminately feed already-footage-shifted oscillator frequency into both filter key inputs.

IMPORTANT: SG's voltage law and actual loaded circuit nodes need reconciliation. M41 applies external summed controls through networks with rank-selected low-kilohm shunts and much larger source resistors. R39 also attenuates sources. Therefore a raw normalized panel slider, or a raw TP1 envelope output of±5V, is **not proven to equal**±5V at the law's effective `V_fc`. Expose `effective_cutoff_cv` separately from panel position. Do not describe a guessed5-octave panel contour as calibrated.

### 3. Small-signal filter equations (derived model)

For a calibrated effective cutoff `omega=2*pi*f_c` in rad/s and `k=1/Q`, one normalized continuous-time realization is:

```text
h = x - k*b - l
db/dt = omega*h
dl/dt = omega*b
H_HP(s) = s^2 / (s^2 + omega*k*s + omega^2)
H_LP(s) = omega^2 / (s^2 + omega*k*s + omega^2)
```

Use HP output of first instance then LP output of second. These equations describe the ideal topology, **not every M41 gain/inversion**. Match actual board gain and polarity before inserting a post-filter sine mix; polarity matters there. TPT/trapezoidal integration is an appropriate numerical implementation choice, not an original hardware property. At internal sample rate`F_s`, prewarp using`g=tan(pi*f_c/F_s)`, clamp below Nyquist and smooth control changes. Avoid Euler instability being mistaken for analog resonance.

IC42 specifies nominal Q endpoints5 at control0V and0.5 at10V. It does not supply a complete Q curve. A provisional damping interpolation `k=.2+.18*V_Q` would meet both endpoints, but is **an assumption, not a derived Yamaha law**. Prefer an explicit replaceable map/table and endpoint tests; do not call its intermediate positions calibrated.

### 4. Verified M41 filter component/node inventory

Each voice uses IC10(HP) and IC11(LP), both IG00156:

|Connection/component|Value|Consequence/status|
|---|---:|---|
|C1 pin14 ↔ BP pin13|0.0015µF=1.5nF|first integrating capacitor|
|C2 pin11 ↔ LP pin10|1.5nF|second integrating capacitor|
|HP pin15 ↔ C1 pin14|4.7MΩ|parallel stage-input conductance; **not capacitor leakage**|
|BP pin13 ↔ C2 pin11|4.7MΩ|same for second stage|
|IN pin16 ↔ HP pin15; IN16 ↔ LP10|33kΩ each|feedback/summing network|
|Pin3 frequency trim load|220kΩ to ground|part of IC bias/reference; not a standalone cutoff RC|
|Frequency trims VR7/VR9|100kΩ, with150kΩ connection|must include wiper position for nodal solve|
|Q trims VR8/VR10|50kΩ|calibration-dependent|
|R6, IC rank A/B/C|2.7/2.2/1.8kΩ|HP control-node shunt|
|R7, IC rank A/B/C, CS-80|3.3/2.7/2.2kΩ|LP control-node shunt; CS-50/60 values differ|
|Control-node capacitor|0.01µF|source/load-dependent smoothing|
|Rails|±15V|not sufficient evidence for audio clipping threshold|

M41 also shows180pF and1.2MΩ shaping components in each filter network. Their full surrounding net must be preserved in a nodal extraction before deriving a pole; do not assign an audible single-pole response from their product alone.

For the fixed4.7MΩ feedforward path, `1/(R*C)=141.844 s^-1`, or22.575Hz if interpreted as an isolated unity integrator coefficient. This **must not simply be added to Yamaha's published cutoff law**: it may already be included in calibration, and the chip's transconductance summing/bias must be accounted for. This number is a useful circuit-solver check, not an independently established minimum audible cutoff.

R39's HP/LP summing differs: HP sources include120kΩ paths, LP includes39kΩ paths; brilliance paths and envelope injection also differ. M41 shows TP1 filter-envelope output branches through100kΩ toward HP control and47kΩ toward LP. This establishes that equal knob values need not yield identical effective CV excursions. Exact loaded divider gain requires all connected source impedances and trim positions.

### 5. Filter and VCA envelopes/sustain routing

IC42 IG00152 AL0..+5V, IL0..−5V, held sustain reference0V. This establishes polarity/shape. It does **not** bypass the preceding warning about loading and volts→octaves. IG00159 provides amplitude ADSR (IC52); IG00151 has separate exponential and linear control inputs.

SG p61 explicitly chooses the lower voltage between global SUS and each individual VCF/VCA release control. Higher timing voltage shortens a stage. Ideal monotonic consequence:

```text
filter_release_time = max(filter_individual_release, global_sustain_time)
amp_release_time = max(amp_individual_release, global_sustain_time)
```

Apply this when sustain is enabled, subject to actual footswitch/mode behavior. Global SUSTAIN must not become a held filter level.

### 6. Hardware calibration acceptance points

ADJ p11: use its stated square-wave/footage/key/control setup, not an arbitrary sine measurement. At FEET2', C1, resonance maximum, HP control FH=4.0V, trim HP for peak at CP3 and4.3±1Vpp. LP alignment uses HP low, HP resonance low, LP resonance high, FL=3.3V, CP3 target2.2±1Vpp. These are **setup-specific board outputs**, not universal unit-gain targets. Capturing these settings in a voltage-domain harness is the next concrete validation step.

## Saturation and remaining boundaries

There is no verified internal transistor schematic, transistor parameter set, slew rate, input differential voltage limit, or THD curve in the inspected sources. Do not present a `tanh` at every stage, an assumed thermal voltage, or a±15V hard clip as documented Yamaha circuitry. Keep any safety limiter external and labeled; keep optional coloration separately switchable.

Missing characterization does **not** block a physics-informed nonlinear model. Use the established topology and nominal source anchors, explicitly choose and document inferred active-element laws and parameter ranges, and compare credible alternatives where the sources do not identify a unique law. Explain the modeled mechanism and its uncertainty rather than selecting a nonlinearity solely because it sounds pleasing. Integration requires independent review, stability and sample-rate/convergence checks, small-/large-signal response and alias tests, sensitivity analysis, listening comparisons and compatibility regression coverage. These establish a defensible engineering approximation, not measured silicon equivalence. Manufacturer characterization, die-level evidence or authorized hardware measurements are optional later evidence for refinement and stronger verification claims, never prerequisites for this development.

## Next implementable increment and tests

1. Introduce explicit key-voltage and effective filter-CV domains without changing the stable SVF solver.
2. Test documented cutoff anchors(.25V,5V→1kHz; .5V,5V→2kHz; .25V,6V→2kHz), Q endpoints, HP/LP12dB/oct asymptotes, and nonoscillation at finiteQ.
3. Preserve separate HP/LP modulation gains and prepare a nodal solver for M41/R39. Do not collapse the two into one shared coefficient by convenience.
4. Implement global release-time priority for both envelopes. Add tests that global sustain changes no held cutoff, extends both releases, and modeII cancels older release tails when the next note arrives.
5. Add optional voltage-domain trace points CP2/CP3/TP1; calibrate per-wave gains and board gains before changing saturation.
6. Verify4.7MΩ stage conductance and180pF network in a linear circuit simulation against the published law before adding them to production DSP.

These steps are circuit-informed progress possible without a physical unit. They are not sufficient to claim measured sonic equivalence.
