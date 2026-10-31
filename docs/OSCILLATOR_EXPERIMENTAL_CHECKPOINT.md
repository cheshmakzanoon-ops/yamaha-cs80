# Experimental IG00153 / IG00158 checkpoint

> **Promoted (2026-10-31):** the oscillator now lives in
> `src/core/ReconstructedOscillator.h` and drives the circuit engine; see
> [OSCILLATOR_PROMOTION.md](OSCILLATOR_PROMOTION.md). The statements below about
> Engine selection and the nonlinear-path failure describe the 2026-10-30 checkpoint.

2026-10-30. **Implemented standalone model; not an Engine candidate, measured CS-80 match, or completed W3 milestone.**

## Useful result and release boundary

`src/experimental/ReconstructedOscillator.h` now produces phase-coherent WSC saw, comparator pulse, and folded/PWL-shaped sine in volts from one event-driven charge/reset trajectory. The actual two coupling states, fractional reset timer, comparator edges, all fold/shaper corners, and overlapping antialias tails continue through control changes. Cold startup has explicitly zero **audio** prehistory; physical capacitor initialization remains an inferred convention. There is no note-on reset method and no oscillator RNG draw.

The model is deliberately **not selected by Engine**. Its short antialias kernel passes the standalone linear 2x output path but fails the existing nonlinear SVF path; its complete 16-oscillator real-time cost is also not qualified. The header exposes both `productionNonlinearPathQualified=false` and `productionRealtimeQualified=false`. These are status metadata, not host parameters or another fidelity switch. No legacy oscillator arithmetic was changed.

The frozen [design proposal](OSCILLATOR_REALTIME_PROPOSAL.md) retains the evidence, contradictory reference-current-switch prose, selected restoring-current interpretation, nominal profile and original acceptance goals. This checkpoint records implementation results separately; it does **not** retrospectively weaken those goals.

## What was implemented

- `OscillatorNominalProfile::id = w3-current-fold-v1`: fixed selected 30 us reset, fixed converter gain, 3.5 V pin7 plateau, two equal 4.7 uF/22 kOhm ideal-port coupling time constants, explicit midpoint triangle limiting, inverting odd four-segment sine transfer and frozen reference sine trim. These opaque-IC laws remain labeled inference.
- Exact normalized charge integration: ramp `q'=-f`, reset `q'=1/tau-f`. Live frequency changes do not reset charge or restart the one-shot; zero current freezes the ramp but finishes an active reset and continues capacitor relaxation.
- Exact first coupling for a linear pin7 segment and exact second coupling on each folded segment. Both nominal poles have the same RC. Between events each final branch is `C+(A+B*t)*exp(-t/RC)`.
- Antialiasing integrates that complete exponential basis against one prepared kernel; it does not feed a smoothed saw into the comparator/fold. The finite event residuals have exact zero tails by their construction, rather than truncating a nonzero BLAMP asymptote.
- Current candidate: 16 internal-sample, 64-fractional-phase, symmetric Blackman–Harris sinc, cutoff .50 cycles/internal sample, **8 internal samples of explicit delay**. It is optimized for the current 2x/255-tap linear output chain, not direct full-internal-Nyquist use.
- Fixed callback storage and bounded event/root loops. Nonfinite controls retain the last finite value and set diagnostics. Frequency extrapolation is bounded by `min(.45*internalRate,.95/resetSeconds)` and reported, not silently called a hardware limit.
- Full-object copies preserve physical state, profile/rate, prepared residual basis, and all pending tails. Raw pin diagnostics remain ungated; returned audio is a downstream-gate-selected pin-voltage readout.

## Qualification results

The test source is `tests/ReconstructedOscillatorTests.cpp`. An independent periodic fixture uses analytic first-RC solutions and convergent dense-period integration for the second RC; it does not call the production event scheduler. Complex harmonic comparisons use one shared fixed delay, not separate per-waveform phase/gain fitting.

| Observation | Result / limitation |
|---|---|
| Release standalone checks | **1,418,466 checks, zero failures** before final file relocation; final relocated-source rerun recorded alongside artifacts. |
| ASan + UBSan | Same 1,418,466 checks pass with no reported sanitizer finding and `detect_leaks=0`. **LeakSanitizer did not run:** this host reported its ptrace incompatibility. This is not a leak-check pass. |
| Charge/timing | 32.7032–16,744 Hz over six supported 2x host-rate cases; <.01-cent cycle test, <1 ns reset-duration test, continuous frequency-change/reset/zero-current fixtures. |
| Raw nominal pins | Selected reference-condition 3.5 Vpp pin7, 3.2 Vpp triangle, 3 Vpp sine within .1%; exact two-level comparator. This is agreement with the selected model, not independent hardware validation. |
| Comparator | Independent static root checks within 10 ns across duty proxies, outside thresholds and reset-only cases. Stopped exact equality is low (`u > threshold`). |
| Independent host passband | Worst complex harmonic residual **−83.6268 dB** in the tested stationary matrix. The separately printed sparse off-bin probe is explicitly not total alias energy. |
| Actual 255-tap 2x linear chain | Worst desired-hostband residual **−83.0731 dB**; total off-harmonic energy **−92.3188 dBc**, fundamental-referenced and excluding DC. Above-host folds are counted as error, not desired harmonics. A separate actual-filter-response check includes transition leakage for diagnosis. |
| Direct full internal Nyquist | **Fails.** Six retained 88.2/96 kHz internal fixtures have worst residual/off-harmonic result approximately **−18.2 dB**, using the exploratory selected-in-band-power normalization (not a fundamental-referenced dBc acceptance result). The short kernel must not be advertised as qualified over this bandwidth. |
| Existing nonlinear HP/LP chain | **Fails.** Independent review found **107/448 failures through 16.744 kHz** (worst approximately **−20.12 dB**). Including the 23 kHz/48 kHz-host numerical stress subset gives **142/504 failures, worst −6.583 dB**. The nonlinear stages mix upper-internal-band mismatch down before final decimation. Bypass controls pass, so the linear-chain result cannot waive this failure. See the [independent nonlinear review](OSCILLATOR_NONLINEAR_REVIEW.md) and retained harness/results. |
| Allocation | No setup allocation in the implementation fixture, including initial shared-kernel initialization; zero callback `new`/`new[]` calls under modulation. |
| Memory | 22,224 bytes per oscillator, including 464-byte continuation State and 5,192-byte Diagnostics; 355,584 bytes for 16 instances, plus one 24,616-byte shared kernel. Existing Engine/tail/stack totals still need integration-specific accounting. |
| CPU | **Not qualified.** Shared-host standalone measurements leave essentially no full-Engine budget at 96 kHz under moving pitch, and exceed a 256-frame deadline in some 192 kHz cases. Cache/transpose experiments did not establish a robust broad win. Do not infer real-time support from zero allocation. |
| Mutation sensitivity | Reversed restoring-current polarity, altered shaper ordinate, omitted slope-event residual, and independent sine substitution each fail the existing tests. This does not cover the not-yet-integrated Engine note/idle/legacy mutations. |

The six numerical host rates are 44.1, 48, 88.2, 96, 176.4 and 192 kHz. The stationary complex/actual-decimator matrix includes approximately 1,046, 4,186, 8,372 and 16,744 Hz, using coherent nearby pitches. Broad profile-bound checks additionally exercise 8 kHz–1.536 MHz internal rates and RC .001/.1034/10 s. Modulation finiteness and exact copied-state continuation pass; **independent modulation-sideband convergence and the full uncertainty/sensitivity matrix are not complete**.

## Reproduce the current source

```sh
g++ -std=c++17 -O3 -Wall -Wextra -Wpedantic -Isrc \
  tests/ReconstructedOscillatorTests.cpp -o /tmp/reconstructed-oscillator-tests
/tmp/reconstructed-oscillator-tests no-benchmark
# Omit the argument to also print a 16-oscillator high-note timing observation.

# Three mono IEEE-float WAVs, same physical clock and actual 2x decimator.
# One fixed 4.0 volts/full-scale adapter is used for ALL three waveforms.
/tmp/reconstructed-oscillator-tests --render /tmp/nominal-261 48000 261.6 3
```

The render path is audible standalone evidence, not the product signal chain. Cold startup is intentionally included. Generated `/tmp/recon-nominal-{261,8372}-{saw,pulse,sine}.wav` files were checked for valid headers, 144,000 finite samples each, and peaks below full scale with the fixed common adapter. A render manifest is retained, not the large audio binaries. No claim of hardware listening comparison is made.

## Retained failures and provenance

Under `artifacts/oscillator-experimental/`:

- `rejected-minphase/`: reproducible failed full-length homomorphic minimum-phase kernel. Retained-tail truncation L1 was **.07025094721129384**; it was rejected before product integration. This was not a half-length/square-root magnitude conversion.
- `rejected-root/`: failed header snapshot and failing test output. A time-only Newton stop could miss slope events despite small time error. Both voltage and time residuals are now certified, with 48 bounded long-double fallback bisections. No gate was relaxed to repair the failure.
- `qualification/`: standalone linear, direct-Nyquist, corrected-root, sanitizer and render-manifest observations.
- `mutations/`: the four expected-failure observations and reproduction descriptions.
- `nonlinear/`: independently owned full-path harness, metadata and failed variant reports. Longer/lower-cutoff kernels are experiments, not silently selected fixes.

The temporary root source `eb8bed77d44784b6d0fc9145ad9f84d0e1c257f6253b49b8f6920e37d3e81db3` is the last core-location arithmetic snapshot; relocation and explicit unqualified-status comments/metadata do not change its numerical algorithm. Exact-commit CI remains the publishing owner's responsibility.

## Next decision, not a hardware gate

A production candidate needs a reproducible architecture that satisfies **both** nonlinear waveform quality and the complete voice-bank CPU budget. Shared immutable per-engine residual contexts can remove duplicate prepared tables, but are not yet implemented or a measured performance solution. A sharper kernel or a higher-rate oscillator/nonlinear island needs fresh response, modulation, latency/alignment and CPU evidence; moving the cutoff lower merely trades aliases for missing legitimate internal harmonics.

Engine selection, all-rank idle advancement, full active-tail reuse, noise/control-path alignment, host latency notification, exact legacy-off corpus comparison and full-engine renders remain unimplemented for this oscillator. The blocker is demonstrated numerical/real-time behavior, **not a requirement to obtain hardware**.

### Portable exact-state test path

The committed exact copied-state/rejected-setup comparisons use one shared non-inlined runtime tick wrapper, avoiding different inline specialization/FMA paths without relaxing equality. The CMake target is `cs80_experimental_oscillator_tests`, invoked with `no-benchmark`; observed performance is not a correctness assertion. The final local target passes Release (10.10s) and ASan/UBSan (60.98s, leak detection disabled). These results apply to standalone/linear-path scope only; they do not override the nonlinear or realtime failure metadata.
