# PRA wah: conditional small-signal reference design

2026-10-29. **Source graph, conditional AC implementation and independent nodal tests reviewed. No production integration.** This is a bounded Class A reference, not a WI→WO transfer claim and not pedal calibration. Read [foot evidence](FOOT_CONTROL_EVIDENCE.md) and [routing evidence](OE1_SIGNAL_ROUTING_EVIDENCE.md) for the established placement and remaining hardware gaps.

## Boundary and assumptions

The reference output is IC2 half 2 pin13, **before FET2**, named Y below. It is not WO(65). FET1/2, their gate circuits, switch control and the downstream OE1 load are outside the reference. No switch resistance is inferred. A caller explicitly specifies the omitted dry-branch load at X; that load is a fixture boundary, not a reconstructed FET network.

Use four ideal op amps: infinite open-loop gain/input impedance, zero output impedance, no saturation or bandwidth limit. External compensation capacitors and supply-decoupling circuitry therefore do not participate. Their omission is a declared approximation, not evidence that the physical components are irrelevant. All capacitors below are ideal, all resistors linear. Optical resistance is supplied directly and held fixed for each AC solve. No EI→resistance, travel→EI, hysteresis, lamp response, noise, temperature or tolerance law is supplied.

The offline API accepts: frequency in Hz, complex source voltage, explicit source resistance (nonnegative), explicit passive dry load admittance at X, positive optical resistance, positive unresolved parallel resistance, effective VR1 rheostat resistance [0,25 kΩ], effective VR2 feedback resistance [0,100 kΩ]. It returns node phasors and source/load/output currents. A passive complex load is explicitly supplied at ideal output Y for load-current accounting; by assumption it does not change Y voltage. An open fixture boundary must be explicit, not hidden as an installed load default. Every boundary field defaults to an invalid NaN sentinel, so omitted or partially supplied fixtures fail validation rather than silently becoming ideal sources or open loads. The zero-ohm VR2 endpoint uses its limiting output voltage and feedback current, without division by zero. Transient/discrete-time implementation is deferred.

## Native graph transcription

Ground means the small-signal reference. Component names below are local descriptive names, not invented Yamaha reference designators.

| Connection | Value / evidence |
|---|---|
| WI(23) → X | 10 µF /16 V NP coupling capacitor |
| X → ground | 10 kΩ |
| X → N | 10 kΩ |
| N → IC1A− pin5 | 10 kΩ |
| A=IC1A output pin2 → IC1A− pin5 | 10 kΩ |
| IC1A+ pin6 → ground | 10 kΩ |
| C=IC2A output pin2 → N | 470 Ω feedback branch, **not 470 kΩ** |
| A → IC1B− pin10 | 10 kΩ |
| B=IC1B output pin13 → IC1B− pin10 | 10 kΩ |
| A → T | photoresistor in parallel with fixed resistor of **unlabelled value on PRA53** |
| T → P=IC1B+ pin9 | VR1, B25k, drawn as a two-terminal variable resistance |
| P → ground | 0.0022 µF (2.2 nF), corroborated by PRA54 board label |
| B → IC2A− pin5 | 10 kΩ |
| C → IC2A− pin5 | 10 kΩ |
| B → Q=IC2A+ pin6 | 10 kΩ |
| Q → ground | 0.033 µF (33 nF), corroborated by PRA54 board label |
| C → D | 1 µF /50 V NP coupling capacitor |
| D → ground | 10 kΩ |
| D → IC2B− pin10 | 10 kΩ |
| Y=IC2B output pin13 → IC2B− pin10 | VR2, B100k, feedback rheostat |
| IC2B+ pin9 → ground | 10 kΩ |

The original schematic also connects X to the dry FET1 branch and Y to FET2. Those connections are deliberately cut at the declared reference boundary; no claim that their physical loads are zero is made.

### Unresolved fixed resistor

PRA53 visibly draws a resistor parallel to the photoresistor without a readable numerical label. PRA54 shows `470K` beneath PH3 adjacent to the IC1 area. This is a **candidate** value requiring independent pad/trace reconciliation, not yet an accepted assignment. The API must require an explicit value if that reconciliation remains open. A caller-supplied value makes the arithmetic conditional; it does not close source evidence.

## Conditional equations (graph independently reviewed)

Let R=10 kΩ, r=470 Ω, Rt=VR1+(Ropt || Rparallel), t1=Rt·2.2 nF, t2=10 kΩ·33 nF, s=j2πf. Then the ideal amplifier relations are:

```text
A = −N
P = A/(1+s t1)
B = 2P−A
Q = B/(1+s t2)
C = 2Q−B
(N−X)/R + N/R + (N−C)/r = 0
s·1 µF·(D−C) + D/R + D/R = 0
Y = −(VR2/R)·D
```

Thus the two middle sections are all-pass sections in this idealization, embedded in the feedback around the first inverter, not a generic standalone band-pass:

```text
F = [(1−s t1)/(1+s t1)] [(1−s t2)/(1+s t2)]
C/X = −F / [2+(R/r)(1+F)]
```

For source Vs with impedance Zs=Rs+1/(s·10 µF), and supplied dry boundary admittance Ydry:

```text
(X−Vs)/Zs + X/R + (X−N)/R + Ydry·X = 0
```

At DC the coupling capacitors are open; evaluate the corresponding circuit limit explicitly rather than dividing by zero. Closed-form factorization must be tested against independently assembled nodal equations; matching two copies of the same formula is insufficient. Test source loading, dry-load dependence, optical/trimmer sensitivity, coupling-capacitor limits, the 470 Ω feedback path, output inversion, and node KCL. These prove only consistency with this graph and ideal assumptions.

## Provenance

Native source images already available locally were inspected; no new network retrieval was needed. Source hashes, URLs and dimensions are preserved in [routing evidence](OE1_SIGNAL_ROUTING_EVIDENCE.md). PRA53 source SHA-256 is `a691e710b59deecace01a62b57587e6b939e1084bfd71003efe71929bbe42c61`; PRA54 is `88703d2b43189003e3276ca7837556ef2ac5205dd057d8f5b0c6b320e1042c69`. Both are 5100×3300 pixels. Additional unresized PNG crops are under `artifacts/w7-routing-20261029/`; coordinates are source-native `[left,top,right,bottom]`.

| Crop | Source / rectangle | PNG SHA-256 |
|---|---|---|
| `pra53-wah-left-native.png` | PRA53 `[1700,1000,2630,1510]` | `c63e030a79c24989da84ce1a0d7cd7a21156009cf60fb11dceba928d71580f95` |
| `pra53-wah-middle-native.png` | PRA53 `[2550,970,3330,1490]` | `92756857973fde7b238e736ff3234a0d1a49486d9737b8cecf7025c617243c99` |
| `pra53-wah-right-native.png` | PRA53 `[3200,850,3850,1450]` | `c60031b2a147478994935984be41461fad21c8344b865552c0d58ed2e221a106` |
| `pra53-wah-rc-native.png` | PRA53 `[2200,1150,2520,1380]` | `aef45a57f1cdf2c9a89c1597e0fa864892e59972b7f62c83afb357e249c73170` |
| `pra54-wah-board-native.png` | PRA54 `[1100,1900,2230,2530]` | `48c461abaa1fb691b79349ecce556a96e0e9fd3af387cc88e77eea5de178f8d7` |

Machine-readable duplicate: `wah-reference-provenance.json` in that artifact directory. Review must check the 470 Ω return junction, 2.2 nF cap, VR1 rheostat connection, and candidate 470 kΩ parallel branch before any nominal component claim is accepted.

## Implementation and verification

`src/reference/PraWah.h` is a header-only offline AC solver, with no production engine dependency. `cs80_pra_wah_tests` runs 28,686 checks: a separately assembled 15-node modified nodal circuit stamps individual resistors, capacitors, four ideal amplifiers and zero-ohm trim constraints. It compares all nine returned node phasors and currents, including the final amplifier current from the independent MNA solution. It does not reuse the implementation's all-pass factorization.

Coverage includes frequency/optical/source/trim sweeps, DC, explicit passive complex loads, source and dry-branch loading, ideal-output load independence, complex linearity, endpoint and high-frequency limits, KCL and fail-closed missing/invalid/extreme inputs. Test fixture resistances and voltages are deliberately synthetic, not measured hardware values. Numerical tolerances test equation agreement; they are not analog acceptance tolerances.

Independent review also checked 500,000 KCL residuals over 100,000 randomized fixtures. Small subtractive currents require combined absolute/relative comparisons. Unrepresentable derived coefficients are rejected; the API does not promise unlimited floating-point range.

Nine targeted mutations were detected: the 470 Ω return changed to 470 kΩ, each shunt capacitor changed, output inversion removed, dry loading omitted, either coupling capacitor changed, output shunt omitted and VR1 omitted. This establishes consistency with the declared graph, not a measured instrument.

The complete WI→WO path still requires FET switching/loading, optical control and dynamics, real amplifier behavior, the unlabelled resistor's identity and calibrated captures. The audible wah remains the existing provisional model.
