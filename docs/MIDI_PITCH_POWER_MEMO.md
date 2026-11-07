# Exact MIDI pitch-power memo

**Decision: reject/defer this candidate. This commit contains documentation/evidence only; the optimization and its helper test are inert in `candidate.patch`, not adopted production changes.** Correctness qualification passed, but a clean audio-rate pitch-modulation stress test shows a material 96 kHz CPU regression. The held-pitch benefit does not justify publishing this version as a general optimization.

## Scope and cost

The existing `pow(2.0f, (note - 69.0f) / 12.0f)` result is retained separately for each of the eight voices and two ranks. Only an equal Float32 exponent reuses it. Frequency multiplication, tuning reference, pitch arithmetic, circuit processing, and audio-rate control evaluation retain their order. There is no pitch quantization, approximate exponential, or skipped circuit processing.

The render-thread-owned memo adds 16 pairs of floats: 128 bytes per Engine. GCC/x86-64 `sizeof(Engine)` changes from 277320 to 277448 bytes. Each evaluated voice/rank pitch adds a comparison/branch; a miss executes the original power and stores two floats. A hit avoids that power. No allocation, lock, system call, table construction, or unbounded loop is added to rendering. Memo data is derived, so voice reuse, reset, reprepare, and rate changes need no invalidation. A tuning-reference change always reaches the final multiplication. NaNs miss the cache; NaN payloads and floating-point exception side effects are not an audio-output contract. As with the existing DSP, the normal stable floating-point environment is assumed.

## Qualification (2026-11-07)

Baseline: `185e04c2c50989de33e7b01b446801e1bbf34f50`. Compiler: GCC 14.2.0, Linux x86-64 AMD EPYC 9V74. Both revisions used `-O3 -DNDEBUG -std=gnu++20 -fPIC`, with each revision's own headers and Engine/FactoryPresets sources. Source hashes, raw observations, probe sources, binaries, and byte witnesses were retained with the qualification evidence.

- Standalone memo test: 15,304,168 comparisons pass. Covers repeated and adjacent pitches, changing tuning references, broad Float32 patterns, exceptional values, recovery, and independent memo copies. Non-NaN results require identical bits.
- Independent fresh GCC `-O2 -Wall -Wextra -Werror` and UBSan/float-cast-overflow (no recovery) helper builds also pass all 15,304,168 comparisons.
- Eleven focused JUCE-free core tests pass: memo, parity policy, source performance, filter cache, filter core, voice release, unison/glide, smoothing, pitch-bend range, free-running voices, and modulation matrix.
- Whole-core lifecycle witness: two effect orders, 271872 rendered frames and 759456 serialized values per build, including audio and exposed oscillator/filter/envelope state. Variable blocks, notes/retrigger, reset, reprepare, quality/policy changes, and external input connection changes match byte-for-byte. SHA-256: `fde5c779bad7816fc7c1007c351fde83a0286e8533f95b44fc16208dc3e2d98a`.
- A second lifecycle witness adds separate rank-I/rank-II pitch LFO routes (17.3/19.1 Hz, sine/saw, opposite-signed depths), also matching byte-for-byte. SHA-256: `8bddeb94bb96124a8e284c813edbfd6818e98185113c97823b581208567a01ca`.

## CPU observations

The rate suite uses eight held voices in source-performance policy and normal quality, at 44.1/48/96/192 kHz. Automated cases vary MIDI pitch bend, master tune, and tuning reference every 256-frame block while the engine maintains its audio-rate smoothing. Each invocation warms 16384 frames and times another 65536 frames. Seven trials per case use randomized case/revision order and symmetric baseline/candidate/candidate/baseline (or reversed) pairs, pinned to CPU 3. All 224 audio/state checksums match. CPU time, not elapsed wall time, is compared. Reported percentages are medians of seven paired percentage savings, **not** a ratio of pooled medians.

| Sample rate | Held pitch CPU saving | Pitch/tuning automation CPU saving |
| --- | ---: | ---: |
| 44.1 kHz | 3.64% | 0.91% |
| 48 kHz | 2.72% | 0.18% |
| 96 kHz | 3.13% | 1.65% |
| 192 kHz | 1.60% | -1.25% |

The credible benefit is held-pitch work, especially 44.1/48 kHz where all seven paired trials improved. Automated results have mixed signs and do not establish a robust speedup. The 192 kHz automated median suggests a possible small overhead. Earlier short idle/effects runs were noisy and are not used to claim an improvement. An interrupted broader run is not counted as completed evidence. A first pitch-LFO timing attempt overlapped an unrelated core test and was excluded; only the subsequent quiet-window repeat is used.

The final stress suite layers separate rank-I/rank-II 17.3/19.1 Hz sine/saw pitch LFOs with opposite-signed depths onto blockwise pitch/tuning automation. Seven fresh symmetric trials at each of 48 and 96 kHz produced 56 matching audio/state checksums. These runs started only after other workers explicitly confirmed their builds/tests had stopped.

| Stress sample rate | Median paired CPU saving | Slower candidate pairs | Paired range |
| --- | ---: | ---: | --- |
| 48 kHz | 0.04% | 3/7 | -2.32% to +4.27% |
| 96 kHz | **-3.35%** | **6/7** | -6.22% to +2.31% |

The 96 kHz stress result is a credible regression signal, not a universal miss-path cost estimate. The memo adds comparison/branch/store work when pitch changes, and code layout/cache effects may also contribute; this experiment does not isolate those causes. Further implementation work and fresh qualification would be needed before reconsidering publication.

The rate-suite probe SHA-256 is `fcfdbaac0d724c194823522ea3e650af6f913bd62db485289c3cc784565599f0`; the opposite-rank modulation probe SHA-256 is `4f4ed5d26917cbc22d4d9fada06942c67fcde23f5cca4b71d44460280dbb4695`. Both derive from `tests/FilterCachePerformanceProbe.cpp`, retaining its rendering, snapshot, and timing logic. These are advisory whole-core measurements on one Linux CPU, not plugin/host CPU measurements, deadline guarantees, or hardware performance guarantees. No GUI, processor-host, or plugin validator was run for this change.

## Reproducing the evidence

Exact probe sources, raw paired observations, summaries, source provenance, an inert candidate patch, and a reproduction script are in [`evidence/midi-pitch-power-memo`](evidence/midi-pitch-power-memo/). No binaries or large audio files are committed. `candidate.patch` contains the rejected changes to `CMakeLists.txt`, `src/core/Dsp.h`, `src/core/Engine.cpp`, `src/core/Engine.h`, and the standalone helper test. The documentation branch itself activates none of these changes.

With GCC/Python/taskset installed, create **two separate local worktrees from the original baseline**, and apply the inert patch only to the experimental candidate worktree:

```sh
evidence="$PWD/docs/evidence/midi-pitch-power-memo"
git worktree add --detach /tmp/cs80-pitch-baseline 185e04c2c50989de33e7b01b446801e1bbf34f50
git worktree add --detach /tmp/cs80-pitch-candidate 185e04c2c50989de33e7b01b446801e1bbf34f50
git -C /tmp/cs80-pitch-candidate apply --check "$evidence/candidate.patch"
git -C /tmp/cs80-pitch-candidate apply "$evidence/candidate.patch"
python3 "$evidence/reproduce.py" \
  --baseline /tmp/cs80-pitch-baseline --candidate /tmp/cs80-pitch-candidate \
  --output /tmp/cs80-pitch-results --cpu 3 --trials 7
```

Choose unused worktree paths, an allowed CPU from `os.sched_getaffinity(0)`, and a new output directory. The script builds both probes against each checkout separately, compares full byte witnesses, then checks every timing-run checksum before computing paired summaries. Keep the machine otherwise idle for timing. Exact hashes are same-toolchain evidence, not portable hashes promised across compilers/architectures.

The helper can be built and run independently of JUCE from the **patched experimental worktree**:

```sh
cd /tmp/cs80-pitch-candidate
g++ -O3 -DNDEBUG -std=c++20 -Isrc tests/MidiPitchPowerMemoTests.cpp -o /tmp/cs80-pitch-unit
/tmp/cs80-pitch-unit
```

## Possible next research, not implemented

A separately reviewed experiment could bypass the memo when a pitch path is known to vary continuously, avoiding cache-miss overhead there while retaining held-pitch reuse. That needs a reliable, conservative condition that respects smoothing, rank-local modulation, and transitions; checking the condition also has a cost. This is only a research option, not an adopted design or a promised speedup. Any new candidate needs its own comparison against the original baseline and must retain these rejected results unchanged.
