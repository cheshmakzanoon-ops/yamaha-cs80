# Read-only CS80 core timing audit — 8bb9b81472c7618e910d385d27fc315a8fd40c8b

This is a temporary serial cloud microbenchmark, not a DAW, audio-driver or hard-real-time qualification. The original audit changed no production source. Its harness, raw timing rows and source hashes are preserved beside this document; binaries and intermediate objects are not committed.

## Build and execution
- GCC 14.2.0, C++20, `-O3 -DNDEBUG -fPIC -Wall -Wextra -Wpedantic`, compiled current Engine.cpp and FactoryPresets.cpp into a fresh temporary static core library, then linked the temporary benchmark. No `-march=native`, LTO, fast-math, sanitizer or debug build.
- `/proc/cpuinfo` reports AMD EPYC 9V74 80-Core Processor, approximately 2596 MHz. Nine CPUs are visible in process affinity 0–8. Virtualized/cloud CPU availability and scheduling are not controlled; no exclusive core, CPU pinning, frequency lock or real-time priority. `sched_getscheduler` returns 0 (SCHED_OTHER). `lscpu` fails because its sysfs topology file is unavailable; cgroup quota files are unavailable, so no quota inference.
- Process-local x86 MXCSR FTZ/DAZ enabled (0x9fc0, initial 0x1f80), matching the plugin's `juce::ScopedNoDenormals` intention; no OS/security setting changed.
- Fresh Engine per case; prepare and buffer/vector setup are outside timings. 0.5 seconds of rendered warmup, then 768 measured callbacks per case, 72 cases total, serial. Case order deterministically shuffled and adjacent off/on pair order alternated to reduce order bias.
- Timed region is per-block `Engine::setParams` (as PluginProcessor.cpp:455 does), expression update when applicable, and `Engine::render`. `steady_clock` wall time per callback. No JUCE, parameter-atomic reads, GUI, MIDI queues/dispatch, plugin wrapper, driver, DAW graph or other plugins included.
- Output finite-value scan/checksum and active-voice assertions occur outside timed regions. Thread CPU column includes these checks and measurement/bookkeeping; it is a context signal, not pure DSP CPU timing. No allocations, heap instrumentation, memory, locks or hard-RT safety conclusions were measured.

## Stimulus
- Host rates 44.1/48/96/192 kHz, blocks 64/256/1024. Engine's fixed 2x internal oversampling is retained (88.2/96/192/384 kHz).
- Eight simultaneously held notes {36,43,48,55,60,64,67,72}, velocity .8, each with two active ranks, MIX=.5. Both saw+pulse, sine=.2, noise=.02, HPF+LPF, resonance, PWM; chorus enabled and ring modulation amount .15; original sub modulation active; no modern FX or arpeggiator. Full exact patch is in `bench.cpp`.
- `sustain_exp`: EXP, expression=.8, output HIGH.
- `moving_wah`: EXP-WAH, block-rate pedal triangle .15–.95 at .75 Hz, output HIGH.
- `moving_wah_highlow`: same moving wah, HIGH/LOW toggled every 16 callbacks (48 event callbacks per case); this is an explicit switching stress cadence, not a claimed typical human rate. Event callback statistics are separate.
- All measured cases must finish with eight active voices, finite outputs, and nonzero rendered peak.
- 768 callbacks correspond to 0.256–17.833 seconds of rendered audio depending on rate/block. In particular, the 192 kHz/64 case samples only part of the pedal cycle; these are bounded microbenchmark distributions, not a long-session reliability run.
- Isolation off and on differ in DSP/sonic features (variation, output conditioning, PRA/output circuits, envelope behavior etc.). Their ratios compare computational cost of these two configurations, NOT equal-feature implementations, fidelity or an isolated cost delta for one new module.

## Reading results
- RTF = callback wall time / callback audio duration; less than 1 is faster than real time, greater than 1 exceeds that individual callback budget. `mean_rtf` uses the sum of measured callback wall times, excluding untimed harness work.
- min/median/p95/p99/max are empirical callback distributions; p99 uses nearest upper sample index and only ~8 tail samples/case. Maximum is the worst observed, not an upper bound. Deadline exceedances include scheduling/virtualization noise and cannot be attributed to DSP alone.
- No hard-RT/DAW safety, actual audio dropout count, maximum supported polyphony, allocation freedom or peak-memory claim follows from this data.

## Serial confirmation of the tight 192 kHz / 64-sample case
After the main sweep, `bench 6000 confirm192` reran all three scenarios in both modes, serially, 6000 callbacks each, with the same half-second warmup. Each case rendered 2 seconds of audio, spanning 1.5 pedal cycles. See `confirm192.csv`. This is still a short bounded repeat, not a reliability soak. Isolation-on medians were 292.508–306.780 us versus a 333.333 us callback budget; p99 494.457–720.049 us; maximum 2.290–4.245 ms; 620–2231 of 6000 callbacks over budget per scenario. Differences between repetitions and off/on order show why small cost ratios and outlier causes are not precisely attributable here.

## Source-only redundant-transcendental candidates (not an optimized patch or measured attribution)
- `src/core/Engine.cpp:1307`: DC-blocker `exp` depends only on sample rate but is written inside every internal sample. The DC state is advanced even in isolation although the isolated output selects the pre-DC mix at lines 1319–1320.
- `Engine.cpp:1033–1034`: when glide is active, `exp` depends on sample rate and portamento, not the current sample/voice. Glide was off in this fixture, so this is not an explanation of measured costs.
- `Engine.cpp:1093`: rank PWM `sin(pwmPhase[ch])` is inside the voice/rank loop though the two phases are global; source-level eight evaluations of the same rank value with eight active voices.
- `Engine.cpp:1104`, `Dsp.h:42`: key-voltage `exp2` is rank-independent but is written inside both rank iterations for each voice.
- `Engine.cpp:973–974`, `Dsp.h:747`: all three modern modulation LFO sine values are calculated even when depths/destinations are disabled, including isolation. Their phase evolution is semantically relevant; do not simply freeze the oscillators.
- `Engine.cpp:1066`: the drift sine is evaluated before multiplication by drift; isolation sets drift to zero in buildSettings. No compiler-disassembly/profiler attribution was attempted; optimizer behavior and exact rendering preservation need separate proof before any change.
- The new output circuit uses setup-cached coefficients; its `process` has no transcendental. PRA `process` uses `exp(opticalLog)` per channel per internal sample to map the moving smoothed optical state (`PraWahCircuit.h:76–77`). This is not classified as unnecessary while moving, and no circuit/quality simplification was made or recommended.

No source, state, sonic, rate, compiler-math or engine-processing optimization was performed.

## Reproduce the measured core build

From a checkout of the named source commit, compile `src/core/Engine.cpp` and `src/core/FactoryPresets.cpp` with the flags above and `-Isrc`, then link this `bench.cpp` against those objects. Check the exact preset source path in that checkout. The normal harness invocation runs 768 callbacks per case; `bench 6000 confirm192` runs the repeat. Record compiler, CPU, source hashes and all timing caveats again rather than expecting identical durations.
