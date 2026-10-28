# Hardware-fidelity audit

Audit date: 2026-10-28. Baseline inspected: `97c29f9`. This is an engineering gap register, not a certificate of equivalence. Line references in the baseline findings below refer to that commit; symbols remain useful after edits.

## Evidence hierarchy and acceptance

1. Yamaha service schematics and IC diagrams establish connections and components.
2. Yamaha operating instructions establish observable control behavior.
3. Captures from a documented, calibrated physical unit establish numerical transfer functions, tolerances, transients and spectral differences.
4. Unit tests establish that our implementation obeys its specification. They do **not** establish that the specification is the original circuit.

The project currently has a behavioral synthesis engine. It does not solve a complete CS-80 circuit netlist, reproduce every custom IC internally, or contain a calibrated reference capture corpus. A good-looking panel, finite output, plugin validation and a large preset count cannot establish sonic identity.

### Primary sources

- [Yamaha original owner manual, part 1](https://www.therogoffs.com/cs80/CS80E_1.pdf), printed p14: ring and ribbon behavior.
- [Yamaha original owner manual, part 2](https://www.therogoffs.com/cs80/CS80E_2.pdf), printed pp33–35 and p37: envelopes and footage.
- [Yamaha original owner manual, part 3](https://www.therogoffs.com/cs80/CS80E_3.pdf), printed pp43 and49: assignment and specifications.
- [Yamaha service-manual index](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/index.html), including M board, ICs, SUB, KAS, tone selectors, PRA and output boards. These are scans of Yamaha documentation hosted by a third party.
- [Yamaha's own ring-modulator engineering explanation](https://europe.yamaha.com/en/musical-instruments/keyboards/explore/montage-portal/essentials/cs.html). This discusses the original and explicitly identifies modern extensions; those extensions are not original-hardware requirements.
- [Yamaha's synthesis history](https://europe.yamaha.com/en/musical-instruments/keyboards/explore/synth-40th/history/chapter01/index.html), explanation of IL/AL envelope purpose.

No Arturia artwork, implementation or preset bank is used as the hardware specification.

## Repairs in this change

### 1. Falling-saw antialiasing polarity

**Baseline:** `src/core/Dsp.h:131`, `Oscillator::tick` subtracted the PolyBLEP residual from a falling saw. The residual's sign convention is established by `polyBlep` and the pulse implementation in the same file. Subtracting it reinforces the discontinuity rather than smoothing it.

**Repair:** Add the residual to the falling ramp. Tests isolate the ramp by subtracting the deliberately separate narrow-reset component. They check the midpoint at phase wrap and bounded ramp peaks at 44.1, 48, 96 and 192 kHz.

**Not established:** The modeled reset contribution, waveform amplitude, 30 microsecond assumption, sine third harmonic and their frequency dependence still need circuit/reference measurement. Correct antialiasing does not validate those constants.

### 2. Reset pulse width clamp

**Baseline:** `Dsp.h:125` calculated a fixed-time reset width, but `blepPulse` at line87 silently imposed a 1% minimum duty cycle. A nominal 30 microsecond pulse at 100 Hz occupies 0.3% of the period, so the generic helper changed its intended duration.

**Repair:** Permit sub-percent widths. Regression checks exercise positive and negative interior samples of a 0.3% pulse. The tiny remaining endpoint guard is numerical, not a claimed hardware characteristic.

**Not established:** Overlapping polynomial BLEPs are an approximation for very narrow pulses. A high-rate reference, harmonic-energy comparison and alias residual measurement are still needed across the complete pitch range.

### 3. Footage selectors

The original six positions are 16', 8', 5⅓', 4', 2⅔', 2'. The engine now uses semitone offsets `{-12,0,7,12,19,24}` rather than six successive octaves. The two fifth positions are not errors to be rounded to octaves. [Owner manual, p37](https://www.therogoffs.com/cs80/CS80E_2.pdf)

**Tests:** Assert all six entries. Existing tests previously verified the wrong table while separately testing an unused fifth ratio.

**Compatibility:** Parameter IDs, index count and normalized range remain unchanged. Saved states with FEET indices 2–5 now sound at their hardware-correct intervals. This is an intentional sonic correction, not transparent patch migration. A release must document it and preserve old renders if users need exact historical recall.

### 4. Ring-modulation envelope routing

The original envelope changes modulation frequency; MODULATION controls the effect blend. Its peak frequency depends on the SPEED setting. The old code instead multiplied wet amplitude by an AD envelope at fixed frequency. [Yamaha engineering explanation](https://europe.yamaha.com/en/musical-instruments/keyboards/explore/montage-portal/essentials/cs.html)

**Repair:** Route envelope and depth to oscillator frequency; multiply audio by a unit-amplitude sine, then apply the independent dry/wet control. Advance the envelope while bypassed so engaging the effect cannot resume a stale frozen attack. Existing phrase-level retrigger behavior is retained.

**Explicit provisional calibration:** `ringModulationFrequency()` uses `baseHz * (1 + kProvisionalRingFrequencyExcursion * depth * envelope)`. The constant is currently `1`, giving a one-octave peak excursion at maximum depth. This is a bounded, speed-dependent behavioral approximation, **not a measured transfer function**. The constant has a deliberately conspicuous name to prevent accidental promotion to circuit truth. Do not claim ring-model completion until the SUB board and oscillator/envelope transfer are traced and measured.

**Tests:** Zero depth and zero envelope preserve base speed; nonzero depth raises frequency; peak depends on base speed; legato notes do not reset the phrase envelope; a new phrase does. Maximum parameter/rate combinations remain finite at four sample rates. Tests assert topology and stability, not hardware accuracy.

**Compatibility:** Old ring-heavy patches change sound because the previous envelope was attached to the wrong property. Host parameter names and IDs remain intact.

**New calibration evidence:** The [Yamaha adjustment procedure](https://www.therogoffs.com/cs80/CS80%20Adjustment%20Procedures%20and%20updates.PDF), printed p19, specifies a 150ms period at VC1 = +5V with depth off, and a 5ms period at maximum SPEED, each ±5%. These correspond to approximately 6.667Hz and 200Hz. The host currently retains its historical 500Hz upper range as an extension for state compatibility; values above the documented base-speed endpoint must not be presented as original-hardware range. Exact slider voltage and envelope-depth transfer remain unresolved.

### 5. Ribbon behavior and automation

First contact is the reference position. A full upward stroke is approximately one octave; downward travel reaches sub-audio. Sustain I and II differ in whether released notes follow later ribbon motion. [Owner manual, p14](https://www.therogoffs.com/cs80/CS80E_1.pdf)

**Repair:** The engine now consumes relative displacement as a linear frequency ratio `1 + displacement`, clamped to `[0,2]`, instead of adding +12/−18 semitones. This matches the documented endpoints; the detailed resistance/contact transfer is still unmeasured. Sustain I latches the released voice's last ratio; Sustain II continues tracking it. The previously disconnected host `ribbon` parameter now participates in pitch. Direct performance input and host displacement add and clamp; they do not overwrite each other.

**Tests:** Unison contact, octave-up, zero/sub-audio downward endpoints, held/released routing, host automation path, finite output at zero frequency, and successful envelope release at every tested sample rate.

**Remaining qualification:** The physical ribbon's contact sensing, multi-finger behavior, mechanical response, pitch-voltage interactions and slew are not electrically modeled. MIDI pedal-held notes currently remain live for ribbon purposes; that is an explicit software extension requiring separate physical comparison.

## Remaining high-impact gaps

### Filter contour and sustain: open

The original IL lowers the starting cutoff, AL raises the peak, decay returns to steady cutoff, and release returns toward IL. [Owner manual, pp33–35](https://www.therogoffs.com/cs80/CS80E_2.pdf)

Baseline `Engine.cpp:518–523` maps both IL and AL around a bipolar midpoint, and feeds `masterSustain` into the filter's held level. `Dsp.h:396–404` releases toward zero instead of IL. `Engine.cpp:880–881` adds master sustain again and halves HPF envelope depth. These behaviors need replacement together with parameter/preset migration and control labeling, not isolated cosmetic changes.

Required tests: IL/AL polarity and endpoints; short note-off during every stage; release from settled sustain; independent amplitude and filter envelopes; held cutoff invariance when attack/decay settings change; documented sustain-control action. Required measurement: actual voltages and frequency ratios versus slider position, not arbitrary octave multipliers.

### Voice allocation: open

The manual describes cycling through eight assignments and ignoring an additional held key once eight are occupied. [Owner manual, p43](https://www.therogoffs.com/cs80/CS80E_3.pdf)

Baseline `Engine::chooseVoice` selects the first inactive slot and then steals the oldest voice. This is a conventional modern allocator, not a demonstrated KAS model. Oscillators and filters are reset at every assignment in `startVoice`; real free-running and sample/hold state needs investigation. Repeated-note MIDI note-off ordering and MPE are additional software conventions, not evidence of original scanner behavior.

Required tests: round-robin trace for repeated strikes, all eight held plus ninth, release/retrigger order, sustained versus held priorities, no unintended transient on assignment, and deterministic pitch/reference traces per physical voice position.

### Oscillator and custom IC behavior: open

`Dsp.h` uses analytic ramps, BLEPs, a synthetic fixed pulse and a sine with an arbitrary third harmonic. There is no transistor/IC model, component table, temperature model or measured calibration dataset. `softClip` is a generic tanh, not a measured OTA transfer. Avoid interpreting comments mentioning IG-series chips as proof that those chips are simulated.

Trace the M-board circuitry and published IC diagrams first. Record nodes, units, polarities, supply assumptions and component tolerances. Compare isolated waveforms at several pitches and pulse widths; quantify harmonic magnitude/phase, reset duration, DC offset, tuning curvature and aliasing independently.

### Filters: open

`Svf` is a generic two-integrator TPT state-variable implementation. Input saturation precedes a linear recurrence. There is no documented derivation from the actual VCF feedback network, resonance limiting, IC gain law or component tolerances. Bounds tests only establish that the filter does not diverge for a selected stimulus.

Required evidence: small-signal transfer at multiple cutoffs/Q values, level-dependent sweeps, transient and resonance decay, HPF/LPF interaction, key-follow law and modulation response. Compare magnitude and phase, not only a spectrogram with a similar outline.

### Presets and memory: open

The baseline `FactoryPresets.cpp` constructed original generic instrument settings and used one shared name list for both ranks. The accompanying panel/preset update restores distinct original tab identities, but its voicings are explicitly provisional. The content is not extracted from the tone-selector resistor networks, and memory-panel controls are not yet modeled. Reproducing the UI names does not validate the sounds.

Trace each selector circuit independently, create a provenance row for every parameter and control voltage, and preserve modern patches in a clearly separate bank. Do not describe the original hardware settings as a ROM dump: the relevant source is analog circuitry. Tests should verify bank identity, global-control preservation, channel independence and memory recall before comparisons against real recordings.

### Expression and sub-oscillator: open

The current generic LFO adds triangle and sample-and-hold, but lacks a distinct inverted saw and external-input signal path. The parameter advertised as mod-wheel-to-sub adds a static pitch offset in `Engine.cpp:853` rather than scaling the sub waveform. `P_ribbon` was previously exposed but unconsumed; its repair illustrates why parameter-count tests cannot prove audible functionality.

Baseline assigns cutoff/level/envelope trims but never consumes those fields in the audio path. Drift is a family of phase-offset sinusoids. These are not measured unit-to-unit dispersion models. Test each route with one isolated stimulus, then verify correct sign, destination, neutral state and automation behavior.

### Chorus, output and latency: open

`Chorus` is a modulated interpolated delay with a low-pass filter and arbitrary crossfeed. It is not a stage/clock model of the original output effect. Output pan, generic soft limiting, noise and modern FX further complicate comparisons. Establish dry mono reference conditions before investigating stereo output.

The 31-tap decimator has 15 internal samples of group delay at 2x, or 7.5 host samples, while the plugin reports 8. A latency assertion against the same hardcoded integer does not measure impulse arrival. Verify host compensation and fractional alignment, passband droop and stopband rejection using rendered impulses and sweeps.

## Validation deliverables needed for an eventual fidelity claim

- Versioned source register with exact page/figure/node and component values.
- A control-to-circuit-to-code matrix, including every unsupported control.
- Reference recordings with instrument identity, calibration, sample rate, input/output level and exact settings; permission to redistribute documented separately.
- Automated comparisons separating oscillator error, filter error, envelope error, modulation error and output-stage error.
- Audio-thread safety and performance tests independently of sound-matching tests.
- Blind listening with level-matched, latency-aligned renders. Do not train and evaluate on the same small preset sample.
- Evidence-based numerical tolerances. No percentage-fidelity score without an explicit metric and corpus.

The repaired core suite passes 1,379 checks in a direct C++20 optimized build on the cloud computer. GitHub Actions and plugin/UI validation are separate gates; consult their exact-commit results rather than treating this local result as a platform matrix pass.
