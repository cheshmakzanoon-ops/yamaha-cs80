# Matched default-OFF / masked production-continuity overhead

**Preregistered diagnostic; no timing result yet.** Root approved the matrix and
material-overhead flag before measurement. The production core owner must freeze
and pass functional/review checks first; root must separately provide a quiet
measurement window. Preparation and untimed verification do not qualify CPU cost.

This is independent of the earlier **enabled experimental** timing. It does not
replace or modify that evidence, its first outliers, or the targeted FP-policy
diagnostic. Nor does raw-render compatibility alone establish default-path CPU
compatibility. No production source is edited by this probe.

## Frozen question and comparison

Baseline: pristine commit `16735ee2ae23d8c27f6122c8a72e294e31aa4d5b`, core tree
`fd17b98613abd6d18d53f7e02ecf3d9a46c5769e`. The candidate is the owner's first
frozen production integration of the **default-OFF** `source_continuity_v1` Bool.
The baseline has no such parameter: both default and masked cases map that
absent field to OFF, while all existing parameter values and inputs are matched.

`preregistered.json` is the authoritative matrix/statistic/decision contract.
The cost flag is **candidate median − baseline median > max(5% of baseline
median, 1 microsecond)** in any matched pair. It is a material-overhead diagnostic,
not a DAW, deadline-safety or WCET gate. Even no flags would not establish
universally zero default overhead. Every flagged pair and every outlier remain
visible; no trimming, favorable replacement subset or rerun-to-green.

## Matrix and stimuli

**240 pairs / 480 rows / 245,760 raw callback observations**:

- 44.1/48 kHz × 128/256 frames.
- Source-both selectors `{1,3}` and PANEL-both `{13,13}`. This is not all22 tone
  combinations, mixed ranks, memory banks, high rates or a full FX stress test.
- Cold idle, post-retirement idle, eight held notes. Eight-note stimulus is
  `{36,41,46,51,56,61,66,71}`, velocity0.7, channels1–8, MPE off.
- Static and moving controls. Both arms call `setControlFrame` **every callback**,
  including static cases, matching the core transaction boundary used by hosts.
- Five policies: Legacy OFF, Isolation OFF, Source Performance OFF, Legacy
  masked request, Isolation masked request. Masked static keeps request1; masked
  moving alternates request1/0 every32 callbacks, without changing effective
  model. No Source Performance enabled-continuity row is included.
- Both ranks/filter bypasses enabled, fixed master0.12 and MIX0.5, 8-foot,
  pan0. Modern FX, chorus, ring amount, LFOs, arp, analog noise, variation and
  drift are off. Source-selected programmer values follow the existing physical
  selector adapter; PANEL has a defined saw/pulse, short VCA release fixture.
- Moving frame controls: modest pitch/brilliance/resonance/macro1/PW/cutoff
  motion. Core events apply pitch bend, CC11 and master pressure. Original SUB
  EXT and small VCO/VCF depths use a deterministic external sine. The waveform
  and frames are prepared outside the timer. Isolation retains its established
  controller masks in both arms; no enabled source feature is inferred from
  the stored masked request.
- Retired-idle setup starts eight notes, renders8192 frames, releases them,
  and waits untimed for zero active voices (40-second safety cap). No timer
  claims retirement proves complete downstream digital silence.

Exact values are in `bridge.cpp`. This is default **flag-OFF**, not a claim that
every sound parameter is a fresh-install default. Initial note-ons, retirement,
constructor/prepare/reset, memory footprint and lifecycle allocations are outside
this callback-overhead question.

## Matched binary, ordering and FP scope

Each Engine and its bridge compile against their own immutable core headers in
**distinct C++ namespaces**. Only POD configuration, opaque pointers and function
pointers cross `BenchBridge.h`; no Engine layout crosses the boundary. Both arms
link into one executable, using the same flags:

`-std=c++20 -O3 -DNDEBUG -fPIC -Wall -Wextra -Wpedantic -Werror`

No `-march=native`, LTO, fast-math or sanitizer. Compiler/source/object/executable
hashes are recorded before timing. The private candidate field is assigned in
untimed frame preparation, not charged as an artificial bridge-only assignment
inside the Engine timer.

The x86 FP guard sets **FTZ+DAZ ON for both arms before setup**, retains the
incoming rounding/exception-mask controls, checks control bits outside each
callback, and restores the full original MXCSR at exit. It does not change an
OS/security setting. This deliberately also aligns setup/retirement prehistory;
it is not a plugin lifecycle proof, where the host's setup FP scope may differ.
A different architecture needs a separately registered FP policy.

Fisher-Yates case order uses `mt19937` seed167350139 with explicit raw-generator
modulo indexing (not implementation-dependent `std::shuffle`). Original case ID
and execution order are retained in `matrix.csv`. The first arm for a measured
callback is `(execution_pair + callback) & 1`; the other immediately follows.
Creation and warmup orders are also alternated. Every arm gets64 warmup callbacks
and512 measured callbacks from matched prehistory/inputs. Interleaving controls
short-term drift but is not exclusive hardware, cache isolation or DAW operation.

## Timed boundary and retained evidence

The wall timer surrounds the same opaque callback: `setControlFrame`, configured
moving Engine controller calls, and `render`. Input waveform generation/frame
preparation, allocation, warmup, inspection, finite/byte comparisons, hashing,
raw vector append and file writing are outside. They still affect cache and
scheduling context; there is no isolated function-cost attribution.

Before timing, an all-matrix **untimed** run executes the same64 warmups and four
verification callbacks/arm, without wall/process/thread clock reads. Both runs
check bit-identical paired stereo output (including signed zero), finite output,
expected active/base-policy state, effective continuity false and zero effective
kernel diagnostic samples/rejections. This diagnostic API exposes effective
state only, not an independent audit of hidden inactive circuit internals.

Audio is compared directly in memory, with counts and per-arm FNV-1a sample-bit
hashes retained; full audio samples are not persisted. Equal hashes/checksums
alone are not asserted as proof of equality of all hidden Engine state. The
comparison statements are scoped to the executed in-memory checks.

Every measured duration, pair/case ID, callback ID, arm and order is retained in
`callback-times.csv` (lossless gzip may be committed). No measurements are
removed. On failure, already collected raw observations and an explicit partial
status/error are still written; a failed run is not quietly replaced.

Median is the mean of sorted indices255/256. p99 uses nearest-rank
`ceil(.99*N)-1`. Overrun is strict `duration > 1e9*frames/rate`. Mean/median/p99/
maximum and counts must be independently reconstructible from raw data.

Process and Linux thread CPU are sampled once around the **entire paired phase**,
including both arms and untimed input/validation/hash work. Their repeated values
in the two arm rows must not be double-counted. They are not per-arm DSP CPU,
not per-callback CPU, and cannot explain a particular wall outlier.

## Preparation / reproduction

```sh
# Extract the pristine baseline, not whatever the working tree currently holds.
mkdir -p /tmp/continuity-off-work/base
git archive 16735ee src/core | tar -x -C /tmp/continuity-off-work/base
# After owner freeze, copy/hash that exact candidate src/core into a separate root.
docs/voice-continuity/integration-benchmark/build.sh \
  /tmp/continuity-off-work/base /tmp/continuity-off-work/candidate \
  /tmp/continuity-off-work/NEW-build
/tmp/continuity-off-work/NEW-build/continuity-off-probe --verify \
  /tmp/continuity-off-work/NEW-untimed
# Only after integrity/review pass and a root-coordinated CPU window:
/tmp/continuity-off-work/NEW-build/continuity-off-probe \
  /tmp/continuity-off-work/NEW-first-measurement
```

All output/build directories must be new. Preserve the preregistration, exact
probe and candidate source identities, untimed result and **first** measured
attempt. A revised candidate requires a new identified/authorized measurement,
not another chance to obtain a green number. Core-only results exclude JUCE,
plugin UI/queues, driver/DAW graph, other plugins and target-hardware load.
