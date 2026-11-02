# ADJ p11 NI-source boundary diagnostic: pre-registration, 2026-11-02

Status at pre-registration: **predictions not yet evaluated**. This is an
offline sensitivity probe of the existing ideal linear M-board model, not a
hardware voltage claim or a change to the registered service-point gate.

## Source and node identities

The original Yamaha CS-80 **M circuit diagram**, printed p41, KEC-4223-65,
places a separate **150 kΩ resistor between NI terminal 43 and the HP mixer
node**. The same node has a permanent 150 kΩ resistor to ground; FET6 and FET7
each connect their own 150 kΩ waveform resistor to it. The 5100×3300 image
SHA-256 is `1278f17ff05cd82b2b84153838bf95109d3f1ea1e864119de0a3f4a4af4f0080`:
[Yamaha M41](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/41%20-%20M%20Circuit%20Diagram.jpg).
The M11/12 board wiring, printed p44 KEP-NA03574-6Y, maps **NI(43)** to
**SUB NO1(39)**, while **CP2(46) ↔ CP7(53)** and **CP8(56) ↔ CP3(29)**:
[Yamaha M44](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/44%20-%20M11-12%20Circuit%20Board%20%26%20Wiring.jpg),
SHA-256 `3320dd2f625c79d7cca9b98abd5cade9d1b95477e6d8990a93a296c9cda71256`.
The **SUB circuit diagram**, printed p29 KEC-4446-68, shows NO1(39) at the
IC3/2 unity-buffer output; it gives **no measured NO1 voltage or loaded output
impedance** in the CS-80 ADJ setup:
[Yamaha SUB p29](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/29%20-%20SUB%20Circuit%20Diagram.jpg),
SHA-256 `b81c7e5ad2927ac04d5912bfc455a631237f512a0c7700bda41250a14bd33e60`.

## Fixed scenarios, chosen before the result

Maintain the existing one-FET-on, one-FET-off pulse fixture, 3 Vpp square at
C1 2′ (261.6255653005986 Hz), 192 kHz, 3-second linear pair simulation,
last 0.2 s Vpp readout, existing HP/LP setup cutoffs and Q. Both scenarios
use the **unchanged physical filter stage models** and output taps.

| Case | Explicit hypothetical NO1 boundary | Effective mixer source resistance | Pulse source-to-mixer ratio |
| --- | --- | ---: | ---: |
| A: original fixture | NI branch **open** at NO1; no NI source loading | 75 kΩ | 1/2 |
| B: stiff zero | NI connected through its source-derived 150 kΩ to an **ideal zero-volt** NO1 buffer with zero output impedance | 50 kΩ | 1/3 |

Case B is a deliberately idealized **zero-signal endpoint**, not a claim that
the actual SUB NO1 buffer is silent or has zero output impedance. If the NO1
buffer has unknown passive real output impedance `Z_NO1 >= 0`, its zero-signal
loading lies between these ideal endpoints, with
`G_N = 1/(150k + Z_NO1)`, `R_th = 1/(1/150k + 1/150k + G_N)`, and pulse
gain `(1/150k) * R_th`. The drawing alone supplies neither `Z_NO1` nor NO1
waveform, so **no intermediate value will be selected, fitted, or called
source-derived**. Active waveform drivers and other attached M-board inputs
remain idealized; the theoretical interval is conditional on their stated
boundary assumptions.

For each case, report **both CP2 (HP pin 15 output)** and **CP3/CP8 (LP pin 15
output, M44 harness)** in the HP and LP service setups, with explicit
`NOT hardware` qualification. Preserve the original CP3 band and baseline
verdict: HP **4.3 ± 1 Vpp**, LP **2.2 ± 1 Vpp** at CP3. Only finite/positive
checks may be added for the *non-gating* alternative calculation. Do not touch
production DSP, source amplitude, physical passband normalization, V1
fingerprint, existing numeric controls, or tolerance JSON. After this source
and pre-registration record is committed, run the variant and publish the
unselected outputs, even if they worsen the mismatch.

## Evaluation after pre-registration, 2026-11-02

The complete source reading and the two unselected ideal scenarios above were
committed first as `f8b0c9e`. Only afterward was the test-only comparison
implemented in `tests/Ig00156NodalTests.cpp`. The two setups use the same
preexisting control coefficients as the NI-open reference; the alternate
source impedance is **not** used to retune cutoff or Q. Yamaha service bands
apply to CP3, not the separate diagnostic CP2 tap.

| Yamaha ADJ p11 setup | Ideal source boundary | CP2: HP stage (Vpp) | CP3/CP8: LP-stage terminal (Vpp) |
| --- | --- | ---: | ---: |
| HP: FH 4.0 V, RESH high | A: NI open | **4.126** | **1.692** |
| HP: FH 4.0 V, RESH high | B: NO1 ideal zero | **3.812** | **1.482** |
| LP: FL 3.3 V, RESL high | A: NI open | **1.038** | **1.648** |
| LP: FL 3.3 V, RESL high | B: NO1 ideal zero | **0.891** | **1.435** |

These are **simulated, linear, ideal-filter voltages** only, not Yamaha or
restorer scope readings. Case B moves HP **CP3 downward** by 0.210 Vpp, away
from the manufacturer's **3.3–5.3 Vpp** at CP3. Even the alternative
ideal-source limit therefore **does not explain the ADJ HP miss**. Its CP2
value happens to fall inside the numerical CP3 band, but this is no evidence
to substitute the probe or alter the target. Both LP CP3 values remain within
the original 1.2–3.2 Vpp band. No finite `Z_NO1`, nonzero noise waveform,
FET on/off resistance, downstream loading, or VR7/VR8 adjustment law was
measured or inferred from these numbers.

The nodal executable reports **122 checks, zero failures**, including the
unchanged full-band gate and original `ADJ p11 HP point: MISS`; the additional
alternative has only finite-positive checks. Command:

```sh
./build-circuit/cs80_ig00156_nodal_tests tests/parity/tolerances-v2/ws1-voice.json
```
