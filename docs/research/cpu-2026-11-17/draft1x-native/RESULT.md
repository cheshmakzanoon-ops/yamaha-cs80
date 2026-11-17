# Draft1x fixed-setup CPU study: scoped measured gain

Run [44090859110](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/44090859110), immutable first attempt at `9dd1352393ed510e41ac1855f7503bb738cfc7a0`, completed 2026-11-17. The independently checked decision is **MEASURED_USEFUL_GAIN**. No production DSP/default was changed.

The default-off research prototype renders at 1x instead of stock 2x. This is a deliberate sound/latency tradeoff, not an exact-output optimization. It reduced primary callback CPU by **47.7719%** in this study. The three primary geometric-mean ratios were 0.5220801, 0.5225031 and 0.5222604. Both the useful-gain threshold and separate 10% material target passed. All 18 paired row ratios were below 1.

## What was measured

One public standard Ubuntu runner, Intel Xeon 6975P-C, GNU 13.3, strict O3 PIC builds, pinned logical CPU0. Source Performance mode at 48 kHz, 256-frame callbacks, eight voices. Four held selector-pair rows (0,4,7,10) are primary; selector4 pressure-triangle and release/retrigger rows are secondary. Each run has 2 seconds warmup and exactly 60 seconds measured audio. Callback thread CPU includes events/control updates/render, excludes output scanning and disk I/O.

| Workload | Stock mean callback CPU / audio duration | Draft mean | Paired geometric-mean ratio |
|---|---:|---:|---:|
| Held selector0 | 38.17% | 19.95% | 0.522844 |
| Held selector4 | 40.05% | 20.89% | 0.521476 |
| Held selector7 | 39.87% | 20.68% | 0.518704 |
| Held selector10 | 39.18% | 20.61% | 0.526128 |
| Pressure triangle | 41.58% | 21.65% | 0.520540 |
| Release/retrigger | 21.51% | 11.27% | 0.524002 |

These ratios are not a DAW CPU meter, total-machine utilization, or a hardware-independent prediction.

## Controls and evidence

All 108 scheduled timed runs and 18 full-workload dry runs completed. Calibration and flag-off controls passed unchanged preregistered gates. Stock and candidate flag-off PCM are byte-identical; every timed result matches its own dry arm/row. No rows were discarded, rerun, or window-adjusted. The measured noise screen was 0.18898% primary-ratio deviation; this empirical screen is not a statistical confidence interval.

Independent integrity checks verify 1,991 evidence-manifest files, 23 admission inputs, 424 retained bindings, all 193 original source files and 546 tests, exactly four patched research source files, 12 actual ELF objects/executables, and 126 PCM references resolving to 12 complete canonical files (276,480,000 bytes). A separate CSV implementation, importing no study validator, checked all 1,417,500 callback rows and 1,417,500 event rows across 126 receipts, including CPU sums and float32 pressure patterns. All 159 recorded commands exited successfully. Compiler executable hashes are receipts only because compiler binaries were not retained.

The artifact identity and expiration are in ARTIFACT.json. This evidence commit contains audits, receipts and hashes, not all raw PCM/binaries. No terminal study has been retried.

## Not qualified / not shipped

Quality acceptance is **false**. The accompanying PCM audit finds nominal-latency-aligned waveform RMS differences of 8.006–26.709% across the six workloads, while Draft/stock RMS levels remain 98.348–99.973%. These are descriptive waveform measurements, not percentages of audible error or distortion; see pcm-quality/RESULT.md for per-row values and the important release-window boundary caveat. The prototype changes rendered waveforms and reported latency from 67 to 8 samples. Nominal latency alignment does not remove frequency-dependent phase or sound differences. Prior short-scene waveform comparisons are not aliasing, distortion, audibility or acceptance measurements.

This is a six-workload core-only result on one x86 Linux VM/session. No actual VST3/DAW gain, host PDC correctness, saved-state integration, cold/concurrent registry behavior, live quality switching, Windows/macOS/older-hardware performance, full factory bank/MPE/FX coverage, or real-time deadline certification follows. Production sources/defaults remain unchanged. Any integration requires explicit acceptance of the quality tradeoff and separate product qualification.
