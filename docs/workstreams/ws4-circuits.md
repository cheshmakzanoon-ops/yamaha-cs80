# ws4-circuits: original modulation and effect circuits (Phase 3)

Workstream report, 2026-10-31. Branch base: integration commit `6719ce4`.
Scope: the circuit engine (Source Performance and Isolation) only. The legacy policy is
byte-identical (see "Compatibility gates"). Every law below is labelled either with its
source page or as a **declared inference** with the alternatives that were considered.
Nothing here is a hardware measurement; the test results show agreement between a
realtime model and its offline truth model, not agreement with an original CS-80.

Pre-registered tolerances live in [`tests/Ws4Tolerances.h`](../../tests/Ws4Tolerances.h).
Each block was committed before the model it judges was evaluated.

## Step 1: SUB (IG00150) speed converter and band-limited waveforms

**State: done.**

### What was implemented, and where

- `src/core/OriginalSubOscillator.h`: new circuit-engine methods `circuitTargetRate`,
  `circuitRate` (control-node smoothing) and `tickCircuit` (band-limited waveforms).
  The legacy `tick()` and `targetRate()` are untouched.
- `src/core/Engine.cpp`: the original-SUB branch selects the circuit path under
  `usesSourceModels(policy)`; the legacy policy keeps its old branch.
- `src/reference/Ig00150Sub.h` (new truth model): pin-1 node-current sum through the
  two 120 kOhm resistors, exponential converter fitted to the anchors, the touch voltage
  solved from the node equation, and additive band-limited reference waveforms.

### Laws and evidence class

| Law | Value | Evidence class |
| --- | --- | --- |
| VC3 (SPEED) and VC4 (pressure x AT SPEED) summed at IC5 pin 1 through equal 120 kOhm | additive currents | Source: service SUB29 ([ORIGINAL_SUB_OSCILLATOR.md](../ORIGINAL_SUB_OSCILLATOR.md)) |
| Pin 1 feeds an exponential converter | f = f0 exp(I/Ie) | Source: Yamaha IC guide PDF29 / printed 2-1 ([IG00150_CONTROL_EVIDENCE.md](../IG00150_CONTROL_EVIDENCE.md)) |
| SPEED endpoints | 0.7 and 60 Hz | Source: owner's manual printed p49 |
| 200 Hz anchor = full SPEED plus full touch | full touch adds log2(200/60) = 1.737 octaves | **Declared inference** from the service p52 200 Hz IG00150 calibration point ([HARDWARE_REFERENCES.md](../HARDWARE_REFERENCES.md)) |
| AT SPEED wiper and pressure bus | linear in the wiper fraction and normalized pressure | **Declared inference** |
| Smoothing | the existing 10 ms original-mode time constant, now on log frequency | **Declared inference**: an RC on the control node acts before the converter |
| Saw resets and square edges | 4-sample integrated cubic B-spline step (in-band images fall as about sinc^4) | Engineering choice for alias control; the ideal waveform shapes and amplitudes are unchanged |

Alternatives considered for the speed law:
1. The previous provisional multiplier `base x (1 + 2 x depth x pressure)`, bounded at
   180 Hz. Rejected for the circuit engine: additive Hz at an exponential converter.
2. Additive hertz. Rejected by the exponential converter in the IC guide.
3. Equal voltage swing on VC3 and VC4 (full touch adds the whole 6.42-octave SPEED span).
   Not adopted: it would need an unrecorded TRG5 SP output swing and reaches kilohertz.
4. 200 Hz as the panel maximum with no touch. Rejected: it contradicts owner p49 (60 Hz).

With the adopted anchors the reference solves the full-touch voltage as **2.705 V on the
0-10 V SPEED scale**, a plausible pressure-bus swing; that number is an outcome of the
inference, not evidence for it. The intermediate SPEED taper remains whatever the host
parameter's Hz value says; it is still unmeasured.

### Measured results (before -> after)

Tool: `cs80_ig00150_sub_tests` (built from this branch), engine renders with one held
key and poly pressure, SUB sine measured from interpolated zero crossings over 1 s.

| Quantity | Before (circuit engine) | After |
| --- | --- | --- |
| Full SPEED (60 Hz) + full touch | 180 Hz (software bound) | 200.000 Hz rendered at 44.1 and 96 kHz, both source policies |
| 6 Hz base, full AT SPEED, half pressure | 12 Hz (additive multiplier) | 10.954 Hz (= 6 x (10/3)^0.5) |
| In-band inharmonic energy, falling/rising saw 173.3 Hz, 88.2 kHz internal | -28.2 dB | -84.0 dB |
| Same, square | -31.2 dB | -85.5 dB |
| Same at 96 kHz internal: saw / square | -29.0 / -31.9 dB | -85.8 / -87.1 dB |

The alias metric is the energy in 20 Hz-20 kHz outside +/-6 bins of every harmonic,
relative to the fundamental, with a 2^17-point 4-term Blackman-Harris DFT.

### Tests and mutation checks

`cs80_ig00150_sub_tests` (327 checks): law versus reference over a base/depth/pressure
grid; the anchors; octaves-not-hertz; log-domain smoothing after one time constant;
rendered and telemetry rates in both source policies at 44.1 and 96 kHz; the legacy
policy still at 180 Hz; the isolation SUB stream equals the band-limited oscillator;
alias ceiling and improvement over naive for both saws and the square at 61.7, 173.3 and
199.3 Hz; in-band harmonics within 0.25 dB of the additive reference.

Fixture note: the first draft used 60 and 200 Hz, whose ratios to 88.2/96 kHz are
integers, so naive aliases fell on harmonics and were excluded by the metric. The
fixture moved to non-integer ratios inside the same range; no tolerance changed.

Mutants (each built against the full suite, then restored), all detected:

| Mutant | Result |
| --- | --- |
| M1 provisional `1 + 2ap` multiplier restored | detected, 61 failures |
| M2 naive resets (band-limiting removed) | detected, 36 failures |
| M3 additive-Hz touch law | detected, 52 failures |
| M4 smoothing in Hz instead of log frequency | detected, 1 failure |
| M5 engine hook removed (circuit engine uses the legacy oscillator) | detected, 25 failures |
| M6 square second edge not band-limited | detected, 39 failures |

### Intentional circuit-engine sound changes

- SUB rate with aftertouch: touch now multiplies by up to 3.33x (1.737 octaves) on an
  exponential law instead of up to 3x on a linear law. Pressure-to-speed sweeps are
  slightly slower at low pressure and reach 200 Hz instead of 180 Hz at the top.
- SUB saw and square edges are band-limited: less aliasing in SUB -> VCA/VCF/VCO paths,
  and a 4-sample (about 45 us at 88.2 kHz) reset instead of an instantaneous step.

### HARDWARE_COVERAGE.md entries (dated 2026-10-31)

- Row "11 Sub oscillator speed": circuit engine uses an IG00150 exponential converter
  anchored at 0.7/60 Hz (owner p49) and 200 Hz full SPEED + full touch (declared
  inference from service p52); smoothing on log frequency. Legacy unchanged. Open:
  intermediate SPEED taper, real TRG5 SP swing.
- Row "12 Aftertouch -> sub speed": circuit engine adds touch in octaves (exponential
  converter, SUB29 120 k summing), declared 1.737-octave full-touch span. Open: pressure
  voltage calibration, TWS a-path smoothing.
- Row "11 Sub oscillator function selector": circuit engine saw/square band-limited
  (alias -84 dB vs -28 dB naive at 173 Hz). Open: IG00150 sine-shaper distortion,
  comparator swing, noise spectrum.

### New parameters

None.

### Open items

- The intermediate SPEED pot taper and the TRG5 SP voltage swing are unrecorded; the
  200 Hz anchor interpretation should be revisited if either is found.
- IG00150 sine-shaper distortion and the NJM4558 comparator swing (square amplitude) are
  not modelled; amplitudes keep their nominal normalized values.

## Step 2: ring modulator

**State: done.**

### What was implemented, and where

- `src/core/RingModulatorCircuit.h` (new): the speed/envelope -> frequency converter
  fitted to the adjustment-procedure anchors, and the RMO offset multiplier.
- `src/core/Engine.cpp`: in the circuit engine the ring carrier frequency comes from
  `RingModulatorCircuit::frequency` (replacing `kProvisionalRingFrequencyExcursion`) and
  the MODULATION blend uses `RingModulatorCircuit::blend`. The legacy policy keeps
  `ringModulationFrequency()` and the ideal multiply. `kProvisionalRingFrequencyExcursion`
  itself stays in `Dsp.h` because the legacy law still uses it.
- `src/reference/RingModulator.h` (new truth model): converter solved from the two
  anchors in double precision, and the analytic line spectrum of the offset multiplier.

### Laws and evidence class

| Law | Value | Evidence class |
| --- | --- | --- |
| Depth OFF, VC1 = +5.0 V: 150 ms; maximum SPEED: 5 ms (each +/-5 %) | 6.667 Hz and 200 Hz | Source: adjustment procedure printed p19 ([HARDWARE_REFERENCES.md](../HARDWARE_REFERENCES.md)) |
| Maximum SPEED corresponds to VC1 = +10 V | 0-10 V panel span | **Declared inference** (the 0-10 V control convention of IG00152/IG00156, service p42) |
| Exponential converter through both anchors | 0.981 octave/V | **Inference from the anchors**: a linear VCO through 0 V would need a 150 V maximum, and an affine linear law crosses 0 Hz at +4.83 V, leaving half the SPEED range dead |
| AD envelope EO peak at +5 V, full DEPTH adds all of it at the converter node | +4.907 octaves at full depth | **Declared inference** (by analogy with the IG00152 attack-level span 0..+5 V, service p42) |
| DEPTH wiper | linear | **Declared inference** |
| RMO multiplier `y = (x + 5e-4)(c + 5e-3) + 1e-3` | carrier feedthrough -40 dB re the sidebands of a 0.1 voice, signal feedthrough -40 dB re its own sidebands, 1e-3 output DC | **Declared inference**: a trimmed four-quadrant multiplier; the PRA multiplier's trim residues are not recorded |
| SPEED above 200 Hz (host range to 500 Hz) | extension | Masked in PURE CS-80 (isolation) at the 200 Hz maximum-SPEED anchor; kept in Source Performance |

Alternatives considered: the old one-octave provisional excursion; an EO peak of +2.5 V
(2.45 octaves) or +10 V (9.8 octaves, which runs into the VCO range); an affine linear
VCO (rejected above); an ideal multiplier (no feedthrough); trims of -30 dB or -60 dB.
The DC term is removed downstream by the PRA 10 uF input coupling, which every circuit
engine sample already passes through; MODULATION moves therefore give a slow, small
DC transient through that coupling, not a step at the output.

The +5 V attack anchor (A = +5 V gives 31 ms) and the minimum attack/decay EO = 0 V
target are not consumed: the host attack and decay parameters are already in seconds,
and the panel attack-voltage taper is unknown.

### Measured results (before -> after)

Tool: `cs80_ring_modulator_circuit_tests`. Anchor periods are measured from the
rendered output of an isolation engine with no notes, DEPTH 0 and MODULATION fully up,
so the output is the multiplier's carrier feedthrough; interpolated zero crossings after
1 s of settling.

| Quantity | Before (circuit engine) | After |
| --- | --- | --- |
| VC1 = +5 V period | not modelled (SPEED in Hz only) | 150.012 ms (44.1 kHz), 149.946 ms (96 kHz); ADJ: 150 +/- 7.5 ms |
| Maximum-SPEED period | 5 ms (clamp only) | 4.99998 ms (44.1 kHz), 5.00002 ms (96 kHz); ADJ: 5 +/- 0.25 ms |
| Envelope excursion at full DEPTH | +1 octave | +4.907 octaves |
| Signal feedthrough at MODULATION 1 (A4 sine voice) | none (ideal multiply) | -40.0 dB re upper sideband |
| Carrier feedthrough at MODULATION 1 (same voice) | none | -38.6 dB re upper sideband |
| Sideband balance | balanced | 0.00 dB |

### Tests and mutation checks

`cs80_ring_modulator_circuit_tests` (203 checks): anchors and the converter against the
anchor-solved reference over -2..11 V; envelope law against the reference; DEPTH 0 and
envelope 0 return SPEED exactly; rendered anchor periods at 44.1 and 96 kHz; PURE CS-80
masks 500 Hz at 200 Hz and Source Performance keeps it; the engine carrier follows the
law at every frame in both source policies; the legacy policy keeps the provisional law;
multiplier DC, carrier, signal and both sideband lines within 0.01 dB of the analytic
reference with no other lines; in the engine, balanced sidebands, signal feedthrough at
2 x the carrier-port offset, and carrier feedthrough that scales with MODULATION and does
not depend on the signal.

| Mutant | Result |
| --- | --- |
| R1 provisional one-octave law restored | detected, 28 failures |
| R2 linear VCO through both anchors | detected, 102 failures |
| R2b envelope adds hertz instead of volts at the exponential node | detected, 27 failures |
| R3 ideal multiplier | detected, 18 failures |
| R3b signal and carrier offsets swapped | detected, 9 failures |
| R4 engine multiplier hook removed | detected, 6 failures |
| R5 PURE CS-80 500 Hz mask removed | detected, 1 failure |
| R6 engine frequency hook removed | detected, 8 failures |
| R7 EO peak drifts from the reference (10 V) | detected, 24 failures |

Existing tests updated for this intentional change (circuit engine only):
`OutputTapIntegrationTests` and `PraWahIntegrationTests` model the isolation ring stage
with `RingModulatorCircuit::blend` (legacy oracles unchanged). Two silence fixtures in
`OutputTapIntegrationTests` were adjusted without weakening them: the readout-time
constant fixture now sets MODULATION to 0 (its premise is a silent input), and "reset
clears output and decimator tails exactly" now compares bit for bit against a freshly
prepared engine, because carrier feedthrough sounds whenever MODULATION is up.

### Intentional circuit-engine sound changes

- Ring DEPTH sweeps up to 4.9 octaves above SPEED at full depth (was 1 octave).
- With MODULATION up, the carrier bleeds about 40 dB below a single voice's sidebands,
  including with no notes held, and the dry signal leaks 40 dB below its sidebands.

### HARDWARE_COVERAGE.md entries (dated 2026-10-31)

- Row "16 Ring speed": circuit engine converter is exponential and reproduces the ADJ p19
  150 ms (VC1 = +5 V) and 5 ms (maximum SPEED) anchors, rendered 150.01 ms / 5.000 ms.
  The 500 Hz extension is masked at 200 Hz in PURE CS-80. Open: the VC1 = +10 V maximum
  is inferred.
- Row "16 Ring depth": circuit engine envelope -> frequency law adds DEPTH x EO (declared
  +5 V peak) at the same converter node: up to 4.907 octaves. Open: EO peak and DEPTH taper.
- Row "16 Ring modulation amount" and table row "Ring multiplier and carrier/envelope
  circuit": offset multiplier with declared -40 dB carrier and signal feedthrough and a
  DC offset removed by the PRA input coupling; sideband spectrum tested against the
  analytic reference. Open: real multiplier trims, distortion and intermodulation.

### New parameters

None.

### Open items

- EO peak voltage, DEPTH taper and the VC1 maximum are inferences; the 31 ms attack anchor
  is unused until a panel attack-voltage law exists.
- Multiplier nonlinearity (distortion, intermodulation) is not modelled.
