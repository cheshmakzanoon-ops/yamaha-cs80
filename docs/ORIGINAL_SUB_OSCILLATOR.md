# Original SUB functions: bounded functional implementation

This closes the original function-selection and basic speed-range omissions, **not** full analog equivalence. Runtime results must be read from exact-source Actions; the development cloud filesystem was full, so this batch was statically reviewed but not locally compiled.

## What is implemented

Fresh plugin/core instances use sine, falling saw, rising saw, square, white noise and EXT, in physical switch order. Basic speed is0.7–60Hz. A shared double-precision phase is free-running while original mode is active and is not reset by notes. Ideal sine is normalized to peak1; saw peaks are scaled by3.5/3.2 from nominal IC output Vpp. Square peaks and noise amplitude/spectrum remain uncalibrated normalized approximations. Saw/square edges are ideal, not a transistor/op-amp bandwidth model.

NOISE is a dedicated continuously advanced pseudorandom stream, not sample-and-hold, and ignores base/AT SPEED. EXT is zero/disconnected: **there is no audio input bus in this batch**. Reserved EXT LEVEL persists/automates but does not affect sound and is disabled in the editor. Its visible disconnected status must not be mistaken for a complete external-input implementation.

Mode switching retains both parameter sets, never converts them or retriggers either oscillator. Each mode's phase pauses while inactive. This is an explicit compatibility policy, not a Yamaha function. Main FUNCTION/SPEED attachments follow mode on the message thread; in-flight gestures finish against the original attachment before rebinding.

## Source evidence

Primary [Yamaha owner manual](https://device.report/m/08eee3fd014ab14d5f15fe50a150900b9c3ed22ad75895b5736f46b2e186f9b0), printed8–10,37–38,49, specifies the six functions and0.7–60Hz, speed-independent noise/EXT, shared VCO/VCF/VCA modulation and pressure speed/VCO/VCF effects. EXT is a modulation source, not an audio-through path.

Native [service manual](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/index.html) inspection run40803699249:
- SUB29 job123217519172, source SHA256 b81c7e5ad2927ac04d5912bfc455a631237f512a0c7700bda41250a14bd33e60.
- SUB30 job123217519032.
- EJ67 job123217519201, source SHA2560a81a6812213e84993f3ab9d3cdf5cd070914832bec1b94f5374a4916375d4b2.
- Panel2 prior job123168711867 cross-checks switch wiring.

SUB30 identifies IC5 as IG00150, IC6/12 as NJM4558. SIO pin6 is nominal3.20Vpp sine; SO pin7 is nominal3.5Vpp falling saw. IC6 unity-inverts SO into NSO and compares SIO withground, then divides through15k/1.8k into PO. SW1 positionsM1..M6 connect SIO,SO,NSO,PO,NO3,MN respectively. Common SUI feeds IC13 follower SUO, shared to TRG5 and panel VCA depth.

ZD (1S1715P) is biased through150k from+15V and AC-coupled0.047µF into100k plus IC1. The first stage has1M/4.7k feedback network;10µF NP couples into the second10k/10M inverting stage. NO3 is its output. This does not establish the actual noise spectrum/statistics, amplitude or op-amp clipping.

### Speed: additive currents do not imply additive Hz

SUB29 VC3 (panel SPEED) and VC4 (TRG5 SP pressure) each enter IC5 pin1 via120k. The [Yamaha IC guidebook](https://www.goltsynth.com/resources/downloads/CS/yamaha_ic_guidebook.pdf), PDF29/printed2–1, explicitly shows an **exponential converter** at IG00150 pin1. Therefore additive electrical inputs can produce multiplicative frequency effects. No quantitative slope or complete CS80 transfer is established here; see [the independently reviewed IG00150 evidence](IG00150_CONTROL_EVIDENCE.md). Its adjacent CS15 example must not be used to calibrate CS80.

Original mode deliberately retains the existing provisional multiplier:
rateTarget = baseHz * (1 + 2 * ATdepth * maximumHeldPressure), bounded0.7–180Hz.
The180Hz ceiling is a software bound, **not** a measured hardware maximum. Original-mode smoothing is an explicitly provisional10ms exponential, sample-rate independent. The intermediate speed-pot taper is also unmeasured. Legacy mode retains its old rate multiplier and per-sample0.0015 smoothing exactly. Revision-marked VR10/ground resistance differs between annotated schematic and board print; do not silently pick one to claim calibration.

### External circuit evidence for the next batch

Independent readings agree EXI46→10k→IC12 inverter with100k feedback gives nominal−10. EXO45 feeds rear LEVEL VR1 terminal3; opposite end is grounded, wiper2 returns EXL44. IC12 noninverting22k/2.2k stage gives+11 and outputs MN42 to switch position6. Small-signal ideal gain is−110*a, where a is electrical wiper fraction; pot taper/value are unknown. No coupling capacitor is drawn here. Do not invent a high-pass or saturation law.

10mVrms input gives nominal1.1Vrms at full LEVEL, consistent in scale with3.20Vpp sine. This does not calibrate digital full scale. Owner50k input impedance conflicts with the shown/printed10k input resistor; retain the discrepancy. Optional input bus, real-time alias-safe chunk capture, reconstruction and measured nonlinear behavior remain pending.

## Compatibility contract

Core123..126 / host229..232 append:
- sub_original_mode, boolean default1;
- sub_original_function, six choices default0;
- sub_original_speed,0.7..60/default4.2/skew0.35;
- sub_ext_level,0..1/default0, currently disconnected.

All four use AU version hint4. Existing122 core controls/host0..121 keep hint1; programmer/selector host122..227 keep hint2; AT VCF core122/host228 keeps hint3. The old sub_wave0..5, sub_speed0.05..30/default4.2/skew0.35 and every old ID/order/default remain unchanged.

All222 shipped factory programs explicitly select legacy mode. The constructor applies the previous startup tone then overrides only mode; the displayed startup name includes Original SUB. Explicitly selecting any factory program restores its legacy behavior. Missing/invalid mode in host or user-preset XML defaults0, not current live mode or fresh default. subOscStateVersion1 is saved for diagnostics; valid explicit mode remains authoritative even without that marker. No silent old-patch waveform conversion.

## Tests and remaining gates

Core tests cover nominal phase/rates/shapes, opposite saws, white-noise speed invariance, disconnected EXT, arbitrary render partitions, reserved-control isolation, factory legacy modes, and an independent sample-for-sample legacy LFO/RNG/smoothing oracle. Plugin/UI tests cover original233 parameters, old229 order/hints, independent normalized legacy mappings, fresh startup/factory selection, old/new/invalid state and user files, actual timer-driven UI rebinding and visible disconnected status.

These tests establish software behavior, not hardware match. Compile/core numerical and sanitizer runs, UI smoke, pluginval across formats, and actual screenshots remain Actions gates. Next batch adds the optional disabled-by-default mono input, preserving default host layouts and old renders; component/measurement calibration remains a separate open workstream.
