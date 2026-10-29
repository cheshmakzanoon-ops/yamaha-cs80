# Level-control network: conditional loaded reference design

2026-10-29. **Native graph independently reviewed for a conditional offline DC reference. No production integration.** This bounded W1/W9 proposal solves the external R51/R52 level summing network under supplied electrical boundaries, not the IG00151 gain law or calibrated output loudness.

## Verified passive graph and sharing

R39, the common R51–52 schematic, gives each LC1–LC8 output five branches: AM through 180 kΩ, LV through 560 kΩ, and the corresponding TIj, TAj and KBj through 270 kΩ each. There are eight separate LC nodes per rank, not one level voltage copied eight times. Two ranks therefore contain 16 nodes and 80 resistor branches.

R40 explicitly joins R51-AM93 and R52-AM93, with PN2-VR11-2 on the latter wiring column. LV93′ remains separate per rank: rank I names T52-22(9)/MBK-M3-VR20, rank II T54-22(9)/MBK-M4-VR20′. Each KBj is wired between ranks and to KBC2-Oj; KB buses are distinct per voice. TI/TA ports have rank-specific TRG origins, not an assumed rank mirror.

A useful bounded fixture therefore has 43 source ports: one shared AM, two rank-local LV, eight shared KB, sixteen TI and sixteen TA. This is a count of explicitly supplied boundary ports, **not proof that all omitted upstream drivers are independent**. Their source resistance, reference voltage, nonlinear behavior and any upstream multiport coupling remain unknown.

## M-card receiver: exact boundary

R40 connects LCj to M1j/M2j LC terminal31. Native M41 traces LC31 directly to **IC5 LI pin2**, with 0.01 µF (10 nF) to the drawn reference. IC1–IC6 are listed as IG00151. The source itself labels IC5 “VCA (for SUB-OSC)”; this is not inferred from its position. Its signal input +IN3 receives the preceding path through 1 µF NP and 100 kΩ; output8 reaches CP5(32), a 27 kΩ shunt and the following IC6 stage. Thus LC is not silently substituted for IC1's control or a direct master-volume gain.

The proposed DC boundary ends at IC5 LI2 and its supplied input-current law. The 10 nF capacitor is open at settled DC. Its existence does not justify an invented pole/time constant with an ideal driver, or adding a transient implementation before the actual loading/state contract is specified. EI1 and its 100 kΩ/VR13 trim path remain a separate chip control, not a grounded shunt on LC.

## Explicit gaps and source conflicts

R40's rank-I source annotations name TRG2-L14(51) for both TI4 and TA4. However, the subsequent [reciprocal page20 review](TRG_LEVEL_RECIPROCAL_REVIEW.md) identifies TRG1-side L14(51) → R51-TI4(62), RE wire, and TRG2-side L14(51) → R51-TA4(62′), PK wire. Those colors agree with R40. This supports separate outputs and exposes a cross-page conflict in R40's TI4 upstream label; it is not evidence to short the two fixture ports, nor a physical continuity measurement. Preserve both readings. TI5 reads TRG1-L15(54), whereas TA5 reads TRG2-L15(54), corroborated by page20; that pair is not duplicated.

The fixture's TI/TA ports remain explicitly supplied conditional boundaries, not proof of fully independent physical drivers. Preset/memory source isolation, shared supply effects and the full KBC2/TRG output impedances remain outside this graph.

No current-to-gain mapping, input-current curve, saturation, noise, trim calibration, supply motion or component tolerance is inferred. A conditional reference must remain outside production audio.

## Conditional settled-DC equations

For output i=(rank,voice), let its attached set B_i contain AM, its rank LV, its voice KB, its individual TI and TA. Let g_ib be the reciprocal of the corresponding documented branch resistance. Supply a Thevenin fixture (E_b,R_b) for each boundary port, with finite R_b≥0, and an individual affine receiver current I_i=G_i V_i+I0_i, positive into IC5 LI2. Require finite G_i≥0 and finite signed I0_i; nonnegative incremental conductance does not make a bias-injecting offset energetically passive. No receiver or source gets an implicit zero/open/ideal default.

The 59-node full formulation consists of 16 LC voltages and 43 boundary bus voltages:

```text
(G_i + sum_b g_ib) V_i - sum_b g_ib U_b = -I0_i
(U_b - E_b)/R_b + sum_i g_ib (U_b - V_i) = 0
```

Only attached branches enter each sum. For R_b=0 impose U_b=E_b directly. Do not divide by zero or insert an arbitrary small resistance.

A reduced 16-node formulation eliminates each source:

```text
D_b = 1 + R_b sum_i g_ib
U_b = (E_b + R_b sum_i g_ib V_i)/D_b
A_ii starts at G_i + sum_b g_ib
rhs_i starts at -I0_i
rhs_i += g_ib E_b / D_b
A_ij -= R_b g_ib g_jb / D_b   (every source b; every attached i,j, including j=i)
```

The updates apply to every source, including the single-output TI/TA ports: finite TI/TA source resistance changes its own diagonal even though it creates no off-diagonal coupling. In exact arithmetic the diagonal-dominance margin is `A_ii - sum_{j!=i}|A_ij| = G_i + sum_{b in B_i}(g_ib/D_b) > 0` for finite nonnegative source resistances and the documented connected branches. This includes individual sources, not only shared buses; numerical loss of that margin still requires conditioning checks.

AM attaches to all 16 outputs, each LV to 8, each KB to the same voice in both ranks, and each TI/TA to one output within this declared fixture. Finite AM impedance couples all voices/ranks; finite KB impedance couples its two rank receivers. Solving each rank independently loses those effects.

Return 16 node voltages/receiver currents and 43 loaded source voltages/currents. Compute source current with the stable eliminated expression `sum_i g_ib(E_b−V_i)/D_b`, applying bounded weights before accumulation, rather than subtracting nearly equal reconstructed bus/node voltages. Validate representability, conditioning and residuals; finite inputs alone do not guarantee a resolvable floating-point problem. Any numerical-domain limit must be explicitly documented as conservative arithmetic policy, never a hardware impedance limit.

## Proposed independent verification

- Compare reduced 16-node solve with independently stamped full 59-node equations, including explicit ideal-source constraints.
- Check individual node/source KCL and signed affine currents; unequal receivers and source voltages are required, not only symmetric fixtures.
- In the explicitly ideal/open-receiver limit, check the weighted sum `V_i=sum(g_ib E_b)/sum(g_ib)`. With a known G/I0 fixture, check the corresponding loaded divider analytically.
- Perturb one receiver: test global cross-rank coupling through finite AM and same-voice coupling through finite KB; each must disappear only when its respective source is explicitly ideal while the other routes remain accounted for.
- Test distinct rank LV, all 16 TI/TA routes, supplied boundary permutations, missing/NaN/negative/nonfinite values and poorly conditioned or unrepresentable cases.
- Mutation-check 180k/560k/270k values, wrong voice/rank attachments, omitted shared AM/KB connections, missing receiver current, ignored source resistance and unstable current reporting.
- Confirm no production dependency. No transient, gain or perceptual agreement follows from a passed DC solve.

## Provenance

Existing originals and unresized crops were inspected directly. All originals are 5100×3300. All ten stored crop hashes and independently regenerated Pillow crop hashes matched in this pass. Source scans are research inputs, not repository payloads. Boxes are native `[left,top,right,bottom]`.

| Original | Primary URL | SHA-256 |
|---|---|---|
| R39 | [Yamaha service scan](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/39%20-%20R51-52%20Circuit%20Diagram.jpg) | `b77875e1aee0e092c320f8dea69ec7b9d975d4b3551eaa2cdabc597886c80156` |
| R40 | [Yamaha service scan](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/40%20-%20R51-52%20Circuit%20Board%20%26%20Wiring.jpg) | `b76ad67725a661ab416121210f771e6f3b8c6ce57d2221d9dec81a3e79156db7` |
| M41 | [Yamaha service scan](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/41%20-%20M%20Circuit%20Diagram.jpg) | `1278f17ff05cd82b2b84153838bf95109d3f1ea1e864119de0a3f4a4af4f0080` |

| Native crop | Source | Box | SHA-256 |
|---|---|---|---|
| `r39-level-native.png` | R39 | `[3900, 1770, 4800, 3090]` | `80357f2d1c88fc66df6a42306fd7e370392edb33bbd09f3532a9ecb8eab1adc2` |
| `r40-level-left-native.png` | R40 | `[550, 450, 1530, 2650]` | `c94b0630175662f5b0654c7ad07e8e2a6ebdbb0e1db46cafa52de26e4d8b2c37` |
| `r40-level-right-native.png` | R40 | `[3500, 450, 4950, 2950]` | `da5bf8457c292ffc269a6fbb35b07cdeb7d0ec283bd94ba32dcaefbb2824aa8c` |
| `m41-output-vca-native.png` | M41 | `[3380, 330, 4630, 1800]` | `bfe63b353e2420fa116030a5ca556415120fe19d2c7f2839e1071cfa9f5b4a8b` |
| `m41-lc-port-native.png` | M41 | `[2250, 1100, 3480, 1790]` | `22c959706c43295e1df7236eb369c0b2c12f1dce28a0f00197d8774cb4397c72` |
| `r40-level-sources-native.png` | R40 | `[1390, 1680, 2480, 2370]` | `1e9c2b30e63c35195e35a6d5e548ad2c3558b83910638b899e5fcf090bb7e0a5` |
| `r40-level-outputs-native.png` | R40 | `[2470, 2390, 4810, 2910]` | `853f6ebc6dbafde14d961560947251895e1a59b7ff88e54fca2b05df1f4dae39` |
| `r40-kb-links-native.png` | R40 | `[510, 1640, 1490, 2380]` | `3b7aed0793f94c7d3bd968ed98613893c1a98e8ea24dd71d4cc3a7dc31ac93c8` |
| `m41-lc-complete-native.png` | M41 | `[2250, 1100, 3560, 1790]` | `a27d892f5ec24aa6e061fc2f4eca6db8b07ba3e99d17810223a386497136d202` |
| `m41-ic-table-native.png` | M41 | `[3940, 2220, 4660, 2550]` | `6cb0db83bc06d240e691b0cbdba8f917b2cfceb370638d3f23778a28295feda7` |

Local manifest: `artifacts/w1-level-20261029/provenance.json`. Independent review must check junctions, inter-rank AM/KB sharing, the exact IC5 port and capacitor, source-label ambiguities and the reduced-equation signs before implementation. The reciprocal review above records the remaining cross-page conflict. Neither that source review nor the conditional network solver permits substituting this model for the production level law.

## Offline implementation and validation

`src/reference/LevelControl.h` provides `cs80::reference::level::solve(Inputs)`.
All source voltage/resistance and receiver conductance/current members default to
NaN and must be supplied explicitly. The fixture has 16 node-indexed receivers:
rank I LC1–8 at 0–7, rank II LC1–8 at 8–15. Its 43 source indices are AM at 0,
rank LV at 1–2, voice KB at 3–10, node-indexed TI at 11–26 and TA at 27–42.
`Result` reports `nodeVolts`, `receiverAmps`, `busVolts`, and `sourceAmps`.
Source current is positive from source toward network; receiver current is
positive from network into the affine receiver.

The solver uses the reduced 16-node equations, row scaling and partial pivoting.
Its conservative numerical policy rejects a row-scaled infinity-norm condition
upper bound above 1e8 or a pivot at/below 1e-12. Reduced residuals are checked at
relative 1e-10 plus 1e-300 absolute; component KCL at relative 1e-9 plus 1e-18 A.
These are arithmetic acceptance thresholds, not measured CS-80 tolerances.
Stable eliminated source-current calculation separately preserves tiny currents
that a reconstructed bus-voltage subtraction would lose. No transient or IC5
gain law is implemented, and the unresolved TI4/TA4 installed-wire ambiguity
continues to prevent a claim of complete upstream equivalence.

`tests/LevelControlTests.cpp` independently stamps all 80 branches into a
long-double 59-node system, including ideal-source constraints. It compares
all node/bus voltages and currents, local KCL and total current budget. Coverage
includes 250 asymmetric randomized fixtures, every source and receiver varied,
ideal/loaded analytical dividers, finite individual TI/TA source resistance,
AM/KB/LV sharing and isolation, rank permutation, unknown/nonfinite/negative
inputs, overflow, near-floating rejection, accepted common-mode accuracy, zero
excitation and relative ~1e-30 A source-current regressions.

Validation commands (standalone C++17, no production audio dependency):

```sh
c++ -std=c++17 -Wall -Wextra -Werror -pedantic -Isrc tests/LevelControlTests.cpp -o /tmp/cs80-level-tests
/tmp/cs80-level-tests
c++ -std=c++17 -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer -Isrc tests/LevelControlTests.cpp -o /tmp/cs80-level-san
ASAN_OPTIONS=detect_leaks=0 /tmp/cs80-level-san
```

ASan/UBSan validation excludes leak checking because this execution environment's
LeakSanitizer reports that ptrace prevents its operation. Ten isolated mutation
builds were rejected by the suite: wrong 180k, 560k or 270k value; wrong KB rank
route; AM missing rank II; swapped LV ranks; ignored individual-source R;
omitted diagonal elimination; omitted receiver current; and cancellation-prone
reconstructed source current. Mutation files/logs are temporary validation
artifacts, not production dependencies or physical measurements.

Final standalone and ASan/UBSan runs: **54,272 checks, zero failures** each; all ten mutations killed after the final test changes.
