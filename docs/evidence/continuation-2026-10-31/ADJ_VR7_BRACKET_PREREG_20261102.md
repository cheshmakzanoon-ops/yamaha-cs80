# ADJ p11 VR7 CP3 bracket scan: separate preregistration

**2026-11-02, before this grid was evaluated.** This is a *third* offline,
test-only **surrogate** calculation, following two separately committed
predeclared grids. It does **not** establish the physical Yamaha trimmer
range, an electrical `VR7`-to-gm transfer, a continuous global maximum, a
hardware reading, or a corrected service manual specification.

## Why this bracket; no voltage-target fitting

The original [Yamaha adjustment table, printed p11](https://www.therogoffs.com/cs80/CS80%20Adjustment%20Procedures%20and%20updates.PDF)
(PDF page13, full file SHA-256
`eea101eb338494f3d1d3fddced2d8aa7f1d877ec9d6155ca44cc77ece88276ba`)
directs **VR7 to maximum CP3 amplitude before adjusting VR8 to
4.3 ± 1 Vpp at CP3**. The previously committed
[coarse grid](ADJ_NI_SENSITIVITY_PREREG_20261102.md) included a
**184.997 Hz** HP gm-law cutoff (Qhp=5) giving CP3 **1.010 / 0.857 Vpp**
for NI-open / ideal-zero-NO1. The separately preregistered
[fine local C1 grid](ADJ_VR7_LOCAL_GRID_PREREG_20261102.md) reached its
**lower boundary at 245.165083 Hz**, with CP3 **2.410 / 2.293 Vpp** under
those same two source assumptions. The earlier lower sample and higher
boundary define a **data-informed bounded exploratory interval**, not a
claim of unimodality. The interval was selected for *bracketing the
source-model maximization objective*, **not** to make CP3 reach 3.3–5.3 Vpp.

## Locked 33-point bracket

Let `f0 = 261.6255653005986 Hz` (C1 at 2′) and define two *previously
evaluated* endpoints independent of the printed voltage target:

- `f_lo = f0 × 2^(-1/2)` (the coarse **184.997 Hz** candidate).
- `f_hi = f0 × 2^(-12/128)` (the fine **245.165083 Hz** candidate).

Use **exactly 33 logarithmically spaced** gm-law cutoffs

`f_hp(j) = f_lo × (f_hi / f_lo)^(j/32)` for every integer `j = 0..32`,

**including both endpoints**. For each use existing HP total cutoff
`f_hp(j) + ffHz`, where `ffHz` is the 4.7 MΩ / 1.5 nF feedforward corner.
Only the preregistered **NI open** (`alpha=0`, 75 kΩ source, half-pulse)
and **ideal silent zero-impedance NO1** (`alpha=1`, 50 kΩ source,
one-third-pulse) boundary models are evaluated. They are *not* actual
measured NO1 terminations. The NO1 noise waveform is set to **zero** only
to isolate loading; actual source voltage and spectrum remain unknown.

Fix HP low-frequency Q=**5** throughout this bracket. LP `HIGH` is the
frozen **18 kHz** open approximation and Qlp=**5**. The waveform, source
factory control law, physical network and ideal LP output are untouched:
3 Vpp 50%-duty C1 square at 2′, 192 kHz, 3-second linear matrix simulation,
last 0.2 second Vpp. The 33 cutoffs are **not selected from the measured
output**. For each `j` and both boundaries print **CP2 (HP output) Vpp**,
**CP3/CP8 (LP output) Vpp**. After all 33, report the **largest simulated
CP3**, its frequency, CP2 at that frequency, the CP3/CP2 ratio, and whether
the maximizer is **interior or at a boundary**. If multiple local peaks are
present, do not infer uniqueness or a physical VR7 position.

Separately print the **original frozen** `hpEffectiveCv` model prediction
(259.968 Hz GM cutoff) and the unchanged Yamaha printed **3.3–5.3 Vpp CP3
numeric interval**. It may be mathematically noted whether *a synthetic
grid value* falls inside that interval, but **neither a new value nor a
surrogate maximizer can make the registered ADJ p11 HP gate pass**: it must
continue to evaluate the original fixture's CP3=1.692 Vpp and print `MISS`.
The real VR7 position, initial VR8 Q calibration, physical noise level,
FET impedance, IG00156 output impedance and IG00151 VCA input load are not
specified. This experiment cannot infer them.

Use a new explicit `--vr7-bracket-sensitivity` opt-in test-only path, with
finite-positive assertions only. Keep all ordinary nodal, phase-correction,
V1 compatibility, factory-tone hypotheses, original tolerance JSON and DSP
numeric values unchanged. V1 fingerprint remains **`e5f877a025fc52c6`**.
Commit this preregistration **before** implementing/running the bracket.
