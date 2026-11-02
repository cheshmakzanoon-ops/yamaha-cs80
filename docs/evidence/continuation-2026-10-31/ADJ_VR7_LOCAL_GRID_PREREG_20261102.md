# ADJ p11 VR7 local C1-grid refinement: preregistration

**2026-11-02, prior to evaluation.** This is a second, separately fixed
**model-only** experiment, motivated by the explicitly recorded coarse-grid
shortcoming in
[ADJ_NI_SENSITIVITY_PREREG_20261102.md](ADJ_NI_SENSITIVITY_PREREG_20261102.md).
The coarse scan was an exact half-octave grid whose peak was below the
already-frozen HP service alignment model's voltage. That fact motivated
examining *resolution near the independently specified C1 fundamental*, not
choosing a voltage target or a trimmer setting after looking at results.

## Locked local grid

Use the identical Yamaha ADJ p11 HP setup surrogate as before: C1 at 2′,
`f0=261.6255653005986 Hz`, 3 Vpp square, ideal linear 8-state HP→LP pair,
192 kHz, 3 seconds settling and last 0.2 seconds for peak-to-peak.
LP `HIGH` remains 18 kHz + the existing 4.7 MΩ feedforward corner;
low-frequency Qhp and Qlp both remain **5**. Use the same physical filter
control law and fixed coefficients; no retuning Q, source level, output
normalization, 180 pF, or resistors other than the two previously declared
NI boundary configurations. Keep NO1 at **zero signal** only for the
sensitivity calculation, never assert a silent hardware noise source.

Predeclare **exactly 25** HP gm-law cutoffs at

`f_hp(j) = f0 × 2^(j/128), j = -12,-11,...,+11,+12`,

covering approximately **±6.7%** around C1 with approximately **0.54%**
frequency steps; add the existing `ffHz` term to obtain the solver's total
cutoff. Repeat the *entire* fixed list for only the earlier preregistered
synthetic limits:

- **NI open:** `alpha=0`, mixer 75 kΩ, pulse gain 1/2.
- **NO1 ideal silent, zero-impedance:** `alpha=1`, mixer 50 kΩ, gain 1/3.

For every point print the input hypothesis, integer `j`, frequency, **CP2
HP output Vpp**, and **CP3/CP8 LP output Vpp**. Then print the maximum
CP3 among these 25 points and its grid frequency, **not** a claim about the
physical VR7 travel, an actual continuous maximum, or all possible
nonlinear/coupled circuit states. As a fixed external reference, also print
the existing frozen source-factory alignment prediction in each NI boundary
but **do not substitute it into the grid selection**. Preserve the printed
Yamaha ADJ p11 CP3 band **4.3 ± 1 Vpp**, original baseline `MISS`, and
all frozen gates; add only finite-positive checks for synthetic outputs.

The Yamaha [ADJ manual](https://www.therogoffs.com/cs80/CS80%20Adjustment%20Procedures%20and%20updates.PDF)
(printed p11/PDF page13, SHA-256
`eea101eb338494f3d1d3fddced2d8aa7f1d877ec9d6155ca44cc77ece88276ba`)
requires **VR7 maximize measured CP3 amplitude, then VR8 adjust CP3 to
4.3 ± 1 Vpp**; it does not define a physical VR7-position-to-gm curve or
initial VR8 offset. Hence neither this grid nor the existing Q sensitivity
constitutes a replay of that hardware procedure. Exact-test-point identity
and the NI/CP3 branch evidence are documented in
[NATIVE_FILTER_EVIDENCE.md](NATIVE_FILTER_EVIDENCE.md).

Implement as a separate *opt-in* `--vr7-local-sensitivity` offline path in
the existing nodal test executable; the normal full-band and ADJ gates must
run exactly as before. No production DSP, V1 numbers,
`e5f877a025fc52c6` fingerprint, tolerance files, hardware readings, or
unverified resistor values may change. **Commit this preregistration first.**

## Evaluation after pre-registration, 2026-11-02

The fixed grid above was committed as `a4e1350` **before** building/running
the opt-in variant. All **50** candidate conditions passed their offline
finite-positive checks; these are synthetic *linear-model* results only.

| NI boundary | Frozen original gm cutoff | Original CP2/CP3 | Highest CP3 among 25 local grid points | Grid location |
| --- | ---: | ---: | ---: | --- |
| NI open, alpha 0 | 259.968 Hz | 4.126 / **1.692** Vpp | **2.410** Vpp (CP2 6.139) | `j=-12`, **245.165083 Hz** |
| Silent zero-Z NO1, alpha 1 | 259.968 Hz | 3.812 / **1.482** Vpp | **2.293** Vpp (CP2 5.897) | `j=-12`, **245.165083 Hz** |

Both 25-point grids **decreased monotonically from the low-frequency
boundary to the high-frequency boundary**, with alpha-0 CP3 moving
2.410 → 1.109 Vpp and alpha-1 CP3 2.293 → 0.934 Vpp. Thus **the maximum
is on the scanned boundary**: these are not true local maxima, and this
result cannot establish an upper bound or reproduce physical VR7 tuning.
The existing original HP service reading remains **1.692 Vpp, MISS** against
the unchanged **3.3–5.3 Vpp at CP3**.

For comparison, the independently earlier half-octave scan included HP
gm cutoff **184.997 Hz** with CP3 **1.010 Vpp** (NI open) or **0.857 Vpp**
(ideal-zero NO1), both lower than the new boundary result at **245.165 Hz**.
That pair of *previously fixed* frequencies creates a bounded interval
for the next **explicitly preregistered** bracket search; it does not prove
a single peak, a physical VR7 operating range, or an actual potentiometer
setting. The complete CP2/CP3 series is reproducible via:

```sh
./build-circuit/cs80_ig00156_nodal_tests tests/parity/tolerances-v2/ws1-voice.json --vr7-local-sensitivity
```
