# Loaded cutoff-control pair: conditional offline DC experiment

2026-10-29. W1/M2 document-derivable increment. **No instrument or captures were measured; no calibration, analog parity, complete rank-network or audible-behavior claim.** `src/reference/LoadedControl.h` does not drive the engine. It solves one R51/R52 voice's FH/FL pair into the M-card cutoff pins under explicitly supplied electrical boundary conditions. It is not a complete R51/R52 board simulation.

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

**Shared-bus exclusion:** real SUB, BR, FH and FL feed other voices, and source circuitry can connect additional loads. This model does not include those seven other voices or multiport driver coupling. A single fixed independent Thevenin source is not generally equivalent to that complete network. Use the fixture for isolated pair sensitivity/KCL experiments only; do not treat a result as the whole instrument's voltage. Full eight-voice shared-bus and upstream source graphs remain W1/W9 work.

## Equations and useful distinction

For one source voltage `E`, source resistance `Rs`, and fork conductances `gh=1/Rh`, `gl=1/Rl`, the terminal voltage is

`Vs = (E + Rs*(gh*Vh + gl*Vl)) / (1 + Rs*(gh+gl))`.

Each control-node KCL includes `(Vnode−Vs)/Rbranch`, grounded-shunt current, trim and master-cutoff branch currents, and the explicit signed chip current. Eliminating each shared source produces a symmetric 2×2 matrix. Its off-diagonal contribution is `−Rs*gh*gl/(1+Rs*(gh+gl))`; it vanishes only for an explicitly ideal source. Two independent voltage dividers would miss this coupling. No capacitor is used to invent a time constant; no physical pin voltage is passed to a presumed volts/octave conversion.

Unknowns still blocking promotion: actual source impedances (including presets/PANEL/memories), all eight-voice and inter-rank loading, chip input-current law, trim state, supply/reference behavior, temperature/tolerances, dynamic acquisition and physical-pin-to-effective-cutoff convention. The service FH/FL voltage and CP3 amplitude alignment gates require the actual audio network and hardware evidence; this fixture cannot satisfy them.

## Software verification only

`tests/LoadedControlReferenceTests.cpp` checks 8,132 assertions: ideal-source closed-form divider, unequal HP/LP excursion, finite-source back-coupling, independent literal-component node/source KCL across all nine grade pairs and 100 synthetic boundary sets, trim equivalents, signed receiver currents, and rejection of unknown/invalid boundaries. These are numerical tolerances for synthetic algebra, not preregistered analog tolerances.

Standalone execution (no JUCE, production changes or CMake target required):

```sh
c++ -std=c++20 -Wall -Wextra -Wpedantic -Werror -Isrc \
  tests/LoadedControlReferenceTests.cpp -o /tmp/cs80-loaded-control
/tmp/cs80-loaded-control
```

Local strict-warning build and ASan/UBSan execution passed (leak detection disabled; no leak-clean claim). Three isolated mutation builds changed a 120 kΩ branch, reversed the source-coupling sign, and reversed receiver-current sign; each failed its corresponding regression. This is software verification, not calibration.
