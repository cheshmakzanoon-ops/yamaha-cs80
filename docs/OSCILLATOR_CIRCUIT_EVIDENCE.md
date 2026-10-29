# Oscillator / waveshaper evidence audit

Reviewed 2026-10-29 against source e912db1f7877b48d07463e0f5a4d3a915f64b981. This is a source and implementation audit, not hardware measurement or a claim of sonic equivalence.

## Evidence boundaries

[Scott Rider's firsthand VCO observations](https://www.cs80.com/vco.html) distinguish original IG00153 pin7 measurements from a later discrete replacement module. Original measurements show a roughly 0.45V, 30µs pulse leading the ramp and a subsequent IG00158 inverted-saw observation. The later 6Vpp saw and explicitly uncalibrated sine are the author's replacement, not original IC specifications. The stated 24.5kΩ and 1200pF multiply to29.4µs; the article's scientific-notation intermediate is a typographical error. These observations do not establish pulse injection gains for every output or the complete reset waveform across pitch, temperature and IC specimens.

[Original CS80 service sheet42](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/42%20-%20M%20Circuit%20ICs.jpg), as transcribed in HARDWARE_REFERENCES, gives nominal VCO saw3.5Vpp and WSC sine3Vpp / triangle3.2Vpp. These pin-level values are not final output levels. Board loading, polarity and sine/filter summing gain remain necessary.

[Original Yamaha CS20M/40M manual, printed49–50](https://www.polynominal.com/sample-packs/yamaha-cs20m/Yamaha-CS-20m-service-Manual.pdf) describes IG00158 saw-to-triangle conversion and diode/resistor feedback in an inverting sine-shaping amplifier. This supports a shaped-triangle model rather than independently choosing a sinusoid plus harmonic. Exact slopes, breakpoints and CS80 peripheral values are not established by this textual description; CS20M electronic selection circuitry is not automatically CS80 circuitry.

## Original guidebook inspection

[Yamaha IC guidebook](https://www.goltsynth.com/resources/downloads/CS/yamaha_ic_guidebook.pdf), visually confirmed from [source-inspection job123178349523](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/40791735546/job/123178349523):
- PDF35: IG00153 / VCO III, printed2-4. KV2 controls transistor current I=Kv/Rft with resistor at FT1. Comparator input12 drives a one-shot and switched reference-current path; timing network is external. Pin7's drawn waveform is falling saw with a leading reset feature.
- PDF39: IG00158 / WSC, printed2-6. Falling saw enters triangle converter14→3, inverter13→5 (rising saw), and pulse comparator9→7. Triangle3 is routed to sine-shaper15→2 and double-triangle12→6. PW/PWM circuitry uses11/10. The generic diagram labels3Vpp; do not override instrument-specific amplitudes with it.
- PDF36 is a **CS5** application; PDF40 is **CS30**, not CS80.
- These diagrams supply topology, not transistor parameters or quantitative reset/sine-shaper transfer.

PDF SHA256: `ff051aed0687cad4c4b246d145f85896c229e4cc679333328483118a3e7f8cc1`,13,608,992bytes. Full-page render PNG SHA256: page35 `f6318cad256f30ef2d1fdf4c9ddde018166bc624cc2a674394cc3ee234f8a3e0`; page39 `672a3663f8e59126ecea0c2dfc59d38660ebb3b3a14b93b483529047b3bfa816`.

**Polarity warning:** production's falling saw corresponds to VCO-side direction, whereas the WSC inverter output is rising. Do not flip it in isolation before checking CS80 board inversion and relative sine/filter summing.

## CS80 M41 native routing verification

Read from [original service M41](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/41%20-%20M%20Circuit%20Diagram.jpg), native crop in [job123187579181](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/40794512782/job/123187579181), box[400,1100,1800,2600], no resizing:

- IC8 OUT7 couples through4.7µF NP toT1 and IC9 inputs TI14, SI13 and PI9; the input node has22kΩ to ground.
- IC9 SO5 goes throughT2/FET6/150kΩ; PO7 throughT3/FET7/150kΩ. These meet a passive audio-mixing node with noise input NI and loading resistors, then couple through1µF NP to **IC10 AI1**, not IN16.
- No active signal inverter intervenes between WSC SO5 and HP audio input. The WSC rising-saw direction therefore reaches that input. Filter transfer polarity still matters for downstream sine-relative phase.
- Triangle TO3 is routed back toward2TI12 and through4.7µF NP toSTI15, with22kΩ shunt on the shaper-input side.
- There is no separate VCO-reset injection branch into pulse output. Comparator-input reset affects threshold crossings, not an independently mixed output pulse.

Source image SHA256 `1278f17ff05cd82b2b84153838bf95109d3f1ea1e864119de0a3f4a4af4f0080`; native crop PNG SHA256 `9059c656f422a68fe32d84b5cfd10b6372bf179beefa7cfc823b25a52c05c4cf`.

The coupling capacitors and loaded summing node prevent equating pin amplitudes or raw DC offsets with final sound. Their complete impedances must be retained in a voltage-domain model.

## Reproducible implementation findings

Before the bounded pulse correction, Dsp.h constructed a bipolar pulse p in {-1,+1}, with bandlimited transitions and duty d=clamp(30µs*f,.0001,.45):

```text
narrow = .55*p
saw = falling_saw + .70*narrow
pulse = selected_pulse + .45*narrow
sine = sin(phase) + .035*sin(3*phase)
```

The numerical coefficients .55/.70/.45/.035 have no verified electrical derivation. The mean of the synthetic narrow component is .55*(2*d-1). Consequently its added saw offset approaches -.385 and its added pulse offset approaches -.2475 at low duty. This is not a zero-baseline narrow excursion. Engine.cpp sums these outputs, scales by .9, then processes filters, amplitude and subsequent effects; its 10Hz DC blocker occurs near final output. Removing DC there does not establish correct upstream signal bias or undo preceding nonlinear/modulated effects.

A pin7 ramp containing a reset pulse feeding a comparator does not imply an independent additive pulse at the comparator output. Output transitions must follow the comparator threshold crossings of the actual input. Any eventual model must derive each WSC branch separately.

## Conditional equations, not calibrated parameters

For a constant charging current and ideal capacitor, dr/dt=I/C during the ramp. A reset event requires a separately specified discharge/one-shot state and its voltage trajectory. Given period T and reset duration tau, the ramp interval is T-tau, not automatically T; whether the current ramps continuously during reset depends on the verified circuit.

An ideal comparator output is Vhigh when its signed input difference is positive and Vlow otherwise. Its polarity, clamp references, threshold CV transfer and propagation behavior must be taken from the actual WSC network.

An ideal diode-feedback shaper can be represented by a continuous piecewise-linear transfer y=F(triangle), once slopes and thresholds are known. Neither those coefficients nor diode exponential parameters may be invented from the word “sine”.

## Required next evidence

Reconcile the verified guidebook/M41 routing with clamp levels, comparator polarity, triangle transfer, sine polarity, filter AI-to-output polarity and loaded mixing gains. Keep guidebook application examples distinguished from CS80 external values. Capture original waveforms with DC-coupled voltage scales and calibrated pitch/pulsewidth before fitting reset shape, nonlinear thresholds or device spread. Numerical antialiasing quality is a separate test from analog fidelity.

## Bounded production correction

This change removes the independently added `o.narrow*.45` term from pulse output: its extra output levels have no corresponding comparator-output branch in the inspected circuit. A bandlimited two-level pulse is a more defensible **ideal comparator approximation**, not a measured reconstruction of reset-dependent edge timing or finite comparator dynamics. CoreTests adds coherent-period mean/plateau checks at four sample rates, four frequencies per rate and five duties; exact existing BLEP-kernel comparisons; zero-frequency/extreme-width/overlapping-edge finiteness and bounds; and saw/sine/reset/phase isolation checks. These verify the intended numerical model, not physical IC behavior. C++ CI execution is required before release.

Do not change saw polarity alone, normalize all waveforms to equal voltages, or fit new sine/reset coefficients without resolving the shared phase convention and loaded signal path. Full analog equivalence remains unestablished.

## Compatibility

Existing pulse-enabled patches intentionally change timbre and upstream DC bias because an unsupported secondary reset waveform is removed. Parameter IDs, preset values, serialized state, oscillator phase advancement, saw/reset and sine paths remain unchanged. No legacy coloration toggle is added. The comparator's physical clamp levels, reset-dependent crossing shifts and finite dynamics remain unmodeled; this is not a complete IG00158 reconstruction. Existing numerical alias measurements should be rerun in CI; retaining the same pulse BLEP kernel is not proof of measured analog equivalence.
