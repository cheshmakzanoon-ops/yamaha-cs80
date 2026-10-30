# Current source-route core timing baseline

**Offline cloud-core timing, not DAW/driver or hard-real-time qualification.**
Measured DSP source: `932c6d4781cad174d90fcc6297b11e685238393d`; core tree:
`1bacecb7f1a709d67e6aedf7affe7d2e916e7fdc`. The new reusable harness is
[`src/tools/source_factory_benchmark.cpp`](../../../src/tools/source_factory_benchmark.cpp).
An immutable copy of the clean core tree was made before concurrent feature work;
all measured compilation uses that snapshot, not later worktree edits.

## Build and machine

- GCC 14.2.0 (Debian 14.2.0-19), C++20, `-O3 -DNDEBUG -fPIC -Wall -Wextra -Wpedantic`.
  Fresh direct compilation of `Engine.cpp`, `FactoryPresets.cpp` and the harness.
  No `-march=native`, LTO, fast-math, sanitizer or debug instrumentation.
- x86-64 Linux kernel 6.18.55; `/proc/cpuinfo` reports AMD EPYC 9V74 80-Core
  Processor, approximately 2596 MHz. Process affinity allows CPUs 0–8.
  No CPU pinning, priority adjustment, frequency control or exclusive-host claim.
  `lscpu` cannot read the absent sysfs topology; cgroup quota files are unavailable.
- Team heavy builds/tests were paused for the measured run. This does **not**
  prove the virtual/cloud host was otherwise uncontended or exclude preemption.
- Process-local x86 FTZ/DAZ is enabled and restored on exit, like the plugin's
  denormal guard. This is not an OS/security setting change. Other architectures
  report an unchanged denormal configuration and require their own measurement.
- Compiler, source identifiers, clock, timing scope and run configuration are in
  `metadata.json`. `source-evidence.txt` records the clean core-tree hashes.
  `completed.json` exists only when every case finishes without a harness error.

## Cases and stimulus

The complete sweep has **162 fresh-Engine cases and 306 aggregate rows**:

- Rates **44,100 and 48,000 Hz**, callback sizes **64, 128 and 256 frames**; fixed
  Engine 2× internal oversampling is unchanged.
- Eleven two-rank selector pairs, each run with isolation on and off. Pair `n`
  selects tab `n` on **both** ranks; together these exercise **all 22 tabs**.
  This is not an exhaustive 121 cross-pair matrix or a one-rank cost attribution.
- Eight simultaneous notes `{36,43,48,55,60,64,67,72}`, velocity 0.8; both ranks
  enabled, MIX 0.5, fixed master 0.12, 8-foot footage and centred pan. HPF/LPF on.
  No per-tone normalization, limiter or DSP modification.
- One rendered second of untimed warmup, then **1,024 measured held callbacks**.
  These span 1.365–5.944 seconds depending on rate/block. "Warmed" describes the
  execution/cache warmup, not a claim that every long envelope has settled.
- Legacy baselines use the same requested selectors and the existing legacy
  programmer values, through `setControlFrame`. Source cases use the actual
  source adapter. Only the 26 programmer controls are replaced, matching the
  processor; footage, pan and filter bypass remain live. These are **different
  sonic paths**, not equal-feature implementations or an isolated module delta.
- Per held case, a second row times the note-offs and release phase. Render at
  least one second, then stop once `activeVoices()==0`, capped at 40 seconds.
  `tail_voices_complete` states whether that criterion was reached. Voice
  retirement is **not proof of digital silence**, nor a universal plugin tail
  bound. The output stages can retain state and the plugin supports continuous
  noise and arbitrary effect histories outside this fixture.
- Two moving-control cases per rate/block (source and legacy): both selectors
  advance every 32 callbacks; all 22 source identities are traversed; a 0.75 Hz
  triangle sweeps expression 0.15–0.95 in EXP-WAH, with block-rate pressure
  0.1–0.9 on all eight notes. This accelerated switching is a stress cadence,
  not a typical human control rate. Event callback timing is reported separately.
- Three idle cases per rate/block: source quiet, legacy quiet, legacy analog
  noise 0.12. No notes are ever started in them. Source isolation masks analog
  noise, so it is not mislabelled as a source noise benchmark.
- Chorus, ring amount, SUB depths, modern modulation, modern FX, variation,
  arpeggiator and MPE are disabled. This is a **dry core-route baseline**, not a
  worst-case full-patch/FX, many-instance, maximum-release or automation sweep.
- Case order is shuffled with `std::mt19937`, seed 803010. Standard-library
  shuffle ordering is reproducible within this build, not guaranteed identical
  across different standard-library implementations.

## Timed boundary and validation

`steady_clock` surrounds `setControlFrame`, active expression/pressure or
note-off events, and `render`. Route values and frame arrays are prepared outside
the timing; the unchanged control transaction is still applied every callback.
Selector changes compile inside the timed `setControlFrame` call.

Buffer/vector allocation, warmup, output finite checks/checksum, voice-count
checks and CSV output are outside the timed callback. These checks still affect
surrounding cache/scheduling context. The process-CPU column includes untimed
per-phase bookkeeping/checks and is **not pure DSP CPU cost**.

The harness requires accepted selectors/routes, eight voices in every held
callback (zero in idle), finite output and nonzero held output. It checks zero
source recompilations for unchanged selectors and exactly two compilations per
source switching event. Output peak/RMS and values at or above full scale are
reported without clamping. Numeric failures stop with an error; timing-budget
exceedances are observations, **never a platform pass/fail**.

The measured boundary excludes JUCE/APVTS reads, MIDI queues/dispatch, plugin
wrappers, UI, audio driver, DAW graph, disk streaming and other plugins. It does
not test lock/allocation freedom or provide actual audio-dropout counts.

## Interpreting the CSV

- `mean_rtf`: sum of timed wall durations divided by rendered audio duration.
- `median_rtf`, `p99_rtf`, `max_rtf`: callback duration divided by that callback's
  audio deadline. RTF >1 exceeds the individual callback budget.
- Percentiles use the upper empirical index `ceil(q*(N-1))`; p99 on 1,024 held
  callbacks samples about 11 tail observations, not a long-session bound.
- Maximum is only the worst observed. Exceedances include scheduling noise and
  cannot be attributed to DSP alone. Release rows mix active release and idle
  work, so do not compare their medians as eight-voice costs.
- The old 2026-10-29 `setParams`/PANEL benchmark is not a matched baseline for
  this `setControlFrame`/source fixture. Do not infer an exact regression ratio.

## Reproduce

From the recorded DSP commit, place the harness in that checkout (or compile it
against an immutable copy of that commit's `src/core`) and run:

```sh
g++ -std=c++20 -O3 -DNDEBUG -fPIC -Wall -Wextra -Wpedantic -Isrc \
  src/tools/source_factory_benchmark.cpp src/core/Engine.cpp \
  src/core/FactoryPresets.cpp -o /tmp/cs80_source_factory_benchmark
/tmp/cs80_source_factory_benchmark \
  --out /tmp/cs80-source-timing-NEW \
  --source-commit 932c6d4781cad174d90fcc6297b11e685238393d \
  --callbacks 1024
```

Output must be a new directory; existing paths are refused. `--source-commit`
is a **caller-verified label**, not an automatic checkout check. Preserve hashes,
compiler flags, machine context, `metadata.json`, `results.csv` and
`completed.json` for each run. `--quick` restricts to 48 kHz/128 and pairs 0/1/10;
it is a harness preflight, not all-22 coverage. `--callbacks` accepts 16–1,000,000;
short runs do not support reliable tail percentiles.

Verify this dry baseline again in the actual plugin and target DAW/hardware.
A practical starting configuration must leave budget for the host and other
plugins. 96/192 kHz, mobile/desktop CPUs and new source-performance mode need
separate evidence; this report does not qualify them.
