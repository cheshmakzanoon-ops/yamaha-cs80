# ADJ p11 input-boundary and alignment-surrogate sweep: preregistration

Date: **2026-11-02**. Status of this first commit: **no results yet**.
This follow-up is an **offline, model-only sensitivity calculation**, not a
Yamaha measurement, an amplifier or source model fix, or a substitute ADJ
acceptance requirement. The preceding two endpoints and their outcomes are
preserved in [ADJ_NOISE_LOAD_PREREG_20261102.md](ADJ_NOISE_LOAD_PREREG_20261102.md).

## Sources, physical identity and what is not known

- Yamaha CS-80 [M circuit diagram p41](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/41%20-%20M%20Circuit%20Diagram.jpg),
  KEC-4223-65, original JPEG SHA-256
  `1278f17ff05cd82b2b84153838bf95109d3f1ea1e864119de0a3f4a4af4f0080`:
  the selected gated FET waveform drives a 150 kΩ mixer resistor, a separate
  fixed 150 kΩ goes to ground, and a *third attached* 150 kΩ goes to NI(43).
  The off FET is ideal-open **only in this diagnostic**. PW and PWM connect
  to IC9 pulse-width controls, not directly to the mixer node.
- [M11/M12 wiring p44](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/44%20-%20M11-12%20Circuit%20Board%20%26%20Wiring.jpg),
  KEP-NA03574-6Y, SHA-256
  `3320dd2f625c79d7cca9b98abd5cade9d1b95477e6d8990a93a296c9cda71256`:
  NI(43) runs to SUB NO1(39) and other M-board NI terminals; HP output
  CP2(46) joins CP7(53); LP output CP8(56) joins CP3(29) on each board.
- [SUB circuit diagram p29](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/29%20-%20SUB%20Circuit%20Diagram.jpg),
  KEC-4446-68, SHA-256
  `b81c7e5ad2927ac04d5912bfc455a631237f512a0c7700bda41250a14bd33e60`:
  NO1(39) is on the feedback-buffered IC3/2 output. The drawing supplies
  neither its output spectrum/voltage in the HP setup nor its loaded output
  impedance with multiple NI branches connected.
- [Yamaha CS-80 adjustment procedures and bulletins](https://www.therogoffs.com/cs80/CS80%20Adjustment%20Procedures%20and%20updates.PDF),
  36-page PDF SHA-256
  `eea101eb338494f3d1d3fddced2d8aa7f1d877ec9d6155ca44cc77ece88276ba`:
  printed ADJ p11 (PDF page 13) says HP setup C1, 2′, PANEL, pulse ON,
  RESH/LPF/RESL HIGH, FH +4.0 V; scope **CP3**, first adjust **VR7 for
  maximum amplitude**, *then* **VR8 for 4.3 ± 1 Vpp**. LP setup next
  adjusts VR9 then VR10, scope again CP3 (2.2 ± 1 Vpp). It does not set
  NO1 noise voltage or prescribe a source-impedance measurement. The
  electrical mapping of VR7/VR8 travel to gm/Q is not specified there.
- M41 and M44 also trace CP3 through a **1 µF/50 V NP capacitor and 100 kΩ
  resistor** to IG00151 IC4 `+IN` pin 3, not to CP4. IC4 input and IC11
  output impedances are not given. Thus no finite downstream stage load
  value will be invented or fitted in this sweep.

## Locked scenarios and observations (before execution)

Treat NO1 as **zero signal only** to isolate its *possible loading*. Parameter
`alpha = (1/(150k + Z_NO1)) / (1/150k)` varies its conductance as a fraction
of the physical 150 kΩ branch's ideal termination. Use the five fixed points
`alpha = {0, 0.25, 0.5, 0.75, 1}` and **all** results; these are systematic
mathematical interpolation points, **not five source-verified impedances**.
Their conditional passive, real `Z_NO1` values are respectively
`{infinity, 450 kΩ, 150 kΩ, 50 kΩ, 0}`. The selected pulse, grounded
150 kΩ and NI branch then give

`R_source = 150 kΩ / (2 + alpha)` and `pulse_gain = 1 / (2 + alpha)`.

Keep the already registered 192 kHz, 3-second linear 8-state pair; C1 at 2′,
`f0 = 261.6255653005986 Hz`, 3 Vpp square, last 0.2 s for Vpp. Use the
existing `source_factory::alignment` HP/LP cutoff and Q *control coefficients*
unchanged as `alpha` varies; there is **no readjustment of gm/Q per changed
resistance**. LP source/network is unchanged, and the ideal IC11 output
drives that LP stage. For all five points, print and retain:

1. HP service setup **CP2** (HP stage output) Vpp, **CP3/CP8** (LP stage
   output) Vpp, and their CP3/CP2 ratio.
2. LP service setup the same three readouts.

To probe the **order** in Yamaha's instruction without claiming a calibrated
trimmer law, only at the two fixed impedance boundaries (`alpha=0,1`):

3. **VR7 surrogate**: with HP Q control frozen at 5, LP HIGH=18 kHz and
   Qlp=5, evaluate **every** HP gm-law frequency `f0 × 2^(k/2)` for integer
   `k=-4,-3,...,+4`; total cutoff adds the existing 4.7 MΩ feedforward
   corner `ffHz`. Print every CP2/CP3 value, then report the **largest CP3
   model voltage** and its grid frequency. This emulates the *maximize*
   objective, not measured VR7 settings or a continuous global optimum.
4. **VR8 surrogate**: hold the exact grid-selected cutoff from step 3 and
   separately report CP2 and CP3 for every low-frequency Q setting
   `{0.5, 1, 2, 5, 9}` (the existing nodal grid); LP remains Qlp=5.
   This is *sensitivity to hypothetical Q law settings*, not a VR8 ohms-to-Q
   conversion or an adjustment chosen to meet the service tolerance.

Print the native p11 CP3 requirement **4.3 ± 1 Vpp** and continue to mark
the original HP setup **MISS** if still outside. Only finite, positive
diagnostic checks, no new thresholds, may be added for the sweep. The
ordinary nodal test invocation must remain unchanged; the extra sweep runs
only under explicit `--ni-sensitivity`. Do not change DSP, source amplitude,
physical normalization, model laws, tolerance JSON, factory-tone manifests,
or the V1 fingerprint **`e5f877a025fc52c6`**. Check the normal nodal,
phase-correction and source-factory tone suites again afterward.

**Interpretation constraint:** a low-impedance NO1 buffer may also emit a
nonzero noise waveform. That case is *not* simulated; real noise spectral
content and time-domain Vpp cannot be inferred from this zero-signal sweep.
The synthetic cutoffs/Q values are discriminators for a future *simultaneous
CP2/CP3 and NO1* hardware measurement, **not** new component values or
hardware acceptance ranges. Commit this document before running the sweep.

## Evaluation of this pre-registered grid, 2026-11-02

The source-only preregistration above was committed as `9fdba0a` **before**
the opt-in test-only implementation was built or evaluated. All samples are
from the frozen linear model, **not actual M-board measurements**. All five
synthetic zero-signal NO1 loading points were retained, without selecting an
intermediate impedance to match the Yamaha voltage band:

| alpha | Equivalent passive `Z_NO1` hypothesis | HP CP2 Vpp | HP CP3 Vpp | HP CP3/CP2 | LP CP2 Vpp | LP CP3 Vpp |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 0 | open / ∞ | 4.126 | 1.692 | 0.41003 | 1.038 | 1.648 |
| .25 | 450 kΩ | 3.991 | 1.636 | 0.40997 | .997 | 1.589 |
| .50 | 150 kΩ | 3.904 | 1.583 | 0.40543 | .959 | 1.534 |
| .75 | 50 kΩ | 3.858 | 1.531 | 0.39687 | .924 | 1.483 |
| 1 | ideal zero | 3.812 | 1.482 | 0.38870 | .891 | 1.435 |

For all five cases the HP **CP3** result remains below the printed **3.3 Vpp
lower boundary**. The original alpha-0 service HP gate remains `MISS`; it was
neither replaced with a CP2 check nor relaxed. The LP output stays within the
existing **1.2–3.2 Vpp CP3** band. The intermediate impedance values are
**synthetic scenarios**, not values established for SUB IC3/2.

The coarse **VR7 CP3-maximization surrogate**, run only at alpha=0 and 1 with
Qhp=Qlp=5, selected the **261.626 Hz** grid point in both cases: **1.621 Vpp
CP3 (NI open)** and **1.410 Vpp (ideal-zero NO1)**. These values are **not
upper bounds**. The preexisting frozen alignment at approximately 260 Hz
already predicts **1.692 Vpp** at alpha=0, larger than the coarse grid's
1.621 Vpp. Thus half-octave spacing is too coarse to characterize a local
amplitude maximum, and this diagnostic does **not** reproduce Yamaha's
physical VR7 maximization procedure.

At that coarse selected cutoff, the predeclared **VR8 Q surrogates** yielded:

| HP model Q (not a VR8 angle) | CP3 Vpp, NI open | CP3 Vpp, ideal-zero NO1 |
| ---: | ---: | ---: |
| .5 | .889 | .794 |
| 1 | 1.011 | .919 |
| 2 | 1.263 | 1.159 |
| 5 | 1.621 | 1.410 |
| 9 | 1.869 | 1.688 |

The model permits a large-Q increase but gives no verified mapping from VR8
position or actual IG00156 pin-6 current to these settings. Do not interpret
this discrete Q grid as a bound on true hardware resonance or a replacement
adjustment instruction. Complete per-point CP2/CP3 printouts are emitted by
the opt-in diagnostic:

```sh
./build-circuit/cs80_ig00156_nodal_tests tests/parity/tolerances-v2/ws1-voice.json --ni-sensitivity
```

It returned **33 offline finite-positive checks, zero failures**, without
exercising or changing the normal acceptance gate. Because the coarse VR7
sampling demonstrably misses the existing frozen-model local value, a
**separately preregistered local C1 refinement** is needed before drawing
anything further about narrow amplitude peaks; no parameter should be
adjusted retrospectively to reach the Yamaha band.
