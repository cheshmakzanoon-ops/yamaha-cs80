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

## Evaluation after preregistration, 2026-11-02

The complete grid above was committed as `3f99776` **before** the opt-in
numeric evaluation. All ten model points passed finite-positive checks,
zero failures. The procedure produced the following **unselected** values
at the bracket's exact `j28`, `236.685656 Hz` gm cutoff:

| Hypothetical HP Q | CP2, NI open (Vpp) | CP3, NI open (Vpp) | CP2, silent NO1 (Vpp) | CP3, silent NO1 (Vpp) |
| ---: | ---: | ---: | ---: | ---: |
| .5 | 1.856 | .901 | 1.671 | .807 |
| 1 | 2.267 | 1.051 | 2.102 | .965 |
| 2 | 3.295 | 1.416 | 3.157 | 1.339 |
| 5 | 6.630 | 2.595 | 6.507 | 2.524 |
| 9 | **11.027** | **4.148** | **10.899** | **4.075** |

For **Q=9 only**, the *synthetic linear-model CP3 prediction* happens to lie
numerically within Yamaha's printed **3.3–5.3 Vpp** range in both boundary
models. That is a **conditional calculation, not an ADJ pass or verified
hardware Q value**. In particular:

1. Yamaha's documented low-frequency control endpoints (service M p42)
   correspond in the existing nodal model to **Q≈5 at VQ=0 V** and
   **Q≈0.5 at VQ=10 V**. Q=9 was included in the earlier *numerical
   stress-test grid*; it is **outside those documented endpoint values**
   and not independently shown to be reachable through the actual VR8
   trimmer. The real IG00156 Qadj pin-6 current/transfer remains unknown.
2. The simulated **~11 Vpp CP2** peak-to-peak value at Q=9 makes the
   calculation especially sensitive to nonlinear OTA compression,
   clipping and finite chip power rails not covered by this **linear**
   service-fixture solve. Passing the numerical stress/finite checks does
   not validate 11 Vpp as a hardware waveform.
3. The bracket-selected `236.686 Hz` is from the offline surrogate at
   Q=5. A real technician re-adjusting VR7, followed by VR8, has an
   unknown initial Q, switch resistance, noise source and interstage load.
   This Q sweep holds cutoff fixed and does **not** rerun VR7 at Q=9.
4. The registered original source-factory HP setup remains **CP2 4.126,
   CP3 1.692 Vpp, `MISS`** against the unchanged printed band. The
   synthetic numerical overlap is **not** a tolerance relaxation, change
   of measurement terminal, source normalization, or approved DSP update.

The result motivates **actual, simultaneous CP2/CP3 scope observations**
while following the manufacturer's VR7-maximum-then-VR8-CP3 sequence,
alongside NO1(39)/NI(43), IC10 pin 4 Vfc and pin 6 Qadj readings and
source-verified nonlinear operating limits, rather than calibration by
matching any Q in an unmeasured test model. Reproduce the entire fixed
grid without a hidden parameter selection via:

```sh
./build-circuit/cs80_ig00156_nodal_tests tests/parity/tolerances-v2/ws1-voice.json --vr8-after-vr7-sensitivity
```

## Unchanged registered regressions after all opt-in probes

The ordinary, non-opt-in full nodal command still reports **122 checks,
zero failures**, whole-grid small-signal **MET**, and exactly
`ADJ p11 HP point: MISS; LP point: within band`, with the HP/LP CP3
voltages **1.692 / 1.648 Vpp**. The source factory tone suite passes
**373,977 checks, zero failures**, including its independently pinned V1
manifest fingerprint `e5f877a025fc52c6` and seven-tone 16,632-value
V1 corpus. The IG00156 phase-correction suite passes **853 checks,
zero failures**, including the original 2×/4× full-band grids at
88.2/96/176.4/192/384 kHz. No tolerance/manifest/production header or
sound path is modified. These regressions validate only preservation of the
existing software model contracts, **not** the source-hypothesis accuracy
of an actual Yamaha M-board.
