# Original Yamaha CS-80 reference ledger

> **Policy precedence (2026-10-29):** Under [the current requirement](ANALOG_FIDELITY_REQUIREMENT.md) and [roadmap](PARITY_ROADMAP.md), missing physical measurements do not block development or reviewed integration. Use public evidence plus explicitly labeled engineering inference, with assumptions, uncertainty and tests. Earlier measurement/promotion gates below remain limits on claims of physical verification, not a prohibition on inferred models; recorded offline status and source conflicts remain unchanged.

Research checked 2026-10-28. This is a hardware specification/reference ledger, **not a claim that the software implements or measures all of it**. No real CS-80 audio, voltages, or physical dimensions were measured in this work. “Documented” means read from Yamaha-authored scans; “inferred” and “unverified” remain distinct. Arturia manuals/UI are not the reference instrument.

## Primary sources

- [Yamaha museum instrument and photograph](https://www.yamaha.com/en/about/experience/innovation-road/collection/detail/2039/); [direct color photograph](https://www.yamaha.com/en/about/experience/innovation-road/collection/detail/2039/image/2039.jpg).
- Yamaha owner manual scans: [part 1](https://www.therogoffs.com/cs80/CS80E_1.pdf), [part 2](https://www.therogoffs.com/cs80/CS80E_2.pdf), [part 3](https://www.therogoffs.com/cs80/CS80E_3.pdf). Page numbers below are printed numbers unless explicitly called PDF pages. These are Yamaha-authored documents mirrored by David Rogoff, not third-party synth descriptions.
- [Yamaha service-manual index](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/index.html): original schematics, board diagrams, IC descriptions and panel circuits.
- [Yamaha adjustment procedure and service updates](https://www.therogoffs.com/cs80/CS80%20Adjustment%20Procedures%20and%20updates.PDF).
- [Yamaha modern explanation of its original ring-modulator design](https://europe.yamaha.com/en/musical-instruments/keyboards/explore/montage-portal/essentials/cs.html). Distinguish original behavior from explicitly expanded MONTAGE features.

## Tone selector: exact physical order

Documented by service manual [p1 specifications](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/01%20-%20Specifications.jpg), owner p37 (part 2, PDF23). Columns read left to right:

|Column|Rank I (upper)|Rank II (lower)|
|---|---|---|
|1|STRING 1|STRING 2|
|2|STRING 3|STRING 4|
|3|BRASS 1|BRASS 2|
|4|FLUTE|BRASS 3|
|5|ELECTRIC PIANO|BASS|
|6|CLAVICHORD 1|CLAVICHORD 2|
|7|HARPSICHORD 1|HARPSICHORD 2|
|8|ORGAN 1|ORGAN 2|
|9|GUITAR 1|GUITAR 2|
|10|FUNKY 1|FUNKY 2|
|11|FUNKY 3|FUNKY 4|
|12|MEMORY 1|MEMORY 2|
|13|MEMORY 3|MEMORY 4|
|14|PANEL|PANEL|

Eleven factory circuits per rank, **not the same eleven sounds on both ranks**. Only one selector per row is active. Four memories are physical mini-slider parameter sets under the upper-left hinged block-diagram lid, not digital save slots. Main PANEL settings add one selectable sound per rank. Yamaha museum confirms four miniature panels and their location. Service p1 specifies black leatherette/walnut veneer case finish.

Preset names do not establish matching sound. Owner pp28–30 provide visual panel recipes; circuit preset resistor networks are the stronger quantitative source. Existing or new numeric presets need separate provenance and audible validation.

## Panel geometry and colors

Use the [original panel-layout drawing, service p2](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/02%20-%20Panel%20Layout.jpg), owner part1 PDF3 overhead photograph, and Yamaha museum color photo together. Layout: diagram/memory lid upper left; two repeated synth-control rows upper right; global performance controls and two tone-selector rows below; long ribbon above keyboard; sustain/portamento/tremolo controls left of keyboard. Preserve narrow vertical slots, rectangular colored lever caps, small white legends and thin group outlines—not modern knobs for every parameter.

Owner pp8–14 identifies global cap colors: detune white; brilliance green; resonance red; keyboard brilliance green and level gray; ring attack/decay black, depth/modulation gray, speed white; sub function black, speed/VCO white, VCF green, VCA gray; touch pitchbend/speed/VCO white and VCF green; sustain yellow; portamento white. Most global amounts increase toward the player; do not assume a universally upward-positive visual scale.

Owner pp15–23 identifies row colors: PWM green, speed white, noise/HPF green, resonance red, amplitude A/D/S black and release yellow, VCF/sine mixing gray. Exact RGB, original paint reflectance, typography metrics and all remaining cap assignments are **unmeasured**; sample multiple well-lit hardware photographs rather than claiming colorimetric accuracy.

## Functional mapping and numerical evidence

|Area|Documented hardware behavior|Evidence/status|
|---|---|---|
|Structure|8 simultaneous keys; two independent synthesis ranks per key|Owner p31; documented|
|Oscillator/filter route|Oscillator/noise into HPF then LPF; sine bypass mixed before amplitude stage|Owner pp35,42; documented|
|Filter contour|IL negative, AL positive; attack IL→AL, decay→steady zero, release→IL; both filters move|Owner pp33–34; documented|
|Footage|16, 8, 5⅓, 4, 2⅔, 2 feet|Service p1; documented. Semitone offsets −12,0,7,12,19,24 are derived|
|Ring|Sine multiplication; separate wet amount; AD/depth changes oscillator frequency; legato does not retrigger while keys remain held|Owner pp13–14,45; documented|
|Ribbon|First contact defines reference, upward full travel ~octave, downward can reach subaudio; sustain-mode-dependent release behavior|Owner p14; documented; transfer unmeasured|
|Allocator|Repeated notes cycle voice circuits; ninth simultaneously held key ignored|Owner p43; documented|
|Envelope range|A 1ms–1s; D/R 10ms–10s|Owner p49 and service IC pages; documented nominal ranges|
|Main sub oscillator|0.7–60Hz; sine/saw/inverted saw/square/noise/external|Owner p49; documented|
|PWM duty|50–90% OFF duty (equivalent 50–10% ON duty)|Owner p32; documented|

## IC-level mapping

[Service p42](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/42%20-%20M%20Circuit%20ICs.jpg):

- **IG00153 VCO III**: key-voltage proportional frequency; table 0.25V→130.8Hz, 0.5V→261.6Hz, 1V→523.2Hz, 2V→1046Hz, 4V→2093Hz at normal footage. Saw output nominal 3.5Vpp. This is Hz/V behavior, not a generic 1V/oct input assumption.
- **IG00158 WSC**: waveform conversion; sine nominal 3Vpp, triangle 3.2Vpp; separate pulse/inverted-saw circuitry.
- **IG00156 VCF**: Q control documented 0V→Q5 and 10V→Q0.5. Reference cutoff 1kHz at key voltage .25V and cutoff-control 5V. Chip has HP/LP/BP outputs; full-board connection determines audible topology.
- **IG00152 VCF-EG**: 0–10V controls; AL 0→+5V, IL 0→−5V, sustain normally zero; higher timing-control voltage shortens stages.

[Service p52](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/52%20-%20M%20Circuit%20ICs.jpg): **IG00151 VCA** has distinct exponential and linear level-control inputs; **IG00159 VCA-EG** supplies ADSR; **IG00150 VCO II** supplies sine/saw and specifies a 200Hz calibration point. Chip presence alone does not imply a transistor-level software model.

## Ring calibration: unusually useful constraints

Adjustment document printed p19 / PDF21, PRA board:

- Depth OFF, VC1=+5.0V: oscillator period **150ms ±5%**, equivalent ~6.667Hz.
- Speed maximum, depth still OFF: **5ms ±5%**, equivalent **200Hz**.
- Attack control A=+5V: **31ms ±5%** attack target.
- Minimum attack/decay: envelope EO zero target **0±0.01V**.

These are documented service acceptance targets, not measurements obtained here. Yamaha's modern design explanation says original maximum envelope excursion depends on the base Speed setting; its new “Deep” mode deliberately differs. Do not substitute a fixed high endpoint independent of Speed or assert a calibrated 2kHz base maximum.

## Validation needed before claiming faithful emulation

1. Reconcile all implemented routes/controls against the ledger; maintain separate labels for implemented, approximated and absent behavior.
2. Obtain authorized clean hardware captures with exact panel photographs and calibration status: isolated sine/saw/pulse, per-voice repetition, filter sweeps, contour steps, ring speed/depth matrix, chorus stereo output, touch response.
3. Match service test points and tolerances in a circuit-derived model. Determine panel taper from panel wiring/component values, not endpoints alone.
4. Compare steady spectra, envelopes, cutoff/Q, pitch drift distribution, alias energy, and stereo phase. Use listening comparisons only after level matching.
5. Full transfer functions, nonlinear saturation, component spread, ribbon curve, exact voice reassignment edge cases and factory resistor-derived parameters remain unverified. They require schematic analysis and/or hardware measurements. Passing software tests demonstrates consistency, not authenticity.
