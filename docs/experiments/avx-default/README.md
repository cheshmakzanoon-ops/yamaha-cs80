# Qualified AVX2 batching default: CPU evidence

The existing exact filter-batch backend is a performance option, not a new filter model. Default enabling is limited by existing compile-time and runtime admission; unsupported builds and calls use the original scalar path. Explicit `-DCS80_ENABLE_AVX2_FILTER_BATCH=OFF` remains supported. Existing CMake caches retain their previous option value; use a fresh build directory to evaluate the new default.

## Measurement and limits

Source baseline: local `bfbd8f6506836c267911b41cc2fc7295a10860aa`, production-equivalent to published compatibility repair `5714483d7a80a7576edfca8b50e1a0c485fb534f`. GCC 14.2 Release, `-O3 -DNDEBUG`, experimental recovery OFF. Compare scalar OFF versus existing AVX2 ON binaries; no fast-math/native tuning. Virtualized Intel Xeon Platinum 8573C, nominal 2.3 GHz, 9 visible logical CPUs; benchmark pinned to CPU 0. Other-core build activity and uncontrolled shared-host scheduling are limitations.

The metric is mean timed callback wall duration divided by represented audio duration. It includes callback control events and render, not a DAW, driver or plugin UI. It is **not system CPU percentage**. Process/thread CPU counters in raw CSV also include validation/observation work and must not be relabeled render-only CPU. This evidence does not meet a 2% target and does not establish Windows/macOS performance.

All runs use 48 kHz and 256 held callbacks after benchmark warmup. Quick runs use 128-frame blocks, full wide run 256-frame blocks. Active quick uses 3 tone selections and 3 performance profiles; wide uses all 33 profiles. Baseline suite includes held, moving-control and quiet/noisy idle cases. See raw CSV scenario, phase and selector columns. Timings are descriptive; exact checksum and finite-output checks are hard requirements.

## Results retained without filtering

Median of matched warmed-source candidate/baseline mean callback ratios:

- Three alternating quick pairs: 0.91755, 0.98204, 0.94939 (36 rows per run).
- Full wide pair: 0.93876 across 33 warmed source cases (100 total rows per run).
- Initial baseline-suite pair was adverse: isolation median 1.48151; legacy scalar-only control median 1.54125. This is retained, not silently removed.
- Three alternating baseline-suite repeats: isolation medians 0.96289, 0.90680, 0.93635; legacy controls 0.95621, 1.00868, 1.03636 (19 rows per run). Quiet isolation idle ratios specifically 0.96297, 1.01619, 0.98095 approximately. Idle timings are noisy and show no claimed speedup.

All paired rows have identical checksums and zero nonfinite outputs. Substantial variation in scalar-only controls argues against interpreting small single-pair differences as stable improvements. Across three active pairs plus the broader workload, admitted AVX2 work consistently improved median callback timing; this motivates enabling the already-qualified backend for its narrowly supported builds, not a universal percentage promise.

## Scope

The backend remains restricted to qualified generic x86-64 GCC math contracts, CPU/OS AVX2 support, supported floating-point environment and bounded eligible stage groups. AppleClang, MSVC, unsafe/FMA contracts and unsupported machines retain scalar behavior. ISA-targeted implementation stays isolated; application baseline ISA is unchanged. Dynamic state/admission/failure-atomicity protections are not weakened.

Fresh default and explicit OFF configuration, historical byte-exact corpus, batch dispatch/fallback and admission tests are required before publication. Fresh GCC qualification passed; final logs and the initial wrong-corpus harness error are retained in [qualification evidence](../avx-default-qualification/README.md). Clang CI remains pending.

The shell files record original experiment commands/paths; CSV copies here are flattened for publication. Full original run directories remain preserved in the working evidence package. Hashes identify actual measured binaries, not hypothetical rebuilt executables.
