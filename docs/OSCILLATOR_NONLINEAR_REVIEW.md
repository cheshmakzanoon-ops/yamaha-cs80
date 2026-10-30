# Oscillator → nonlinear VCF isolation review

2026-10-30. **Failed integration gate, not a hardware-fidelity measurement.** The current 16-sample oscillator remains experimental; oscillator→decimator success does not qualify insertion before the existing nonlinear SVFs. No Engine changes were made for this review.

## Reproducible experiment

Saved inputs, header snapshots, scripts, manifests and all CSV results: [repository artifact archive](../artifacts/oscillator-experimental/nonlinear/README.md). The original working directory was `/tmp/oscillator-nonlinear-review/`.

| Input | SHA256 |
|---|---|
| `ReconstructedOscillator.h` snapshot | `eb8bed77d44784b6d0fc9145ad9f84d0e1c257f6253b49b8f6920e37d3e81db3` |
| Unchanged `Dsp.h` snapshot, including SVF and 255-tap decimator | `4e0457f36af2ea27304de47e863cb4ec77b10521adf7c00607e9202c79fe07b7` |
| Independent `make_reference.py` | `0f0d0532d5d42266db9a972955621cd1fa2ed4cfae31eb1b847cc1359ea801e6` |
| `nonlinear_probe.cpp` | `13171c39a430ea5d987587ba9bd6e41f310134f53903dcfd7fb390a3d6944895` |
| `variant_probe.cpp` | `2d4453c7631624ac515c644e1b46a714a37581b756453b2d3c4d1a9b7810757f` |
| Independent reference manifest | `d13b8010369b624bcd5c904d18aebf4cef7ab674dd0f6bd995cea8e7db3b8643` |

The reference independently evaluates continuous periodic `w3-current-fold-v1` equations on a 2^20-point period grid: continuous-frequency RC transfer, fold/clamp, second RC, then PWL shaper. Its Fourier coefficients are ideally lowpassed to **internal Nyquist before sampling**. It does not invoke the production event scheduler. A 2^19/2^20 convergence comparison is below -91.08 dB relative to reference AC power across these inputs (`reference_convergence.json`).

Both arms use the same fixed sample timing, voltage adapter and downstream code:

```
x = 0.9 * (enabledSawVolts + enabledPulseVolts) / 1.75
x = optional existing Svf HP(x), then optional existing Svf LP(x)
y = x + enabledSineVolts / 1.75
hostOutput = existing 255-tap 2x decimator(y)
```

The SVFs retain their existing `softClip(x*1.1)` behavior. There is no drive reduction, per-waveform normalization, independent phase fitting, or fitted-mean removal. Common oscillator delays are 8/16/32/64 internal samples for the four variants; the independent reference is aligned accordingly.

Each candidate has **630 cases**: internal rates 88.2/96 kHz; coherent held-out pitches near 1046/4186/8372/16744/23000 Hz; all seven nonempty saw/pulse/sine combinations; nine filter configurations. Eight regular configurations cover bypass, HP/LP and single-filter paths, with HP cutoffs 18 Hz/1/5/15 kHz, LP cutoffs 12/18/30 kHz, Q=.707/3/5. A ninth Q=9 API-stress configuration is reported separately. Exact frequencies/configurations are in the manifest and CSV files. Both arms settle for at least two seconds, then capture 16384 internal samples.

**Error metric:** full time-domain candidate-minus-reference energy divided by reference **AC** power. DC error remains in the numerator. No spectral mask excludes known aliases; folded energy and aliases on legitimate harmonics therefore contribute to the residual. This comparison isolates oscillator numerical differences while holding the same 2x nonlinear VCF fixed. It does **not** certify that VCF's own aliasing, analog convergence, or the complete instrument.

## Failures retained

The 504-case subset below excludes Q=9 and requires a non-negligible reference AC RMS and fundamental below the relevant host Nyquist. Thus the expanded-range 23 kHz stress case is included at 48 kHz host, but not 44.1 kHz host. Its inclusion is a numerical range stress, not a newly measured CS-80 operating law.

| Kernel support / cutoff, cycles per internal sample | Cases exceeding -45 dB | Worst residual |
|---|---:|---:|
| 16 / .50 | 142 / 504 | -6.583 dB |
| 32 / .375 | 286 / 504 | -0.262 dB |
| 64 / .45 | 146 / 504 | -0.438 dB |
| 128 / .49 | 33 / 504 | -17.603 dB |

Restricting the original 16-sample candidate to the tested pitches through 16.744 kHz still gives **107/448 failures**. At 96 kHz internal, approximately 16.746 kHz, HP=1 kHz, LP=12 kHz, Q=3: saw+sine is -20.119 dB and pulse-only is -20.242 dB. At 88.2 kHz internal, worst residuals near 8.372/4.186 kHz are -37.218/-38.538 dB.

**Control:** filter-bypassed cases through 16.744 kHz are approximately -82 to -109 dB. The large filtered errors are therefore not explained by a common phase/level mistake. Upper-internal-band waveform differences can mix into the audible band before the final decimator. Lowering cutoff also removes legitimate internal-band harmonics; it is not automatically a repair.

The 128/.49 variant passes only the tested 1046–16744 Hz regular-Q subset (worst -50.177 dB), then fails the 23 kHz/48 kHz-host stress cases. It is not qualified for integration.

| Preserved report | SHA256 |
|---|---|
| `results.csv` — 16/.50 | `6f8a38a3bab5beee1ce3f43e07c5426aa561b592f75d9c6216c6030e0af60805` |
| `results-linear32_0375.csv` | `ad3ce5ffa91873a252cefcb35d01d766197c11f49f57a0fc35e387e486e8911e` |
| `results-linear64_045.csv` | `e42f70e286e2fafac001c43b2a610b62d37d066703f396e4aa8156fb278edccf` |
| `results-linear128_049.csv` | `89da346e10318ea18ae1ce47b77fd667f6eaf6a73e399180e816226d5ee4b189` |

## Reproduction and next decision

From the repository root:

```sh
bash artifacts/oscillator-experimental/nonlinear/reproduce.sh
```

The wrapper copies the archived inputs to the original temporary working path, regenerates the independent references, compiles the saved snapshots and reruns all four candidates. It requires Python with NumPy and a C++17 compiler; it performs no network or installation action. Generated binary reference streams and executables are intentionally not archived.

These saved-snapshot experiments are **exploratory**, not exact-commit CI. The archive retains their inputs, reference/convergence manifests and all failed-candidate CSV files; subsequent implementation changes must not overwrite these historical results.

**Decision:** do not integrate the 16-sample model directly ahead of the current SVFs, and do not hide failures by changing drive or tolerances. A higher-rate **VCO-plus-nonlinear-VCF island** needs a separate numerical/CPU plan: define its control reconstruction, source amplitudes, conversion filters and complete-path latency; compare independently converged rates such as 4x/8x; then run modulation, state, alias, compatibility and real-time benchmarks. A sharper reconstruction kernel is another explicitly costed numerical hypothesis, not an accepted fix. No hardware access is required to continue this work.
