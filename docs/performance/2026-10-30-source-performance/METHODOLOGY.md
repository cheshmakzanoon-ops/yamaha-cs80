# Source Performance controller/FX timing — v2 harness

**Bounded offline core measurement, not plugin/DAW/driver real-time qualification.**
DSP commit: `d7b6e8510e42cf8bd7f35edad49c81795bc48801`; core tree:
`fd17b98613abd6d18d53f7e02ecf3d9a46c5769e`. An immutable, clean core snapshot
was copied before subsequent plugin-only editing. The measured executable uses
that snapshot and the separately hashed v2 benchmark harness.

The [932c6d4 source-isolation baseline](../2026-10-30-source/SUMMARY.md) and all
its original evidence remain unchanged. **Source Performance is not Isolation**:
the CSV carries explicit `mode` and `source_performance` columns and the harness
checks the Engine's accepted `RenderPolicy` and rank-local source routes.
The old default CLI selects the original `baseline` suite; this run explicitly
uses `--suite performance`. Metadata schema/fixture version is now 2. The v1
harness remains retrievable with `git show d7b6e85:src/tools/source_factory_benchmark.cpp`
(SHA-256 `cf989f64ab2ae2d2272797a154a07247adaee66d123cd8993b89f705ee6fe4d6`).
Use that v1 harness against 932c6d4 to reproduce the old evidence; v2 requires
the newer Source Performance parameter/API and must not be substituted silently.

## Build and environment

GCC 14.2.0, C++20, `-O3 -DNDEBUG -fPIC -Wall -Wextra -Wpedantic`, fresh direct
compilation of Engine, FactoryPresets and the harness. No fast-math,
`-march=native`, LTO, sanitizer or debug instrumentation. Warning-free build.
CPU context reports AMD EPYC 9V74 80-Core Processor, approximately 2596 MHz,
x86-64 Linux 6.18.55, visible affinity 0–8. Sysfs CPU topology and cgroup quota
files are unavailable. No core pinning, priority/frequency change or assertion
of exclusive physical hardware. The team paused heavy builds/tests during the
measurement window; uncontrolled cloud activity remains possible.

The x86 FTZ/DAZ guard is process-local and restored at exit. Other architectures
need their own denormal-policy record and measurements. See `context.txt`,
`source-evidence.txt`, `metadata.json` and `completed.json` for the exact evidence.
The source-commit argument is caller-verified, not an automatic Git checkout check.

## Matrix: 156 cases / 400 aggregate rows

Host rates **44.1 and 48 kHz**, blocks **128 and 256**, eight simultaneous notes
`{36,43,48,55,60,64,67,72}`, velocity 0.8, two ranks, MIX 0.5, fixed master 0.12,
8-foot footage and centred pan, both audio filters enabled. Fixed production
2× backend and 63-sample latency are unchanged. There is no limiter or per-tone
normalization.

For each rate/block, **all 22 source tabs** are covered as 11 two-rank pairs:
selector `n` in rank I and selector `n` in rank II. This is not all 121 rank
combinations. Source Performance runs three profiles for each pair:

| Profile | Controller policy | Modern FX slot type / mix |
|---|---|---|
| `paired_tones` | Neutral dry; source flag on, isolation off | All off |
| `mpe_chorus_reverb` | Live MPE and controllers below; EXP-WAH | Chorus 1 / 0.20; plate reverb 4 / 0.15; third off |
| `mpe_phaser_delay_drive` | Same live controls; EXP-WAH | Phaser 2 / 0.20; tape delay 3 / 0.15; drive 5 / 0.10 |

Current-commit **legacy and isolation dry anchors** each cover paired selectors
0, 1 and 10 at every rate/block. They are separately labelled, not passed off as
Source Performance or remeasured 932c6d4 data. Current dry Source Performance and
these anchors share the neutral parameter settings, but legacy is a different
sonic path. Cost differences are not isolated FX-module or equal-feature ratios.

All profiles disable the separate provisional hardware chorus, ring amount,
base SUB depths, LFOs, macros beyond their neutral defaults, arp, drift/dispersion
and analog noise. The two expressive profiles enable the mod-wheel SUB/filter
transports specified below. These are representative **modest configurations**,
not worst-case all-features/maximum-polyphony/many-instance qualification.
64-frame support is optional in the tool (`--include-64`) but **not measured in
this run**. 96/192 kHz and the experimental oscillator are not included.

## Live controller stimulus and checks

Expressive profiles place one note on each MPE member channel **2–9**; channel 1
is master. Plugin/core MPE is explicitly enabled, with member bend range 12
semitones and master bend range 2. Dry cases use channel 1 as before.

During each expressive held and sustain-hold callback:

- CC11 on master drives a 0.75 Hz triangle, expression 0.15–0.95, through the
  source PRA EXP-WAH pedal. The actual PRA target minimum/maximum is observed;
  a constant target fails the fixture. No direct-pedal substitute masks CC11.
- CC1 on master sweeps 0.15–0.30, with `modWheelSub=0.15`, `modWheelVcf=0.20`.
- Master pitch bend spans ±0.05 normalized (±0.1 semitone).
- Each member gets a distinct phase offset, channel pressure 0.1–0.8, bend
  ±0.1 normalized (±1.2 semitones), and CC74 0.15–0.50. Every callback checks
  each voice's channel, pressure and member bend against the sent values.
- Master CC64 toggles every 64 held callbacks. A dedicated sustain phase then
  sends CC64 down **before note-offs on all eight members**, renders at least
  0.25 seconds, and verifies all eight voices are pedal-held, no longer key-held.
  The first release callback sends CC64 up, actually releasing these notes.

This is block-rate event stress, not a claim about typical MIDI device density.
Waveform inputs are computed before the timed callback; calls into Engine are
inside. Control waveforms restart at each phase, so phase boundaries deliberately
include discontinuous live control updates. Source tabs do **not** change in the
v2 expressive profiles; every phase checks zero redundant source compilation.
The old baseline suite's separate moving-tab scenario is retained unchanged.

MPE state and PRA movement are checked directly; CC74 is dispatched live and
counted, while its semantic cutoff mapping is covered by the separate
`SourcePerformanceTests`. This benchmark does not claim every tone has nonzero
touch depth or that every controller must be audible on every tone.

## Warmup, releases and FX tails

The initial eight note-ons occur before warmup; this is not a note-retrigger or
voice-allocation storm benchmark. One rendered second of warmup precedes
**1,024 measured held callbacks** per case. Depending on rate/block, these cover 2.731–5.944 seconds. "Warmed" does not
mean every envelope or delay/reverb history has reached a fixed state.

Dry cases have held and release rows. Expressive cases have held,
`sustain_pedal_hold` and release rows. After CC64 up, expressive release timing
continues for **at least three rendered seconds**, stopping only when active
voices are zero, with a 40-second cap. Dry release minimum remains one second.
Each CSV row reports its actual duration, final active voices, whether the
voice-retirement criterion completed, and the **last block's output peak**.

Neither three seconds nor zero active voices proves FX/output-stage silence.
A nonzero final-block peak is retained and reported; this is not an offline
export tail cutoff recommendation. The plugin's unbounded capability-level
tail policy is unchanged. Release distributions mix active voices and FX-only
work, so their median is not a held-eight-voice cost.

## Timing boundary and clocks

`steady_clock` measures `setControlFrame`, controller/note events and `render`.
There are no thread-CPU clock calls inside this timed interval. Preparation,
array/waveform generation, buffer/vector allocation, validation/checksum and
CSV writes are outside the interval. Surrounding work still influences cache
and scheduling context.

Both `std::clock` **process CPU seconds** and, on Linux,
`CLOCK_THREAD_CPUTIME_ID` **thread CPU seconds** are captured at phase boundaries.
Both include the phase's untimed per-callback input generation and integrity
checks, but exclude warmup and output formatting. They are not pure DSP CPU
measurements, do not pair CPU time to individual outlier callbacks, and cannot
alone classify each wall-time overrun as contention or compute. Unavailable
thread CPU is represented by -1 and described in metadata.

Mean RTF = summed timed wall duration / audio duration. Median/p99/max RTF =
individual callback wall duration / its audio budget; >1 exceeds that budget.
Upper empirical quantile index is `ceil(q*(N-1))`; 1,024 held callbacks provide
only roughly 11 p99 tail observations. Maxima are observed, never an upper bound.
Do not remove outliers or treat aggregate CPU as permission to dismiss them.

The core-only boundary excludes JUCE/APVTS reads, MIDI queue/parser dispatch,
plugin wrappers, UI, audio driver, DAW graph and other plugins. No hard-RT safety,
heap/lock, audible dropout or universal latency guarantee follows.

## Reproduce

Use the recorded DSP snapshot and separately recorded v2 harness:

```sh
g++ -std=c++20 -O3 -DNDEBUG -fPIC -Wall -Wextra -Wpedantic -Isrc \
  src/tools/source_factory_benchmark.cpp src/core/Engine.cpp \
  src/core/FactoryPresets.cpp -o /tmp/cs80_source_factory_benchmark
/tmp/cs80_source_factory_benchmark \
  --out /tmp/cs80-performance-NEW \
  --source-commit d7b6e8510e42cf8bd7f35edad49c81795bc48801 \
  --suite performance --callbacks 1024
```

The existing CMake target is `cs80_source_factory_benchmark`; timing remains a
manual diagnostic, not a CTest performance pass/fail. Output must be a new
directory; overwrite is refused. `--quick` restricts performance to 48 kHz/128,
paired selectors 0/1/10 (15 cases/36 rows); it is preflight, not all-22 coverage.
Preserve new evidence separately for changed DSP, compiler, flags or machines.
