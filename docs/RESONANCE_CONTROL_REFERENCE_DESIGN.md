# Resonance control: conditional loaded DC reference design

2026-10-29. **Native graph and equations independently reviewed; offline implementation and independent numerical review completed. No production integration.** This is a bounded W1/W9 passive-network reference. It does not establish IG00156's VQ-to-Q or VQ-to-feedback-conductance law, preset voicings or analog parity.

## Source-closed passive scope

R39 is explicitly the common R51–52 circuit drawing. Each board has eight HP resonance outputs QH1–QH8 and eight LP outputs QL1–QL8. Every output is the junction of **two 220 kΩ resistors**:

- QHj receives one branch from rank-local QH terminal57 and one from RES terminal7.
- QLj receives one branch from rank-local QL terminal15′ and one from RES terminal7.
- No grounded shunt or capacitor is drawn on these output summing nodes in this part of R39.

R40 supplies the inter-rank evidence: R51 RES7 explicitly wires to R52 RES7; the reciprocal R52 column names R51 RES7 and **PN2-VR14-2**. Thus a complete two-rank conditional model has **one shared master RES source**, four rank/side QH/QL sources and 32 individually loaded output nodes. Giving each rank an independent ideal master source would discard real common-source loading. A one-rank experiment can still be defined, but its source boundary would need to account explicitly for the omitted rank.

The master-pot tap is identified, not characterized here: its rail voltages, taper, wiper resistance and any additional connected loading remain outside the boundary. Likewise the four rank buses are the loaded preset/PANEL/memory source ports, not arbitrary normalized slider values. The [rank-I](PRESET_RECEIVER_PORTS.md) and [rank-II](RANK_II_RECEIVER_PORTS.md) ledgers retain their source and memory-path conflicts.

## Receiver connections and labels

R40 connects QHj to M1j/M2j Q1 terminal52 and QLj to the corresponding Q2 terminal57. The rank-II ledger independently traces all eight boards and preserves M26's conflicting `R51-QL6` annotation against the reciprocal R52 destination. The common M41 circuit connects Q1 directly to IC10 **VQ pin7**, and Q2 directly to IC11 **VQ pin7**. No external shunt/capacitor is drawn on those VQ wires. This is not a statement that the chip has zero input current or capacitance.

**M41 conflict:** its Q2 connector label is visibly **55**, while R40 and M-board wiring identify Q2 as **57**. M41 also labels the separate cutoff f2 terminal55. Preserve the disagreement; the named Q2-to-VQ7 route is clear, but this pass does not erase or reinterpret the printed connector number.

The separate Qadj pin6 and B50k VR8/VR10 trim network are not tied to VQ in the external drawing. Their influence must be incorporated in the supplied receiver operating condition or a later explicit chip model. Do not turn the trim into an invented grounded VQ resistor. Similarly, the cutoff pin4 0.01 µF capacitor and grade-dependent R6/R7 shunts belong to the cutoff graph, not this resonance graph.

| Voice j | QHj terminal | QLj terminal |
|---|---|---|
| 1 | 6 | 5′ |
| 2 | 11 | 6′ |
| 3 | 12 | 11′ |
| 4 | 17 | 16 |
| 5 | 89 | 18 |
| 6 | 90 | 90′ |
| 7 | 91 | 91′ |
| 8 | 92 | 92′ |

These are separate output terminals per voice, not a daisy-chained single Q voltage.

## Explicit boundaries

A useful first implementation can solve settled DC with:

- Five independently supplied Thevenin source fixtures `(E_b, R_b)`: rank-I QH/QL, rank-II QH/QL and shared RES. Every source voltage and finite nonnegative resistance is explicit; zero resistance means an intentionally ideal fixture. These independent fixtures do not assert that the omitted real source networks are uncoupled multiports.
- Thirty-two separately supplied affine receiver fixtures `I_i = G_i V_i + I0_i`, current positive **into the chip**, with finite nonnegative G and finite signed I0. Nonnegative G means passive incremental conductance; a signed offset can inject bias current, so the entire affine fixture is not claimed passive in an energy sense. An open receiver requires explicit G=I0=0. The affine boundary is not an identified IC current law.
- A declared common voltage reference. This experiment does not solve supply/reference movement, Qadj trim current or nonlinear receiver behavior.

Missing fields must default to unknown/NaN and fail validation. No implicit open chips, ideal drivers, common component grade, zero offset current, shared trim or default panel voltage. Return every VQ-node voltage, all five loaded bus voltages, source currents and receiver currents so loading can be audited.

No dynamic capacitor or empirical time constant should be added: this source-closed increment is DC only. It must remain outside Engine/Dsp and must not replace the existing provisional resonance mapping.

## Nodal equations

Let `g = 1 / 220000`, each output index i identify rank, HP/LP side and voice, `b(i)` identify its rank/side bus, and M denote the shared RES bus. Then:

```text
(g + g + G_i) V_i - g V_b(i) - g V_M = -I0_i
```

For each bus b, with attached output set S_b and finite positive source resistance:

```text
(V_b - E_b)/R_b + sum_{i in S_b} g (V_b - V_i) = 0
```

There are eight members on each rank/side bus and 32 on shared RES. For R_b=0 constrain V_b=E_b directly; do not form infinite conductance. All branch orientations and source-current signs should be tested independently.

A stable source-elimination expression is:

```text
V_b = (E_b + R_b g sum_{i in S_b} V_i) / (1 + R_b |S_b| g)
```

For a reduced 32-node solve, start with diagonal `2g+G_i` and RHS `-I0_i`. Each source bus contributes RHS `g E_b/(1+R_b |S_b|g)` to its attached rows and subtracts `R_b g²/(1+R_b |S_b|g)` from every matrix element whose row and column are both attached to that bus. In the ideal-source limit the matrix subtraction vanishes. Finite shared RES resistance therefore couples both filter sides, all voices and both ranks. Independent HP/LP dividers or two separately solved ranks generally miss that coupling.

The unreduced 37-node circuit is an independent reference formulation. Very large but finite source resistance can make an eliminated system poorly conditioned; a future implementation must check conditioning/residuals or reject unrepresentable cases rather than silently emitting plausible-looking voltages. This design does not promise unrestricted floating-point range.

## Proposed numerical acceptance

1. Independent literal resistor/source/receiver nodal assembly versus the reduced solver, including explicit ideal sources.
2. Node KCL, five source-current budgets and signed receiver currents across unequal synthetic receivers and source impedances.
3. Open-receiver/ideal-source limit `V_i=(E_b(i)+E_M)/2`; loaded limit with a known G/I0 fixture. These are conditional arithmetic identities, not hardware measurements.
4. Perturb one receiver and demonstrate cross-rank changes through finite shared RES; remove them in the explicitly ideal RES limit while retaining rank-local source coupling.
5. Rank/side/voice permutations only when their **supplied** boundaries are also permuted. Never infer physical symmetry from equal synthetic fixtures.
6. Detect mutations to 220k values, branch assignment, shared-versus-duplicated RES, omitted receiver current and source-resistance loading.
7. Missing/nonfinite/negative boundaries, overflow and ill-conditioning fail closed. Confirm no production dependency.

Hardware calibration remains blocked by actual source/receiver laws, the VQ/Qadj/internal-Q mapping, unit/trim conditions and measured small-/large-signal filter behavior. Arithmetic consistency cannot satisfy W4's physical response gate.

## Primary provenance

Existing source bytes were reused; no new download was necessary. Originals are 5100×3300 pixels. New native crops below use Pillow `Image.open(source).crop(box).save(...)`, no conversion or resizing; coordinates are `[left,top,right,bottom]`. Full-page downscaled overviews were used only to locate native regions, not to transcribe values.

| Original | Primary URL | SHA-256 |
|---|---|---|
| R39 | [R51–52 circuit p39](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/39%20-%20R51-52%20Circuit%20Diagram.jpg) | `b77875e1aee0e092c320f8dea69ec7b9d975d4b3551eaa2cdabc597886c80156` |
| R40 | [R51–52 wiring p40](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/40%20-%20R51-52%20Circuit%20Board%20%26%20Wiring.jpg) | `b76ad67725a661ab416121210f771e6f3b8c6ce57d2221d9dec81a3e79156db7` |
| M41 | [M circuit p41](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/41%20-%20M%20Circuit%20Diagram.jpg) | `1278f17ff05cd82b2b84153838bf95109d3f1ea1e864119de0a3f4a4af4f0080` |

| Native crop | Source / box | SHA-256 |
|---|---|---|
| `r39-q-full-native.png` | R39 `[3520,430,3980,3060]` | `9771f2f8c5f579ab513c2900cb479c6675d7e61e9e4531abbc6e45ea7ad20c0b` |
| `r40-res-link-native.png` | R40 `[3090,1160,4560,1350]` | `7f4da338cb0cae5ab7b92e0a633512e6510e49a0b87e82f77e43a76745398680` |
| `m41-q-native.png` | M41 `[1160,1230,3260,1880]` | `f08e3c7555a708a27b65672a31e492361930eb841fb08fca4084be9224b86cde` |

The existing R40 rank-II left/right crops in the [rank-II ledger](RANK_II_RECEIVER_PORTS.md) were also inspected for receiver endpoints. Local inputs/manifest: `artifacts/w1-resonance-20261029/`. No source scans are repository payloads. Independent source review confirmed branch junctions, shared RES wiring, pin7 routes, the connector conflict and reduced-system signs. Three original hashes and all three stored/regenerated crop hashes matched.

## Offline implementation and numerical boundaries

`src/reference/ResonanceControl.h` supplies the 32-node reduced DC solve, 32 individual VQ voltages/receiver currents and all five loaded bus voltages/source currents. Source and receiver fields default to NaN; callers must explicitly supply every fixture member. It has no production dependency or VQ-to-Q mapping.

The implementation row-scales its matrix and bounds the scaled system's infinity-norm condition number conservatively using strict row diagonal dominance (Varah's bound). For row i the unscaled dominance margin is evaluated from the circuit as `G_i + g/(1+8 R_local g) + g/(1+32 R_RES g)`, not by subtracting nearly equal diagonal and off-diagonal totals. Divide each margin by its row scaling factor, then divide the maximum scaled absolute row sum by the minimum scaled margin. An upper bound above **1e8** is rejected. This deliberately conservative numerical-domain limit may reject a solvable fixture; it is not a measured electrical limit, exact condition-number estimate or analog acceptance tolerance. Pivot and residual checks remain additional safeguards, not substitutes for conditioning.

Source currents are computed by eliminating the bus: `I_b = g sum(E_b−V_i)/(1+R_b n_b g)`, with bounded weights applied before accumulation. Summing reconstructed bus-to-node differences instead loses small currents when a very high-impedance source is nearly disconnected from receiver-driven voltages. A synthetic 1e30 Ω fixture anchored near1 V by receiver offsets retains its approximately−1e−30 A source currents. A nearly floating open-receiver fixture with sources1 V/1e17 Ω is rejected rather than accepting the earlier cancellation-contaminated result. These extreme fixtures test arithmetic conditioning only.

`tests/ResonanceControlTests.cpp` independently assembles all37 physical nodes, including separate source KCL and individual branch stamps, using long-double arithmetic where supported. It compares against the reduced solver across250 deterministic unequal-boundary fixtures, and checks analytic limits, KCL, common-source cross-rank coupling, explicitly permuted/symmetric fixtures, omitted/invalid inputs and scale-sensitive numerical regressions.

The final strict C++20 suite passes **28,059 checks**. Nine targeted mutations were detected: a changed branch resistance, wrong rank/side attachment, missing rank-II shared-RES connections, omitted/reversed receiver offsets, omitted receiver conductance, ignored source impedance, removal of the condition guard and restoration of the cancellation-prone current expression. Some graph mutations trigger the solver's independent residual/KCL rejection on otherwise valid fixtures; the test executable treats that unexpected rejection as failure rather than proceeding with invalid voltages. The condition-guard mutant fails one dedicated check and the current-expression mutant fails five scale-sensitive checks. Full-suite and platform CI outcomes must be read from the exact published commit.
