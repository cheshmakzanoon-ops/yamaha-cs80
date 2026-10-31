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
