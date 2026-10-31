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

## Step 3: OE1/OE2 chorus and tremolo

**State: done.**

### What was implemented, and where

- `src/core/BbdChorus.h` (rewritten, same API): OE1 DO1/DO2 oscillators; clock moved by
  their mean in CHORUS; seven clock-count taps implementing the per-transfer loss smear
  and the MN3001 output-pair hold; independent noise and separate recombination,
  coupling and IC5/IC6 reconstruction per chain (upper chain = left, lower = right);
  band-limited clock feedthrough; 1 kHz VCA trims in the O1..O4 proportions; the OE1
  four-VCA TREMOLO law. No Engine.cpp change was needed (same call).
- `src/reference/Mn3001Packet.h`: new `NonidealChannel` (same graph and events as
  `Channel`, with per-transfer inefficiency, residue on every stage and output-pin clock
  pedestals). With zero options it reproduces `Channel` bit for bit (tested).
- `src/reference/Oe2Filters.h`: new `branchTransfer` composing `solveInput`, a supplied
  MN3001 baseband transfer and `solveReconstruction` for the upper (39 k) or lower (100 k)
  branch with the 1.1 k recombination Thevenin.
- `src/reference/Oe1Modulation.h` (new): DO1/DO2 law, the CHORUS clock modulation and the
  O1..O4 VCA gains.
- Docs: [BBD_CHORUS_MODEL.md](../BBD_CHORUS_MODEL.md) rewritten for the new model.

### Laws and evidence class

| Law | Value | Evidence class |
| --- | --- | --- |
| Upper chain (pins 13/14) -> IC5 (39 k) -> IC7 (O1), IC9 (O2) -> OE1 I2/I3; lower chain (pins 9/8) -> IC6 (100 k) -> IC8 (O3), IC10 (O4) -> OE1 I4/I5; I2/I3 and I4/I5 to separate summing amplifiers -> O2(55)/O3(60) -> left/right | true left/right wet paths | Source: OE57/58, OE55/56 ([OE2_CIRCUIT_EVIDENCE.md](../OE2_CIRCUIT_EVIDENCE.md), [OE1_SIGNAL_ROUTING_EVIDENCE.md](../OE1_SIGNAL_ROUTING_EVIDENCE.md)); left = I2/I3 is the routing ledger's reading |
| OUT1 on CP2 slots, OUT2 on the following CP1 slots | one-period hold, delay centre 256.5 periods | Derived from the reviewed MN3001 graph ([MN3001_CLOCK_EVENT_DESIGN.md](../MN3001_CLOCK_EVENT_DESIGN.md)) |
| OUT1/OUT2 through 2 x 2.2 k into 1 uF / 100 k | 1.1 k Thevenin loads coupling and IC5/IC6 (13.67 kHz, Q 0.812) | Printed values; stiff pins are a **declared** boundary |
| Transfer inefficiency | 5e-4 per transfer, 512 transfers (-0.37 dB at 10 kHz at the nominal clock) | **Declared inference** (typical PMOS BBD; alternatives 1e-4, 2e-3) |
| Packet noise | 1.5e-4 rms per packet, independent per chain | **Declared inference** (alternatives -70 dB and -90 dB wet-path floors) |
| Clock feedthrough residual | 0.01 square at the recombined node, odd harmonics below 0.45 fs only | **Declared inference** (amplitude); band-limiting is an engineering requirement |
| DO1 56 ms, DO2 0.55 s at maximum SPEED | DO2 = 0.1018 x DO1; DO1 = SPEED value in Hz | Ratio source-derived (adjustment procedure printed 21); one shared SPEED control, sine shapes and phase 0 at reset are **declared** |
| Clock modulation in CHORUS | mean of DO1 and DO2, +/-0.5 octave at full DEPTH | **Declared inference** from the two equal 100 k feeds at the BA617 clock node (OE57); the excursion is the existing inference |
| O1..O4 0.65/0.58/0.58/0.65 Vpp | branch weights; trims referenced to 1 kHz | Proportions source-derived (printed 21); the 1 kHz reference is **declared** (the B4 key label is not mapped to a pitch) |
| TREMOLO | linear IG00151 control, index 1 at full DEPTH; LO1 = +DO1, LO2 = +DO2, LO3 = -DO2, LO4 = -DO1; dry path kept; clock nominal | **Declared inference** |

Alternatives considered for the tremolo:
1. Dry branch switched out in TREMOLO (pure amplitude modulation of the wet). Not adopted:
   the 159 Hz interstage high-pass (and the 0.015 uF VCA output capacitors) would make a
   wet-only tremolo bass-less.
2. Quadrature LO phases (0/90/180/270 degrees of one oscillator). Not adopted: the
   adjustment procedure lists two oscillators.
3. Exponential VCA control (+/-6 dB at full DEPTH). Possible; the linear input was chosen
   because IG00151 documents a separate linear input (service p52).
4. DO1 for TREMOLO only and DO2 for the CHORUS clock only. Not adopted: the clock node has
   two equal feeds.
5. The previous generic amplitude law. Replaced as the brief requires.

### Measured results (before -> after)

Tool: `cs80_oe2_stereo_chorus_tests` and a scratch harness that runs the 6719ce4
`BbdChorus` against the same truth. Steady tones, 1 s after 1 s settling, internal 96 kHz,
CHORUS on, DEPTH 0; wet = output minus dry; truth = `NonidealChannel` staircase x
`branchTransfer`, trimmed at 1 kHz.

| Wet transfer versus truth | Before (6719ce4) | After |
| --- | --- | --- |
| 50 Hz | -0.138 dB, +0.004 rad | -0.046 dB, +0.001 rad |
| 2 kHz | -0.132 dB, +0.124 rad | -0.056 dB, +0.026 rad |
| 5 kHz | -0.059 dB, +0.314 rad | -0.075 dB, +0.066 rad |
| 10 kHz | +0.635 dB, +0.602 rad | +0.113 dB, +0.098 rad |
| Wet noise with no input (L / R rms) | 0 / 0 | 4.13e-5 / 4.13e-5 (truth 4.2e-5), L/R correlation -0.001 |
| Left and right wet | identical | separate chains; identical transfer, independent noise |
| Clock line at 1.536 MHz internal | none | 1.02e-4 (truth 1.05e-4); none representable at 88.2/96 kHz |
| TREMOLO | generic whole-signal law | OE1 law; worst sample error versus truth 0.0040 (L), 0.0042 (R) at 0.5 input amplitude |

### Tests and mutation checks

`cs80_oe2_stereo_chorus_tests` (97 checks): zero-option `NonidealChannel` equals `Channel`;
lossy chain DC (packet plus sensed residue); left and right wet transfer against their own
truth branch at 50 Hz-10 kHz and 88.2/96/192 kHz; noise level, determinism, L/R
independence and reset; feedthrough line where representable and no alias at 96 kHz;
tremolo trajectories against the OE1 truth; DO2/DO1 ratio.

`cs80_bbd_chorus_tests` was updated for the intentional changes (literal 256.5-period hold,
512 x 5e-4 loss, 1.1 k loading and 1 kHz trim in its analytic chain; the clock oracle now
uses DO1 + DO2 and a window spanning a DO1 peak; "tremolo equals the legacy law" became
"tremolo is not the legacy law"; "reset empties the chain" now requires equality with a
fresh instance). Its tolerances are unchanged. In `OutputTapIntegrationTests` the silent
readout fixture also turns the chorus off (its premise is a silent input).

| Mutant | Result |
| --- | --- |
| C1 charge-transfer loss removed | detected, 12 failures |
| C2 output-pair hold centre ignored (256 periods) | detected, 16 failures |
| C3 BBD noise removed | detected, 4 failures |
| C4 both chains draw from one noise stream | survived: consecutive draws are still independent, so this was not a behaviour change |
| C4b both chains receive the same noise sample | detected, 1 failure |
| C5 clock feedthrough removed | detected, 1 failure |
| C6 clock feedthrough not band-limited | detected, 6 failures |
| C7 tremolo VCAs static | detected, 2 failures |
| C8 right VCAs in phase with left | detected, 1 failure |
| C9 CHORUS clock from DO1 only (`cs80_bbd_chorus_tests`) | detected, 6 failures |
| C10 recombination loading of IC5/IC6 removed | detected, 4 failures |
| C11 VCA trim normalization removed | detected, 32 failures |
| C12 DO2 removed | detected, 3 failures |

### Intentional circuit-engine sound changes

- CHORUS sweep gains a slow DO2 component at 0.1 x SPEED; delay centre moves by half a
  clock period (4.7 us); wet path slightly darker above 10 kHz (loss and 1.1 k loading)
  and trimmed to unit gain at 1 kHz (+0.1 dB).
- TREMOLO is no longer the generic whole-signal law: the wet returns are modulated in
  left/right antiphase at two rates over the kept dry path.
- With the effect on, each side carries independent BBD hiss (about -79 dB below a
  0.5-peak sine).

### HARDWARE_COVERAGE.md entries (dated 2026-10-31)

- Row "15 Tremolo versus chorus": circuit engine TREMOLO is the OE1 four-VCA law (declared
  phases and linear index, documented DO1/DO2 ratio and O1..O4 proportions); CHORUS clock
  follows DO1 + DO2. Open: OE1 oscillator waveforms, LO phase network, IG00151 control law.
- Row "LEFT and RIGHT outputs" / table row "Chorus/tremolo circuitry": circuit engine OE2
  has true left (upper chain, IC5, O1/O2) and right (lower chain, IC6, O3/O4) wet paths
  with independent noise, per-transfer loss, output-pair hold and band-limited clock
  feedthrough, tested against the NonidealChannel/Oe2Filters truth. Open: MN3001 loss,
  noise and pedestal data; OE1 summing gains and polarity.

### New parameters

None.

### Open items

- The host SPEED range (0.05-8 Hz) cannot reach the documented 17.9 Hz DO1 maximum;
  extending it would need a new parameter and a PURE CS-80 decision.
- Not modelled: MN3001 bias/aperture/leakage and distortion, IG00151 exponential input and
  CV feedthrough, OE1 FET switching transients, VCA output capacitors and OE1 input
  coupling, the 0.1 uF at the BA617 node, chain-to-chain tolerances.

## Step 4: PRA wah and EXP pedal (optical cell)

**State: done for the PRA wah photoresistor; the EXP volume optical network is an open
integration hook (see Open items).**

### What was implemented, and where

- `src/core/PraWahCircuit.h`: `Profile::opticalDecaySeconds` (new, 60 ms) beside the
  existing `opticalTauSeconds` (12 ms, now the brightening constant). Each sample the
  log-resistance one-pole uses the attack coefficient when its target resistance is below
  the current one (cell brightening) and the decay coefficient otherwise. Both
  coefficients are exact per-sample exponentials, computed after validation, so the
  atomic-rejection contract holds. No Engine.cpp change: the engine already feeds the
  pedal to both PRA channels every internal sample.
- `src/reference/PraWah.h`: new `OpticalCell` ODE truth, dl/dt = (lt - l)/tau(direction)
  in l = ln R, integrated with RK4 (`integrateOptical`).
- Docs: [REALTIME_PRA_WAH_MODEL.md](../REALTIME_PRA_WAH_MODEL.md) optical row updated.

### Law and evidence class

| Law | Value | Evidence class |
| --- | --- | --- |
| PRA53 photoresistor in the wah network, EI-driven LED/optical path | resistance-controlled all-pass network | Source ([FOOT_CONTROL_EVIDENCE.md](../FOOT_CONTROL_EVIDENCE.md), [PRA_WAH_REFERENCE_DESIGN.md](../PRA_WAH_REFERENCE_DESIGN.md)) |
| Heel/toe resistance and log taper | 1 MOhm / 2 kOhm, log interpolation | Existing declared profile, unchanged |
| Optical dynamics in log R | 12 ms while R falls (brightening), 60 ms while R rises (darkening) | **Declared LDR model**: CdS cells respond faster to rising illumination than to its removal; the 12 ms keeps the prior timescale for the fast direction |

Alternatives considered: the previous symmetric 12 ms one-pole; vactrol-class figures
(turn-on of a few ms, turn-off of tens to hundreds of ms; from general device literature,
not verified in this session and not CS-80 data), e.g. 3 ms / 35 ms or 12 ms / 250 ms; a
level-dependent decay (slower towards dark); a two-stage (fast then slow) release as in
optical compressor cells; an additional lamp thermal lag in the pedal. The 60 ms value and
the 1:5 ratio are engineering choices for a playable sweep, not measurements.

### Measured results (before -> after)

Tool: `cs80_pra_optical_cell_tests`, internal 96 kHz, nominal profile, full-travel sine
pedal sweep; resonance is the nominal middle-network value 1/(2 pi sqrt(t1 t2)).

| Quantity | Before (symmetric 12 ms) | After (12 ms / 60 ms) |
| --- | --- | --- |
| 90 % traverse heel -> toe / toe -> heel | 27.6 ms / 27.6 ms | 27.6 ms / 138.2 ms |
| 3 Hz sweep: cycle-mean log R bias versus the pedal | -0.000 Np | -0.726 Np (lags towards bright) |
| 3 Hz sweep: resonance range | 334-3328 Hz | 422-3328 Hz |
| Realtime versus ODE truth (3 Hz sweep) | not tested | 2.95e-4 Np worst (bound 2e-3) |

At 8 Hz the asymmetric cell no longer reaches the dark end: the resonance stays within
750-2948 Hz.

### Tests and mutation checks

`cs80_pra_optical_cell_tests` (35 checks): attack and decay step responses against the
closed forms at 88.2/96/192 kHz; the traverse-time ratio; 0.5/3/8 Hz sweeps against the
RK4 ODE truth (worst error and cycle mean); the asymmetric bias signature, with the
symmetric cell as a control; Engine EXP-WAH optical trajectory (darken, then brighten
half way) against the closed form, both channels identical.

Existing tests updated for the intentional change: `PraWahCircuitTests` covers the ninth
profile field in its nonfinite, boundary, rollback and sensitivity fixtures and checks the
60 ms darkening constant (more checks, nothing removed); `PraWahIntegrationTests` uses the
direction-dependent time constant in its closed-form optical oracle, and its toe -> heel
time-constant check now expects the 60 ms decay.

| Mutant | Result |
| --- | --- |
| P1 symmetric 12 ms cell restored | detected, 30 failures (and 61,490 in the Engine integration suite) |
| P2 attack and decay directions swapped | detected, 33 failures |
| P3 decay default drifts (30 ms) | detected, 24 failures |
| P4 forward-Euler decay coefficient | detected, 5 failures |
| P5 decay coefficient written before validation | detected by the rollback suite, 10,759 failures |

### Intentional circuit-engine sound changes

- EXP-WAH sweeps close (towards heel, darker) five times more slowly than they open, so
  fast pedal work sits nearer the bright end and the wah "hangs" on heel-ward moves.

### HARDWARE_COVERAGE.md entries (dated 2026-10-31)

- Row "17 Foot pedal selector EXP / EXP-WAH": circuit engine PRA optical cell has a declared
  asymmetric LDR law (12 ms brightening, 60 ms darkening in log R), tested against an ODE
  truth. Open: measured optical resistance versus travel/EI, lamp and LDR dynamics.

### New parameters

None.

### Open items

- **EXP volume optical networks (I2 -> VL1, I3 -> VL2)** keep the immediate linear
  expression gain. They are the same kind of LDR stage, but their steady-state law and
  polarity are not recovered, and the gain is applied on the output line that ws2 is
  reworking (controller smoothing for CC11). Integrator hook: run the expression value
  through the same declared log-R cell (`PraWahCircuit`'s law, heel/toe endpoints mapped
  back to 0..1) in the source policies before `smoothedVolume * 4 * expression`, after
  ws2's CC11 smoother lands, then update the expression oracles in
  `OutputTapIntegrationTests` and `PraWahIntegrationTests`.
- FET switch transients, optical noise and temperature effects are not modelled.
