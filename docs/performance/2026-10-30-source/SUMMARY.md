# Results: current source-route dry core baseline

**Useful typical margin, but not a real-time pass.** On this cloud EPYC host, eight-note/two-rank source rendering used a median **0.238–0.290 RTF** (about 24–29% of one callback budget). Matched-selector legacy medians were **0.192–0.248**. Some source callbacks exceeded their deadlines substantially; a DAW/driver/target-machine run is still required.

Measured DSP commit `932c6d4781cad174d90fcc6297b11e685238393d`, GCC 14.2.0 `-O3`, 2026-10-30. See [methodology](METHODOLOGY.md), [all aggregate rows](results.csv), [machine context](context.txt), [metadata](metadata.json) and [source hashes](source-evidence.txt). The reusable harness adds no DSP change.

## Eight held notes, both ranks, all 22 source tabs

Each table row spans 11 paired selector cases, **11,264 callbacks**. RTF is timed wall duration / audio duration; **1.0 exhausts the callback budget**. Maxima are observed values, not upper bounds.

| Mode | Host Hz | Block | Median RTF range | p99 RTF range | Worst max ms | Budget exceedances / 11,264 |
|---|---:|---:|---:|---:|---:|---:|
| Source isolation | 44100 | 64 | 0.238–0.262 | 0.390–0.925 | 3.793 | 32 |
| Source isolation | 44100 | 128 | 0.244–0.258 | 0.342–0.864 | 16.651 | 20 |
| Source isolation | 44100 | 256 | 0.238–0.257 | 0.314–0.846 | 12.091 | 12 |
| Source isolation | 48000 | 64 | 0.253–0.283 | 0.439–2.452 | 13.736 | 46 |
| Source isolation | 48000 | 128 | 0.263–0.277 | 0.373–2.162 | 13.234 | 35 |
| Source isolation | 48000 | 256 | 0.268–0.290 | 0.334–0.676 | 10.865 | 7 |
| Legacy | 44100 | 64 | 0.192–0.219 | 0.317–0.398 | 3.866 | 2 |
| Legacy | 44100 | 128 | 0.195–0.230 | 0.278–0.612 | 9.485 | 8 |
| Legacy | 44100 | 256 | 0.200–0.227 | 0.240–0.579 | 11.783 | 4 |
| Legacy | 48000 | 64 | 0.208–0.248 | 0.316–0.575 | 2.878 | 18 |
| Legacy | 48000 | 128 | 0.211–0.242 | 0.300–0.523 | 10.005 | 7 |
| Legacy | 48000 | 256 | 0.215–0.231 | 0.278–0.464 | 5.235 | 0 |

Source held: **152 / 67,584** callbacks exceeded budget; legacy held: **39 / 67,584**. They are different sonic paths, so these are not equal-feature optimization ratios.

The worst source p99 was **2.452 RTF** at 48 kHz/64, selector pair 2 (Brass 1 / Brass 2), with median 0.283 and maximum **13.736 ms** against a 1.333 ms budget. The largest source-held maximum overall was **16.651 ms** at 44.1 kHz/128. Do not discard these as scheduler noise: in the worst-p99 case, aggregate process CPU (including checks) was 0.528010 s versus about 0.527529 s summed timed wall duration. These different scopes and lack of a per-callback CPU/profile trace do **not** establish an outlier cause.

## Moving controls and idle cost

| Case | Mode | Median RTF range | Worst p99 RTF | Exceedances / 6,144 |
|---|---|---:|---:|---:|
| moving tabs pressure wah | Source | 0.252–0.283 | 1.029 | 16 |
| moving tabs pressure wah | Legacy | 0.209–0.234 | 0.965 | 19 |
| idle quiet | Source | 0.063–0.072 | 0.238 | 0 |
| idle quiet | Legacy | 0.019–0.021 | 0.071 | 0 |
| idle noise | Legacy | 0.019–0.021 | 0.096 | 2 |

Source moving-control rows contain 192 selector-change callbacks and exactly **384 source compilations** (two per event); every unchanged-selector held/idle row reports zero compilations. Warmed idle source processing is measurably nonzero even at exactly zero output; source quiet median is about 6–7% of a callback budget, versus roughly 2% for legacy quiet/noise in this fixture. Noise RMS was nonzero in every legacy-noise row.

## Releases and integrity

- **162 cases, 306 aggregate rows, 229,781 timed callbacks** completed; no failed route, voice-count, recompilation or finite-output assertions.

- All **144 release rows** reached zero active voices. Source rows finished in the minimum observation window (1.000–1.004 s including block rounding); legacy rows took 1.000–1.760 s. This is voice retirement for these patches, **not a global plugin-tail bound or proof of complete output-stage silence**.

- Source release rows had 6 deadline exceedances / 30,252 callbacks; legacy release rows 25 / 33,641. Their medians mix active release with idle processing, not constant eight-voice load.

- Zero nonfinite or absolute-at/above-1 sample values in the measured windows at the common master 0.12. This does not establish safe levels for other patches, master settings or combinations.

- CLI preflight completed 11 cases/19 rows; existing-output and invalid-callback requests returned exit 2, with no new output for invalid arguments. Build completed without warnings.

## Practical next step, not a support promise

Use **44.1/48 kHz and 256 frames as an initial host-audition test setting**, retain output headroom, and check actual underruns with the intended patches, FX and other plugins before reducing the buffer. This data does not establish a guaranteed minimum buffer—even the 256-frame cloud measurements have outliers. The relatively low typical dry-core cost is encouraging, but full-plugin timing, audible latency/dropouts, long-session behavior and target hardware remain unqualified.

No conclusion here qualifies 96/192 kHz, the experimental oscillator, a later source-performance mode, many plugin instances or a mobile/desktop CPU. Preserve the baseline, then repeat on the changed path and the actual intended host.
