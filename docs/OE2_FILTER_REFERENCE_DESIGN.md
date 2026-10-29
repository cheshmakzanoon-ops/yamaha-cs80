# OE2 active RC filters: conditional small-signal design

2026-10-29. **Source graph, equations, offline implementation and independent nodal tests reviewed. No production integration.** This is an offline linear reference, not a calibrated OE2, chorus or BBD model. It follows [OE2 circuit evidence](OE2_CIRCUIT_EVIDENCE.md) and W7 of the parity roadmap.

## Scope and explicit assumptions

The three low-pass sections can be solved without inventing a BBD or clock law. Model their HA1452D amplifiers as ideal voltage-controlled voltage sources: infinite input impedance and gain, zero output impedance, unlimited bandwidth and voltage/current range. All resistors and capacitors are ideal. These assumptions deliberately omit the physical IC's compensation, supply dependence, saturation and noise; they do not establish that those effects are inaudible.

Use small-signal perturbations around an unspecified operating point. Every supply/reference used below is assumed stiff at AC zero. This does not calculate or validate that operating point. The caller supplies the excitation and source resistance, and explicitly supplies any omitted output-load admittance (zero must be explicitly selected). Missing fields must default to unknown and fail validation, never to an installed ideal source or open load.

Two bounded entry points:

1. **Input filter:** source at OE2 I(53), through caller-supplied Thevenin resistance, terminating at IC3 output pin2 **before** its coupling/bias network into MN3001. No MN3001 input loading or bias is inferred. An explicit load at ideal pin2 can be used for output-current accounting, not an alteration of the ideal voltage response.
2. **One reconstruction branch:** source at the already-combined BBD output-pair node, with caller-supplied Thevenin voltage/resistance, through the documented 1 µF capacitor and post-capacitor 100 kΩ shunt, through its low-pass and subsequent AC-coupled inverting amplifier. Terminate at IC5 pin2 or IC6 pin13 **before** the 1 µF NP output capacitors feeding the parallel VCAs. The source boundary includes everything upstream of the 1 µF reconstruction coupling capacitor; it is not inferred from normalized packet values.

The two 2.2 kΩ tap-summing resistors, the pre-coupling 100 kΩ load, actual tap source impedances and their clock-dependent voltage waveforms are outside entry point 2. They must eventually be solved to obtain its Thevenin boundary. In particular, a normalized MN3001 packet is not a voltage, and summing two ideal packet values is not this loaded boundary.

## Native node graph

Local names U, A, B, Y, Z, V are mathematical labels, not Yamaha reference designators.

For each low-pass:

```text
U -- R1=10k -- A -- R2=10k -- B (non-inverting input)
               |             |
            Cf=1.8nF       Cg=680pF
               |             |
               Y           AC reference
Y = ideal follower output, directly fed back to inverting input
```

Cf connects A to **output Y**, not directly to the high-impedance input B. Although ideal feedback imposes Y=B in voltage, they are different current ports. A nodal implementation must not add Cf current to the KCL equation at B.

| Section | Non-inverting B | Inverting follower input | Output Y |
|---|---|---|---|
| IC3 input LP | pin6 | pin5 | pin2 |
| IC5 upper reconstruction LP | pin9 | pin10 | pin13 |
| IC6 lower reconstruction LP | pin6 | pin5 | pin2 |

The reconstruction entry includes `Vs -- Rs -- Cc=1µF -- U`, with U connected to the small-signal reference through 100 kΩ. The first 10 kΩ is between U and A. Source Rs is a fixture parameter, not a claim that the BBD output resistance is constant or purely resistive over the audio/clock range.

After each reconstruction LP, Y connects through **0.1 µF** and **10 kΩ in series** to the inverting input of the second amplifier. Its non-inverting input is referenced through 10 kΩ, with the drawn 33 µF capacitor; zero ideal input current and stiff AC reference make that input zero in this idealization. Feedback is 39 kΩ for IC5 and 100 kΩ for IC6:

| Branch | LP Y | Second amplifier inverting input | Final V | Feedback |
|---|---|---|---|---|
| Upper | IC5 pin13 | IC5 pin5 | IC5 pin2 | 39 kΩ |
| Lower | IC6 pin2 | IC6 pin10 | IC6 pin13 | 100 kΩ |

The final outputs feed their respective pairs of IG00151 branches through separate 1 µF NP capacitors and 100 kΩ input resistors, as recorded in the OE2 ledger. Those VCA paths are outside this proposal. Neither second-stage gain is a frequency-independent constant down to DC because its **0.1 µF input coupling capacitor** remains in the graph.

## Equations for independent review

Let `s=j2πf`, `g1=1/R1`, `g2=1/R2`. For a prescribed U:

```text
(A-U)/R1 + (A-B)/R2 + s Cf (A-Y) = 0
(B-A)/R2 + s Cg B = 0
Y = B
```

Eliminating A gives the unity-follower Sallen-Key transfer:

```text
H(s) = Y/U
     = 1 / [1 + s Cg (R1+R2) + s² R1 R2 Cf Cg]
```

This is the transfer for a prescribed U only. A finite driving impedance changes the response; it must not be multiplied onto this result as an unrelated unloaded divider.

For input-filter source resistance Rs, no upstream shunt or coupling capacitor is present inside the declared boundary. Rs can be combined with R1 in the equations because nothing is connected to their intervening source port. Thus use `R1eff=R1+Rs` in H and reconstruct port voltage/current afterward. At Rs=0, explicitly constrain U=Vs instead of forming an infinite conductance.

For reconstruction entry, define `ys=s Cc/(1+s Rs Cc)` and `gb=1/100k`. Solve the three-node linear system:

```text
(ys+gb+g1) U - g1 A                          = ys Vs
-g1 U + (g1+g2+s Cf) A - (g2+s Cf) B        = 0
                     -g2 A + (g2+s Cg) B   = 0
Y = B
```

At f=0 the coupling capacitor is open and these perturbation nodes are zero. Handle DC directly and avoid `1/s`. This source/load model is conditional; a frequency-dependent BBD source or non-stiff bias network requires expanding the boundary, not silently choosing an Rs.

For the second amplifier, let Ci=0.1 µF, Ri=10 kΩ, and Rf as tabulated. With Z the node after Ci:

```text
s Ci (Z-Y) + Z/Ri = 0
V/Rf + Z/Ri = 0
V/Y = -Rf s Ci / (1+s Ri Ci)
```

The asymptotic gain magnitudes are 3.9 and 10, not their DC gains. Coupling corner `1/(2π Ri Ci)` is about 159.15 Hz under these ideal assumptions; it is a derived nominal stage quantity, not a measured installed-system cutoff.

If load currents are returned, account for follower current into Cf **and** the second amplifier's input path (where present), plus the explicitly supplied extra load. Final amplifier current includes feedback current and the supplied output load. The ideal output load cannot change voltage by construction; this is an assumption to test, not a hardware finding.

## Numerical acceptance tests

- Independent modified-nodal graph with explicit ideal amplifier constraints, compared with the factored equations across a frequency/source-resistance grid. Do not compare two copies of H.
- KCL at A, B, reconstruction U and interstage Z; explicit current signs and source/output-current budgets.
- DC: input LP unity transfer for finite Rs and zero input current; reconstruction and second-amplifier outputs zero because their coupling capacitors open.
- High-frequency second-order LP rolloff and second-amplifier gain limits; no single-cutoff substitution.
- Source-resistance sensitivity, reconstruction 100 kΩ loading, both unequal feedback branches and explicit output loading.
- Mutation detection for swapping Cf/Cg, moving Cf onto B's current node, removing each coupling capacitor, dropping the 100 kΩ shunt, equalizing 39 kΩ/100 kΩ, and reversing the second amplifier sign.
- Missing, nonfinite, negative and unrepresentable boundary rejection; no partial state because the proposed solver is stateless. Negative frequencies are invalid. No empirical tuning to make a plot look plausible.

The source graph has passed independent review; implementation and tests still require independent review. Numerical tests would establish agreement with these declared ideal equations, not with an actual HA1452D or a CS-80.

## Evidence and provenance

Inspected the native OE57 source and the two existing unresized crops below; additionally used temporary native subcrops for reading the interstage paths. No new source acquisition or copyrighted-source publication was needed. Original dimensions are 5100×3300; rectangles use native `[left,top,right,bottom]` coordinates.

Source: [Yamaha OE2 schematic, service page57](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/57%20-%20OE2%20Circuit%20Diagram.jpg), SHA-256 `9d3060b05899526469cbff8e65e5abbde116230fb4951580d9abd25902db72a9`.

| Existing local crop | Rectangle | SHA-256 |
|---|---|---|
| `oe57-input-bbd.png` | `[1550,650,2650,2600]` | `13d85f9b8bb20bd406a1489831f34656c033cc8a4e19557cac82baf4780b0e34` |
| `oe57-filters-vcas.png` | `[2650,650,4750,2950]` | `7df1a04b4a38eef619222b011dfc4c0f19a26fbffdb4a87e809d8b7ab22044f2` |

These originals/crops are in `artifacts/w7-oe2-20261029/`; their hashes were rechecked for this pass. Existing OE2 evidence establishes the identities HA1452D, MN3001 and IG00151. This design makes no new manufacturer bandwidth or pin-law claim. Independent source review confirmed the follower connections, the 0.1 µF paths, the reconstruction-side 100 kΩ shunts, signal/reference junctions, crop hashes and equations.

## Offline implementation API

`src/reference/Oe2Filters.h` uses namespace `cs80::reference::oe2_filters`. It has no production-audio dependencies and no state or time integration. `solveInput(InputBoundary, frequencyHz, sourceVolts)` and `solveReconstruction(ReconstructionBoundary, frequencyHz, sourceVolts)` return complex node voltages and signed branch/output currents. Voltages are volts, currents amperes, resistance ohms, load admittance siemens and frequency Hz.

Every boundary member defaults to NaN or `Branch::unknown`, including omitted complex loads. Callers must explicitly select `Branch::upper` or `Branch::lower`, source resistance, and every omitted load. Negative resistance/frequency, negative real load admittance, nonfinite values and unknown branch selections are rejected. A nonnegative real admittance is a pointwise passivity restriction, not a claim of a causal load model across frequencies. Derived overflow/nonfinite or zero denominators are rejected. DC and zero source resistance are handled explicitly without infinite conductances.

The returned current signs are positive from each driving source/output into its attached network. Reconstruction `followerLoadSiemens` is an **extra** load, excluding the already modeled Ci/Ri interstage branch; final `outputLoadSiemens` is attached before the excluded VCA coupling capacitors. Source Thevenin values and phasors must not be filled with normalized MN3001 packet units.

The factored implementation avoids subtracting nearly identical coefficients for the filter input current near DC. It reconstructs the input port from local voltage drops at very large source resistance, and obtains reconstruction source current from KCL to retain its small quadrature component at very high frequencies. These are numerical-conditioning protections, not extensions of the physical small-signal model's validated frequency range. It is not a physical large-signal, transient or discrete-time audio solver. An initial C++20 compile with `-Wall -Wextra -Wpedantic -Werror` passed; independent implementation and test review passed.

## What remains outside the design

BA617 and clock-buffer waveform laws, BBD operating point and charge transfer, tap voltage/current laws, clock feedthrough, op-amp compensation and limits, all IG00151 laws/trims, supply/reference impedance, OE1 mixing, temperature/tolerances, switching transients, and physical captures remain open. No production chorus or preset should be replaced by this small-signal submodel alone.

## Numerical regression checkpoint

`cs80_oe2_filters_tests` runs 10,190 checks against independently stamped modified nodal circuits, including physical follower output-current ports rather than treating equal-voltage input/output nodes as a shared current node. Both reconstruction branches and solved source/amplifier currents are compared. Synthetic boundary fixtures test circuit consistency only.

Nine mutations were detected: Cf/Cg swapped; follower feedback-capacitor current omitted; reconstruction input coupling bridged; interstage coupling bridged; 100 kΩ shunt omitted; upper feedback gain equalized with lower; inverting sign reversed; and each of the two cancellation-prone expressions restored. The last two each fail a dedicated scale-sensitive regression. These numerical checks are not analog acceptance tolerances and do not justify substituting the model into the production chorus.
