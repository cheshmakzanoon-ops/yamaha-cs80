# ADJ p11 post-VR7 Q sensitivity: final sequential surrogate preregistration

**2026-11-02; committed before numerical evaluation.** This test-only
calculation follows the earlier independent
[coarse NI grid](ADJ_NI_SENSITIVITY_PREREG_20261102.md),
[local C1 grid](ADJ_VR7_LOCAL_GRID_PREREG_20261102.md), and
[bounded bracket](ADJ_VR7_BRACKET_PREREG_20261102.md). The bracket was
predeclared before its results and found a model-grid interior maximum at
**j=28**, HP gm cutoff **236.685656 Hz**, for **both** NI endpoint
assumptions, with Qhp=5. This final probe examines Q sensitivity at that
*already selected* frequency. It must not fit Q to Yamaha's Vpp target.

## Yamaha hardware step, and unverified adapter

The manufacturer [CS-80 adjustment procedure printed p11](https://www.therogoffs.com/cs80/CS80%20Adjustment%20Procedures%20and%20updates.PDF)
(PDF page 13, full SHA-256
`eea101eb338494f3d1d3fddced2d8aa7f1d877ec9d6155ca44cc77ece88276ba`)
directs a technician to adjust M-board **VR7 for maximum CP3 amplitude,
then VR8 to 4.3 ± 1 Vpp at CP3** under the FH +4.0 V HP setup. Original
M41 p41 traces VR7 toward IC10 Vfc pin 4 and VR8 toward Qadj pin 6; it
does **not** provide measured trimmer-angle-to-gm or trimmer-angle-to-Q
functions. The model Q parameter is a **declared inferential adapter**,
not an observed current at the actual VR8 position. CP2 remains the HP
output, while CP3/CP8 is the LP output by M44 harness p44.

## Locked cases

Fix `f0=261.6255653005986 Hz`,
`f_lo=f0 × 2^(-1/2)`, `f_hi=f0 × 2^(-12/128)`,
`f_hp=f_lo × (f_hi/f_lo)^(28/32)` (the bracket's **j28**, not a retune).
Keep LP `HIGH` as 18 kHz, Qlp=5, same 3 Vpp C1 2′ pulse, 192 kHz,
3-second matrix simulation, last 0.2 second Vpp. Output all results
for the **five pre-existing registered nodal Q settings**:

`Q_hp = {0.5, 1.0, 2.0, 5.0, 9.0}`.

Repeat every Q for both and only the two source-boundary cases:

1. `alpha=0`, **NI open**, 75 kΩ effective source, half-pulse.
2. `alpha=1`, **silent ideal-zero-output-impedance NO1**, 50 kΩ effective
   source, one-third-pulse. This is not a hardware NO1 measurement.

For all **ten** cases, report **CP2 and CP3/CP8 Vpp**, CP3/CP2 ratio,
and whether the synthetic numerical CP3 overlaps Yamaha's printed
**3.3–5.3 Vpp** interval (purely an arithmetic comparison). No Q setting
may be selected as an inferred actual VR8 position or used to claim a
passed alignment. Report the original frozen source-factory HP result
**1.692 Vpp at CP3, MISS**, separately, unchanged.

This is a *conditional Q-sensitivity grid* at one model-selected cutoff.
It does not reproduce actual VR7-max Q state, VR8 electrical adjustment,
noise spectrum, FET resistance, IC10/IC11 amplifier loading, or IG00151
VCA input impedance. Only finite-positive assertions can be added. Run
the sweep by explicit opt-in `--vr8-after-vr7-sensitivity`; preserve the
ordinary nodal, registered phase and factory-tone tests, tolerance JSON,
production DSP, frozen V1 controls, and **`e5f877a025fc52c6`** manifest
fingerprint. **Commit this predeclared grid before running it.**
