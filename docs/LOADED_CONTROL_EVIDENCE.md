# Loaded cutoff-control networks: conditional offline DC experiments

2026-10-29. W1/M2 document-derivable increment. **No instrument or captures were measured; no calibration, analog parity, complete rank-network or audible-behavior claim.** `src/reference/LoadedControl.h` does not drive the engine. It solves one R51/R52 voice's FH/FL pair into the M-card cutoff pins under explicitly supplied electrical boundary conditions. It is not a complete R51/R52 board simulation. The later `LoadedControlRank.h` extension below includes all eight cutoff pairs and their four common buses, still under explicit conditional source/receiver boundaries.

## Primary-source provenance

Sources fetched directly and visually inspected at native pixel resolution. All original JPEGs are 5100×3300. Existing evidence ledgers independently record the same original-byte hashes; this inspection does not depend on OCR. Files remain temporary, not redistributed in the repository.

| Source | URL | Original SHA-256 |
|---|---|---|
| R39 | [Service p39, R51–52](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/39%20-%20R51-52%20Circuit%20Diagram.jpg) | `b77875e1aee0e092c320f8dea69ec7b9d975d4b3551eaa2cdabc597886c80156` |
| M41 | [Service p41, M circuit](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/41%20-%20M%20Circuit%20Diagram.jpg) | `1278f17ff05cd82b2b84153838bf95109d3f1ea1e864119de0a3f4a4af4f0080` |
| M44 | [Service p44, M11–12 wiring](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/44%20-%20M11-12%20Circuit%20Board%20%26%20Wiring.jpg) | `3320dd2f625c79d7cca9b98abd5cade9d1b95477e6d8990a93a296c9cda71256` |

Native crops made using Pillow `Image.open(...).convert('L').crop(box).save(...png)` (default PNG options; hashes differ from historical optimized PNGs of identical pixels). Boxes are `[left,top,right,bottom]`, right/bottom exclusive:

| Source / crop | Box | PNG SHA-256 |
|---|---|---|
| R39 first-voice cutoff branches | `[2700,450,3590,900]` | `c39704dec8db2cac18aa0479ba5339849e71eb8c1f0eef6205b205044c835488` |
| M41 filter pins, shunts, trims | `[1180,1080,3250,1850]` | `1ccb5f8792e6a96f879cabfc8bb6cb6b2a12345184532ea969fe7c394a0f92ca` |
| M41 TP1 envelope branches | `[1200,1580,2130,2100]` | `dd020e94bd7168b611062c4f764f4ae34b1dc52c9808b46a37a486bfa005b029` |
| M41 complete CS80 grade table | `[3900,2500,4750,3100]` | `4e73c212be874aefbf87a0b917949d0f080d3acf8fc86aee8a813a0e64ff14a7` |
| M44 FH1/FL1 wiring | `[1500,1530,2250,1940]` | `0dcdee70645acfbce9566f703fde2c0344b69220f3887ad6165c4e80c1fe75d2` |

Independent review status: a separate reader checked the native circuit crops, component identities, pin wiring, grade table, source and regenerated crop hashes, two-node elimination and KCL signs; no blocking discrepancy found. This remains document and software evidence, not hardware measurement.

## Transcription and limits

| Endpoint | HP branch | LP branch | Evidence |
|---|---:|---:|---|
| Each of KC, TF, AB, IB, SUB | 120 kΩ | 39 kΩ | R39 |
| BR | 680 kΩ | 220 kΩ | R39 |
| Master FH / FL | 39 kΩ to HP only | 39 kΩ to LP only | R39 |
| TP1 filter envelope | 100 kΩ | 47 kΩ | M41 |
| VR7 / VR9 trim wiper | 150 kΩ | 150 kΩ | M41 |
| Pin-4 grounded shunt, IC grade A/B/C | R6: 2.7/2.2/1.8 kΩ | R7: 3.3/2.7/2.2 kΩ | M41 **CS80** column |
| Pin-4 capacitor to ground | 0.01 µF | 0.01 µF | M41; open at settled DC |

M44 explicitly connects R51 FH1 terminal 74 to M11 F1 terminal 51, and FL1 terminal 75 to F2 terminal 55. M41 joins f1/f2 directly to the respective IG00156 pin 4. Thus this is a connected control-pin calculation, not multiplication of isolated resistor ratios. IC grade A/B/C is **not** synthesis rank I/II. Grades may be different for the two chips.

The 100 kΩ VR7/VR9 pots span +15/−15 V in the drawing. `trimWiper(low,high,x)` derives `Voc=low+x(high-low)`, `Rth=100000*x*(1-x)` for *ideal, supplied fixed rail voltages*. Position is electrical resistance fraction, not an asserted slider taper. The fixtures explicitly choose ±15 V; supply regulation, wiper contact resistance and actual trim settings remain unknown.

`Inputs.shared` means sources forked to this voice's **two HP/LP nodes**, not seven rank-common buses: KC/TF/AB/IB are per-voice inputs, SUB/BR are common buses, and TP1 is the local M-card envelope output. FH/FL are common cutoff buses but each feeds only its own filter side.

Every source's open-circuit voltage and source resistance must be supplied. There is no guessed zero-ohm driver. Every receiver must supply `I_into_chip=G*Vpin+I0`, including an explicitly requested open receiver `G=I0=0`. This affine load is a conditional experiment boundary, **not an established IG00156 input law**. Unknown boundaries default to NaN and are rejected. Source resistances and receiver conductances must be finite/nonnegative; arbitrary nonlinear/active negative-resistance boundaries are outside this solver.

**Single-pair shared-bus exclusion (addressed only within the conditional rank extension below):** real SUB, BR, FH and FL feed other voices, and source circuitry can connect additional loads. This model does not include those seven other voices or multiport driver coupling. A single fixed independent Thevenin source is not generally equivalent to that complete network. Use the fixture for isolated pair sensitivity/KCL experiments only; do not treat a result as the whole instrument's voltage. The rank extension below includes eight-voice cutoff-bus loading; upstream source graphs remain W1/W9 work.

## Equations and useful distinction

For one source voltage `E`, source resistance `Rs`, and fork conductances `gh=1/Rh`, `gl=1/Rl`, the terminal voltage is

`Vs = (E + Rs*(gh*Vh + gl*Vl)) / (1 + Rs*(gh+gl))`.

Each control-node KCL includes `(Vnode−Vs)/Rbranch`, grounded-shunt current, trim and master-cutoff branch currents, and the explicit signed chip current. Eliminating each shared source produces a symmetric 2×2 matrix. Its off-diagonal contribution is `−Rs*gh*gl/(1+Rs*(gh+gl))`; it vanishes only for an explicitly ideal source. Two independent voltage dividers would miss this coupling. No capacitor is used to invent a time constant; no physical pin voltage is passed to a presumed volts/octave conversion.

Unknowns still blocking promotion: actual source impedances (including presets/PANEL/memories), inter-rank and upstream loading, chip input-current law, trim state, supply/reference behavior, temperature/tolerances, dynamic acquisition and physical-pin-to-effective-cutoff convention. The service FH/FL voltage and CP3 amplitude alignment gates require the actual audio network and hardware evidence; this fixture cannot satisfy them.

## Software verification only

`tests/LoadedControlReferenceTests.cpp` checks 8,132 assertions: ideal-source closed-form divider, unequal HP/LP excursion, finite-source back-coupling, independent literal-component node/source KCL across all nine grade pairs and 100 synthetic boundary sets, trim equivalents, signed receiver currents, and rejection of unknown/invalid boundaries. These are numerical tolerances for synthetic algebra, not preregistered analog tolerances.

Standalone execution (no JUCE, production changes or CMake target required):

```sh
c++ -std=c++20 -Wall -Wextra -Wpedantic -Werror -Isrc \
  tests/LoadedControlReferenceTests.cpp -o /tmp/cs80-loaded-control
/tmp/cs80-loaded-control
```

Local strict-warning build and ASan/UBSan execution passed (leak detection disabled; no leak-clean claim). Three isolated mutation builds changed a 120 kΩ branch, reversed the source-coupling sign, and reversed receiver-current sign; each failed its corresponding regression. This is software verification, not calibration.

## Eight-voice conditional rank extension

`src/reference/LoadedControlRank.h` now models **16 cutoff-pin nodes on one eight-voice board**, not the complete instrument or complete R51/R52 functions. `RankInputs.voices[n].local` contains KC, TF, AB, IB, TP1 for that voice. `RankInputs.buses` contains exactly **one** source each for SUB, BR, FH, FL; their fanout is not replaced with eight independent source copies. Each voice has its own two trim equivalents, chip grades, grounded shunts and explicit signed chip-current boundaries.

### Additional native evidence

R39's continuous vertical SUB/BR/FH/FL lines connect all eight resistor rows; KC/TF/AB/IB are separately voice-indexed. Native upper/lower crops retain an overlap to trace common-bus continuity:

| Crop | Box | PNG SHA-256 |
|---|---|---|
| R39 rank upper | `[2700,450,3590,1750]` | `235e57c0dce00299cd1faa48615bfdd15fde2bb69ac7191e1769e3f57bf09965` |
| R39 rank lower | `[2700,1650,3590,3000]` | `bf0d47d248c48ecd7ada15b2a8a8ac5506437da488c92ed6ffb03ef81bbfff13` |

M44–47 explicitly join every R51 FH1…FH8 to M11…M18 F1 terminal 51, and every FL1…FL8 to F2 terminal 55. Connector numbers with primes must not be collapsed: for example FH4 uses 80′, FL4 uses 80. These pages establish **rank-I wiring only**; this model does not invent any connection to the second rank or assert independence of the actual upstream drivers.

| Additional source | Original SHA-256 |
|---|---|
| [M45, M13–14 wiring](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/45%20-%20M13-14%20Circuit%20Board%20%26%20Wiring.jpg) | `32545b5c15ecc7bd038901af9880da20905103dd7413a34b3e1db5183ccf8627` |
| [M46, M15–16 wiring](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/46%20-%20M15-16%20Circuit%20Board%20%26%20Wiring.jpg) | `197cc76404db321de26f06a8e187e6b33b0690a14afe45eedf56744180c6632c` |
| [M47, M17–18 wiring](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/47%20-%20M17-18%20Circuit%20Board%20%26%20Wiring.jpg) | `2824471b66f408e70c6ef479f2d11bcda01d965bc3b0e34fea5fbfba9b62f56e` |

Originals are 5100×3300; native grayscale PNG procedure is as above. M44 original hash was already recorded above.

| Wiring crop | Box | PNG SHA-256 |
|---|---|---|
| M44 voice2 | `[550,1550,1350,1940]` | `c87fcdbb4444db132a7c38b49c947c1b1b789127f3a0b761b79bcbeecc4e02fa` |
| M45 voice3 | `[900,1580,1650,1940]` | `7bc6ea1eeb2c5bf01251d4a17de4968777953281b2d277054598e8564041b1f8` |
| M45 voice4 | `[2900,1580,3670,1970]` | `4513324a4a3a29cc3c50f253e58e74b4785fb807d1779e7080d5e5eff67981c4` |
| M46 voice5 | `[650,1550,1400,1940]` | `9ac8489b5d5d9cc7b27a7633f761f360be2d76fae9ef4176c2a4fab0b363971a` |
| M46 voice6 | `[2700,1570,3450,1950]` | `b81892b103bc6ecd639c98831a281e3531b63856477388b4c0f0691db2b15ac1` |
| M47 voice7 | `[850,1580,1620,1950]` | `27f3661214d82d7938adb0d3c06c20740e8b2265630bbd6b2e49863675d22a37` |
| M47 voice8 | `[2900,1580,3660,1950]` | `109ebed80f1b8be15e09dbaf8b3c91204febbd4a83afc132a895ab6028525501` |

### Rank equation, tests and remaining boundaries

For source `E,Rs` connected through conductances `g[i]` to multiple pins, let `S=sum(g)`, `z=1+Rs*S`. Eliminating its terminal adds

- diagonal `g[i]*(1+Rs*(S-g[i]))/z`;
- off-diagonal `−Rs*g[i]*g[j]/z`;
- right-hand side `g[i]*E/z`.

This preserves inter-voice back-coupling through finite-impedance buses. Positive grounded shunts and nonnegative receiver conductances make the assembled 16×16 matrix positive definite; Cholesky solves its settled-DC KCL. Reported source-terminal voltages allow independent current-balance tests. No dynamic or effective cutoff law was added.

`tests/LoadedControlRankTests.cpp`: **14,012 synthetic checks** pass strict C++20 and ASan/UBSan (leak detection off). Independent literal-resistor KCL checks every pin/local source/common source across 100 varied fixtures; tests cover symmetric voices, coupling through each common bus, zero-impedance bus isolation, comparison to eight independent pair solves only in that ideal-bus limit, and unknown boundary rejection. Under completely symmetric voices, full-rank output also matches a one-pair symmetry reduction with **eight times each common source resistance**—a mathematical test, not an asserted hardware resistance. Mutating the eighth bus fanout, coupling sign, or LP envelope resistor each produces a failed regression.

```sh
c++ -std=c++20 -Wall -Wextra -Wpedantic -Werror -Isrc \
  tests/LoadedControlRankTests.cpp -o /tmp/cs80-loaded-rank
/tmp/cs80-loaded-rank
```

Independent review status for this extension: source topology, all three added original hashes, all nine added native crop hashes, Schur elimination, Cholesky solve and tests independently reviewed; no blocking discrepancy found. Unknown receiver laws, upstream multiport/driver circuitry, actual source impedances, inter-rank source sharing, trims, rails, component tolerances and physical-pin-to-effective-cutoff conversion remain unknown. Each common bus is *conditionally represented* by an independent Thevenin source, not established as an independent physical source. No receiver has been defaulted to an open circuit. This extends document-derived passive loading only; it does not remove hardware-calibration gates.

## Conditional cutoff-node transient extension

`src/reference/LoadedControlTransient.h` adds a **numerical RC reference experiment** to the same eight-voice passive graph. It uses only the documented **0.01 µF = 10 nF capacitor from each cutoff pin to ground** (M41 native filter crop above). There is no additional assumed chip capacitance, upstream driver pole, transistor law, temperature law or measured settling parameter. All source resistances, independent-source boundary assumptions and affine static receiver currents remain prescribed by the caller, not calibrated.

The rank assembly and source-terminal reconstruction are now shared helpers in `LoadedControlRank.h`; its existing DC public API is unchanged. Prior single-pair 8,132 checks and rank 14,012 checks still pass. Assembly also rejects nonfinite computed matrix/forcing entries instead of allowing an overflowed boundary calculation into the transient state.

### Differential equation and integration

With the fixed conductance matrix `A`, time-varying forcing `b(t)`, and diagonal capacitor matrix `C=10 nF * identity`, the **conditional fixture equation** is

`C dV/dt + A V = b(t)`.

This is the external-node charge equation under the stated memoryless source/receiver assumptions, not the complete CS-80 transient equation. For step duration `h`, trapezoidal integration solves

`(C/h + A/2) Vnext = (C/h − A/2) Vprevious + (bprevious + bnext)/2`.

Both forcing endpoints are included. Source-terminal voltages are reconstructed algebraically from the new pin voltages and supplied source values; no hidden source charge is introduced. The constructor requires finite **explicit initial capacitor voltages and explicit initial source/current-offset values**. It never silently assumes zero or DC equilibrium.

### Events, configuration and failure semantics

- `step(seconds,nextInputs)` accepts finite positive durations and integrates using stored previous forcing and the supplied next endpoint. Boundary source voltages and receiver-current offsets may vary; source resistances, receiver conductances and IC grades must match the initial configuration exactly.
- `setForcing(inputs)` is an explicit **zero-duration forcing event**. It updates algebraic terminals and the stored starting forcing without changing capacitor voltages. For a step at the beginning of an interval, call this before integrating that interval; do not mistake endpoint-averaged forcing for an instantaneous start-of-interval step.
- Reconfiguration is deliberate: construct a new instance with the new configuration, chosen initial forcing, and explicitly chosen initial voltages. Supplying the previous state's pin voltages preserves capacitor charge. For example a real moving trim generally changes both voltage and source resistance; that is a configuration change, not automatically supported by voltage-only stimulus updates.
- Invalid/unknown initial voltages, unknown sources/receivers, invalid durations, changed topology, or nonfinite intermediate calculations throw `std::invalid_argument`. Failed stepping/events leave both state and forcing history unchanged. No unknown receiver becomes a default open circuit.

### Numerical verification and limits

`tests/LoadedControlTransientTests.cpp`: **1,882 checks**, strict-warning C++20 and ASan/UBSan pass (leak detection disabled). Tests use numerical error tolerances for the conditional equations, **not analog-fidelity tolerances**:

- Independent literal-resistor scalar decay against `exp(−G*t/C)` and approximately fourfold error reduction when halving the step size.
- Independently derived symmetric eight-voice 2×2 eigenmodes, analytic exponential decay, retained eigenvectors and second-order timestep refinement. The symmetric reduction includes eightfold common-bus loading, not eight disconnected pair copies.
- Variable-duration partitions checked against the exact scalar trapezoidal amplification product. Different finite partitions are **not** asserted bit-identical or analytically identical.
- Explicit source-step response, ramp response exercising previous/current forcing, and preservation of supplied DC equilibrium over multiple step sizes.
- Independent per-component resistor-current sums verify trapezoidal capacitor charge balance at all 16 pins across 100 changing-voltage/current-offset steps.
- Unknown/invalid boundaries and changed source resistance, receiver conductance, grade and trim impedance reject. A subsequent valid step is compared bit-exactly with an untouched instance to verify failed calls did not mutate state or forcing history.
- Three targeted mutation builds—doubling the node capacitor, omitting previous forcing, and removing the common-bus topology guard—each fail the corresponding regression.

**Accuracy warning:** trapezoidal integration is A-stable but not L-stable. Steps much larger than a fixture's decay time may alternate sign and poorly approximate decay even without numerical growth. A regression explicitly checks that coarse alternating mode; it is not a hardware oscillation claim. Choose and refine `h` against the declared stimulus and conditional eigenvalues; no universal audio sample rate, physical time constant or audible settling claim follows from this fixture. Real source/chip dynamics could change those eigenvalues substantially.

```sh
c++ -std=c++20 -Wall -Wextra -Wpedantic -Werror -Isrc \
  tests/LoadedControlTransientTests.cpp -o /tmp/cs80-loaded-transient
/tmp/cs80-loaded-transient
```

Independent review status for transient extension: equations, endpoint-forcing history, fixed-topology guards, charge-continuous events, exception atomicity, DC refactor and evidence limitations independently reviewed; all 8,132 + 14,012 + 1,882 checks independently rerun with strict warnings, no blocker found. No production engine/plugin path uses this model; hardware captures and calibration gates remain open.
