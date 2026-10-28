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
