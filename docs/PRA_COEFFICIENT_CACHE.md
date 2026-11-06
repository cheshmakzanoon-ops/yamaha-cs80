# Reuse held PRA coefficients

The PRA wah model reuses algebraic coefficients while their exact inputs hold. Every optical recurrence and capacitor-state update still executes. No smoothing snap, approximation, voice/quality reduction, parameter change or state-format change is introduced.

## Scope and numerical contract

- Rate/profile-only coefficients are calculated after a valid prepare.
- Optical-dependent coefficients are reused only while the exact optical logarithm remains equal. A changed optical state recalculates them on that sample.
- Every valid prepare invalidates that cache, even with the same pedal/log key. Invalid prepares leave all state unchanged. Reset/setter changes are detected through the same key.
- The original local expression chain is retained before storing final coefficients. An early prototype that stored intermediate `bN`/`cN` values changed native FMA rounding by one ULP and was rejected. No floating-point flags or tolerances were weakened.
- The class grows from 224 to 296 bytes in the tested x86 build, with no new allocation or lock. Use each complete source tree's own headers/core objects; do not mix Engine layouts.

A fixed floating-point environment is assumed within a run. Changing rounding modes, FTZ/DAZ or floating-exception observation between calls is outside this cache contract. Byte-equivalence results below apply to tested compiler configurations, not all compilers or platforms.

## Correctness

Reference: main `a53c1a8b196d494dedd9f0438be173c1b0ecd28d`. Final tested local code revision: `10c162c28ae4c00e8b84505299fac6fa36c6b8a6`.

- `PraCoefficientCacheTests`: 6,468,728 frames and 246,184,978 checks per run. Compares all nine nodes, all four capacitor voltages/currents, optical and setup state against a frozen main implementation. Covers constructor processing, six rates, profiles, held/moving/samplewise controls, adjacent double values, resets, invalid operations, independent instances, maximum-rate settling and all 36 same-key rate-only reprepare pairs.
- Generic Release, native Release with normal FMA, ASan/UBSan plus standard-library assertions all pass. Leak detection is disabled. Independent review verified the frozen header against main and repeated generic/native comparisons. A native frozen-versus-frozen self-check passes.
- Removing valid-prepare invalidation deliberately fails the same-key reprepare case. The final regression does not relax exact comparisons to accommodate the rejected prototype.
- `PraCoefficientEngineProbe`: 18 cases, 1,019,520 frames and 3,792,960 fieldwise values. Independently built baseline and PRA-only files are byte-identical in generic and native builds. Covers PANEL/factory sources, Legacy/Source Performance/PURE, changing polyphony, SUB/EXT, moving wah, release/panic, reset, rate and quality changes. Every case requires finite, non-silent output.
- Fifteen selected JUCE-free core CTest suites pass, including source performance, output taps, external interpolation, oversampling, smoothing, modern FX and PRA circuit/optical integration. Strict compatibility comparisons are byte-identical: Legacy default/tails versus `d56911a7c5f6953cd244457226b126cc567a538b`, V1 numerics versus `22d6a32c48c85f9b813506edcd856f73f4293c2d`, and V2 audio versus `00ceccfacb3cb9845436701da01c4387d53a361a`. Final candidate probes were rebuilt against their own core; the unchanged pinned baseline files from the preceding gate run were reused and their SHA-256 hashes reverified.
- Final full factory validation completes both suites at 48 kHz / 256 frames / 1,024 callbacks, including all 22 factory tabs: baseline 27 cases / 51 rows and performance 39 cases / 100 rows. All 151 rows match the original main runs for every non-timing observation at emitted CSV precision, with zero non-finite output. These separate unpaired runs support coverage and numerical consistency only; their timings are not used for the paired performance claims below.

This is core numerical evidence, not plugin, GUI, DAW, driver, native ARM or physical-hardware qualification.

## Paired whole-core factory measurements

Five preselected workloads were run for baseline, PRA-only and two other experimental variants, in five deterministic randomized palindromic rounds per suite. All 80 executions were serialized on permitted CPU 8 on one shared AMD EPYC 9V74 x86-64 host. The same original factory harness was used, with only a case-vector filter inserted after its existing shuffle. Warm-up, control events, render, timing, tails and invariant checks were unchanged. Settings: 48 kHz, 256 frames, 1,024 held callbacks, no `--quick`.

Each revision's two observations were averaged within a round, then paired with that round's baseline. The table reports the median of five paired CPU-saving ratios, their full observed range, and positive rounds. These are five rounds, not ten independent replicates. Separate median callback RTFs need not reproduce the paired percentage. Thread CPU includes phase checks; callback wall timing covers setControlFrame, active events and render.

Timing build: PRA-only `9aac241` against main `a53c1a8`, GCC 14.2.0, `-O3 -DNDEBUG -std=gnu++20 -fPIC`, no native or fast-math flags. Rebuilding the focused harness from final `10c162c` produces the identical binary SHA-256 `f650a18349b50086da5506ab1c37fdc67a586f0782a00237521a3b7648e2e80b`; later changes add comments and tests. All 360 logged rows match non-timing observations at emitted CSV precision, including against the original full harness. These sums/statistics are not a complete bytewise audio proof.

The strongest supported observations are 3.79% lower median thread CPU for factory circuit idle and 2.78% for moving wah, both positive in all five rounds. There is no clear general dry/live-FX held improvement. Less favorable observations include legacy-idle thread CPU (-3.39%) and live-FX sustain (-1.63%), each negative in four rounds; do not describe this as regression-free.

| Workload / phase | Baseline callback RTF | PRA callback RTF | Paired CPU saved | Round range | Positive rounds |
| --- | ---: | ---: | ---: | ---: | ---: |
| isolation / idle_quiet / warmed_held | 0.11205 | 0.10955 | +3.79% | +0.74% to +4.26% | 5/5 |
| isolation / moving_tabs_pressure_wah / release_tail | 0.13704 | 0.12916 | +4.80% | +1.77% to +10.79% | 5/5 |
| isolation / moving_tabs_pressure_wah / warmed_held | 0.61997 | 0.60377 | +2.78% | +1.52% to +7.85% | 5/5 |
| legacy / idle_quiet / warmed_held | 0.01993 | 0.01993 | -3.39% | -7.68% to +10.42% | 1/5 |
| source_performance / mpe_phaser_delay_drive / release_tail | 0.13987 | 0.13592 | +2.60% | -6.91% to +5.10% | 4/5 |
| source_performance / mpe_phaser_delay_drive / sustain_pedal_hold | 0.81292 | 0.81602 | -1.63% | -11.73% to +4.79% | 1/5 |
| source_performance / mpe_phaser_delay_drive / warmed_held | 0.79994 | 0.80145 | +0.15% | -3.71% to +1.68% | 3/5 |
| source_performance / paired_tones / release_tail | 0.11428 | 0.10962 | +1.18% | -3.13% to +3.32% | 3/5 |
| source_performance / paired_tones / warmed_held | 0.54808 | 0.54438 | +0.46% | -0.22% to +3.99% | 4/5 |

Source Performance dry uses selector 3 (Flute / Brass 3); live MPE phaser/delay/drive uses selector 4 (E. Piano / Bass). Idle and moving-wah cases use the baseline suite. Legacy idle is a diagnostic control, not a guaranteed timing-identical compilation.

Observed callback budget misses also remain: moving wah 130→45 / 10,240 callbacks; live-FX held 272→329 / 10,240; live-FX sustain 8→28 / 470. Shared-host scheduling, frequency and cache effects remain relevant. Five rounds do not qualify a platform or prove small changes in every workload; timing is advisory, never a CI pass/fail gate.

**CPU targets remain unmet.** The observed PRA-only factory callback medians are about 0.10955× realtime idle and 0.54438× for the selected dry eight-voice case, still well above the historical ≤2% idle and 0.15–0.20× eight-voice targets. These are offline core measurements, not a DAW CPU meter or a hard-real-time guarantee.

## Reproduction

Build the JUCE-free configuration and run `cs80_pra_coefficient_cache_tests`, or compile directly:

```sh
g++ -std=c++20 -O3 -DNDEBUG -Isrc tests/PraCoefficientCacheTests.cpp -o /tmp/pra-cache-test
/tmp/pra-cache-test
```

Repeat with `-march=native` without disabling FMA, and with `-O1 -g -fsanitize=address,undefined -D_GLIBCXX_ASSERTIONS -fno-omit-frame-pointer` and `ASAN_OPTIONS=detect_leaks=0`. Define `PRA_BASELINE_SELF_CHECK` for the native baseline self-check.

Compile the same `tests/PraCoefficientEngineProbe.cpp` against each complete source tree's headers and `src/core/Engine.cpp`, render to separate files, and compare them bytewise. The existing strict compatibility probes cover the pinned Legacy/V1/V2 baselines.

For timings, compile `src/tools/source_factory_benchmark.cpp` with each complete core and identical flags, including `src/core/Engine.cpp` and `src/core/FactoryPresets.cpp`. For example, from the source-tree root:

```sh
g++ -O3 -DNDEBUG -std=gnu++20 -fPIC -Isrc src/tools/source_factory_benchmark.cpp src/core/Engine.cpp src/core/FactoryPresets.cpp -o /tmp/pra-factory-benchmark
taskset -c 8 /tmp/pra-factory-benchmark --out /tmp/pra-factory-baseline-new --source-commit "$(git rev-parse HEAD)" --rate 48000 --block 256 --callbacks 1024 --suite baseline
taskset -c 8 /tmp/pra-factory-benchmark --out /tmp/pra-factory-performance-new --source-commit "$(git rev-parse HEAD)" --rate 48000 --block 256 --callbacks 1024 --suite performance
```

Use fresh output directories and substitute a CPU allowed by your environment. The paired investigation's external copy filters the selected cases after the existing shuffle only. Preserve all observations and the source/filter hashes; compare within-round means rather than unrelated medians. Use fresh output directories and an allowed CPU.

For the same focused workload set, apply only this filter to the case vector after the existing `std::shuffle` in the factory harness:

```cpp
std::erase_if(cases, [](const Case& c) {
    const bool baseline = (c.isolated && (c.moving || c.idle))
        || (!c.isolated && !c.sourcePerformance && c.idle && !c.noise);
    const bool performance = c.sourcePerformance
        && ((c.selector == 3 && c.performanceProfile == 0)
            || (c.selector == 4 && c.performanceProfile == 2));
    return !(baseline || performance);
});
```

Leave the rest of the harness unchanged. The original harness SHA-256 was `1bec0df93eeb15a0e18136d434f16ae9ca9f8dc79d8c4ae8b147d60cc20bd25f`; the exact investigation copy, including its comments, was `105ec9ccec588dee3732fb11d13e0926e9691458e77bc1b0109438e599e6700c`.

The original revision-order recipe was Python `random.Random(916103 + trial * 7 + (suite == "performance")).shuffle(order)`, with `trial` from 0 through 4, suites `baseline` then `performance`, and initial `order = ["baseline", "vca", "pra", "combined"]`. Each suite ran `order + order[::-1]` serially. VCA and combined were attribution controls and are not part of this change; to reproduce a new baseline/PRA-only comparison, use a newly recorded balanced two-revision schedule rather than treating it as the original four-revision experiment.
