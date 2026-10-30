# Immutable full-Engine continuity experiment

**Not production-integrated.** No host parameter, default, preset field, golden
test or production Engine is changed. The baseline is
`428e4b2bb0b4bcee0a8f3bb8826449f699bdb618`; `reproduce.sh` extracts two private
trees and applies the small `continuity.patch` only to one. Each executable
uses its own tree's headers, including exactly one `Dsp.h`.

## Explicit nominal profile

`ExperimentalVoiceContinuityProfile.h` is a test-only API: version 1, disabled
by default, cold MIDI note 60, initial held K 0.5, deterministic phase and
idle-noise seeds. Invalid profiles are rejected before any state mutation.
Changing an effective profile resets notes/controllers/effect histories;
identical profiles and changes masked by Legacy/Isolation do not reset.
The profile is effective **only** when enabled and Source Performance is the
accepted policy. State diagnostics follow that effective condition.

The enabled adapter clocks eight two-rank kernels regardless of allocation.
Assignment does not reset phase/SVF history; established envelope retrigger
behavior remains. Source filter EGs retain their existing single tick in the
shared-control solver. PANEL/memory filter EGs acquire one continuous tick and
a release gate when retired. Inactive amplitude is explicitly zero, independently
of circuit clocks: retained shared FX, decimator and output noise are not erased.
This is a proposed Engine boundary, not a new hard-mute definition of Panic.

The following are **declared software assumptions, not a recovered KAS model**:

- Electrical TR is high when neither physical-key-held nor `pedalHeld` is true.
  Counting a pedal-held key as electrically keyed is a modern extension.
- Electrical SUS is high in Sustain I. Thus I tracks while keyed and holds K on
  release; II tracks each circuit's retained note/glide/master-tune KI. It does
  **not** make every free CS-80 channel follow the global last MIDI note.
- K is the existing linear-frequency quantity
  `0.5 * exp2((MIDI - 60)/12)`, not a 1 V/oct coordinate. VCO bend, ribbon,
  footage, SUB/LFO and trim remain separate multiplicative overlays. The explicit
  calibration is `midiToHz(60, tuningRef)/0.5`, not the rounded sheet42 anchor.
- Retired velocity, pressure, timbre and member bend become zero. Global
  modulation, glide and the initial-pitch transient keep advancing; the shipped
  ribbon coupling rule remains. These choices are not a complete touch model.
- Active rank RNG draws, assignment's two discarded phase draws, and output
  noise keep the existing shared-stream order. Retired ranks use separately
  seeded idle-only noise streams. This isolates noise-free continuity fixtures
  and avoids shifting existing shared noise; it is **not** a recovered common
  physical-noise topology. Noise-bearing auditions must be labeled accordingly.
- All newly observed cold fields are explicitly initialized. No guessed
  capacitor, leakage, power-up delay or physical phase law is introduced.

Both bypassed SVFs continue processing, with the existing software bypass
routing. The existing production oscillator, SVF, oversampling and 63-sample
output latency remain unchanged. Source/PANEL selection and strict/inferred
provenance remain independent of this profile.

## Declared kernel-domain extension

`kernel-domain.patch` leaves the published v1 kernel and its original timing
evidence untouched. The temporary variant identifies itself as
`voice-continuity-electrical-v2-engine-domain` and changes **only** the allowed
VCO multiplier from 4096 to 262144. No oscillator/SVF law changes.

The v1 cap cannot cover legal combined master/member bends: 48+96 semitones
already give 4096, before ribbon (up to 2) and footage (up to 4). The unchanged
255-tap FIR's two coefficient absolute sums are 1.3481754856870793 and
1.0511648928331851. With sanitized external input in [-1,1], the larger
polyphase bound is 2.6963509713741587. Conservatively combining bend 144,
detune 0.5, three pitch LFOs 6, SUB/mod-wheel depth at most 5 times that bound,
trim 0.07, drift 0.9 and gliss rounding 0.5 semitones gives a multiplier below
113132 after ribbon/footage. Master tune/glide cancel in the VCO-only ratio;
initial pitch is nonpositive. The 262144 domain includes margin for float
rounding. Requested high rates still encounter the **unchanged** production
oscillator's numerical frequency clamp, not a new adapter limiter.

The entire two-rank kernel transaction validates before any K/audio-state
mutation, including masked controls. An adapter rejection increments a visible
counter and fails tests; it is not silently accepted as clamped behavior.

## Status and reproduction

Run `bash docs/voice-continuity/engine-experiment/reproduce.sh [new-output-dir]`.
The output directory must not already exist. It retains baseline/candidate raw
corpora and hashes, compiler information, and test logs. Baseline executables
never see candidate API headers. The independent audit's common raw fixture
builds in both trees; candidate-only tests use its explicit compile definition.

Functional testing is complete: **1,675 owned checks**, **8,051 independent
checks** (native and ASan/UBSan), and five independently rejected mutants.
The original primitive suite passes 151,578 checks against the v2 variant;
the independent kernel audit passes 33,962 with only its temporary multiplier
domain-table endpoint updated. No original repository test is loosened.
Four baseline/candidate corpora totaling **150,968,416 bytes per arm** compare
exactly; the independent audit additionally covers explicit-disabled and
masked profiles. See [functional results](functional-results.json) and the
[independent audit](AUDIT.md), including sanitizer and physical-accuracy limits.

The untimed timing-harness verification passes all **144 pairs / 1,152
callbacks**, checking effective policy, active count, hidden-clock count,
finite output and retired PANEL EG relaxation. Its `--verify` branch does not
read a clock. The first timed result below followed the published, frozen
pre-measurement checkpoint; it was not rerun or replaced.

## Frozen first-run timing contract

[Preregistered criteria](preregistered-criteria.json) define the first result:
all 36 eight-held candidate/baseline median ratios must be at most **1.35**;
all 24 48 kHz zero-audible candidate medians must be at most **0.35 RTF**.
The budget is newly declared for the full Engine, not inherited from the
kernel's different 10% self-comparison. Both cold and post-retirement zero
conditions are measured. "Source both" is Source Performance with tabs 1/3,
**not** isolated parity. Mixed 1/PANEL and both-PANEL scenes are also measured.

Build with `bash build-benchmark.sh VERIFIED-REPLAY-WORK-DIRECTORY`, then run
the resulting `engine-continuity-probe NEW-OUTPUT-DIRECTORY` only after the
source/criteria checkpoint and exclusive measurement window. For untimed
verification use `engine-continuity-probe --verify NEW-OUTPUT-DIRECTORY`.
Each arm is compiled from its own private tree in a separately renamed
namespace; the tiny opaque bridge avoids cross-revision headers/ODR mixing.
There is no LTO. Moving parameter application and render are both timed;
preparation, initial MIDI/release, warmup, input generation, observation,
checksums and allocation are outside. Pair order alternates per callback.

The first run retains all 73,728 callback durations; `analyse-benchmark.py`
recomputes every median/p99/max/overrun before applying the frozen gates. Every
outlier and any gate failure remains evidence. Checksums make work observable
and verify finiteness; enabled-vs-baseline audio/state is intentionally different
and is **not** a parity or quality equivalence test. See
[source hashes](premeasurement-source-sha256.txt), which include both complete
core source trees, kernel/profile, bridge, build script and executable.

No actual AudioProcessor/DAW callback, machine WCET, native platform CPU or
audio-dropout qualification follows from this offline Engine measurement.
Passing recommends a separately reviewed opt-in integration design, never
automatic production activation. A cost failure permits at most one separately
identified exact-cache revision before revising/rejecting this integration
budget; it does not permit replacement of the first observation.

## First whole-Engine result — cost gates pass, not real-time qualification

Measured published experiment commit
`c2db96a317657ac214d7a1bd8d69c1af27d909bc`, tree
`88cb4f0d82cb5b1f08044e3a8dcfe5003d24a3bf`, against pristine `428e4b2`.
The shared Linux environment reported AMD EPYC 9V74 / GCC 14.2.0. The exclusive
agent measurement window had no other known heavy local agent work; that is
**not** a controlled operating-system scheduling or CPU-frequency guarantee.

All **288 rows / 73,728 callbacks** completed with expected policy, active count,
finite samples and zero circuit rejections. Every aggregate was recomputed
from the retained raw observations:

- All 36 eight-held median ratios were **1.02837–1.12591**, below 1.35.
- All 24 48 kHz cold/post-retirement zero-audible medians were
  **0.237665–0.298705 RTF**, below 0.35. Matched baseline zero-audible medians
  were 0.029497–0.083851 RTF; paired incremental cost was **0.190958–0.227332
  RTF**. Continuous idle processing is materially more expensive, not free.
- At 48 kHz, eight-held candidate medians were **0.250092–0.302245 RTF**, versus
  baseline 0.230305–0.292202. At 96 kHz they were 0.498569–0.630595 versus
  0.457355–0.603032. These are ranges across declared rows, not averaged rates.
- **143 overruns remain in the record: 46 baseline, 97 candidate.** The worst
  RTF was baseline 17.446516 (23.262021 ms at 96 kHz / 128, moving mixed-source
  one-held fixture); candidate maximum was 9.053369 RTF in that same fixture.
  A separate 48 kHz / 256 candidate one-held PANEL row had p99 **2.935510 RTF**
  and maximum duration **35.534910 ms**. Some 96 kHz rows also had p99 above1.
  These observations cannot be discarded as "scheduler noise" without causal
  evidence and do **not** establish dropout-free playback or WCET.

See [results and exact gate decisions](first-measurement/summary.json),
[all aggregate rows](first-measurement/results.csv),
[every raw callback duration](first-measurement/callback-times.csv.gz), and
[commit/tree/compiler/hash provenance](first-measurement/metadata.json).
The uncompressed raw SHA is retained in metadata; deterministic gzip only
reduces storage. No raw callback or outlier was trimmed.
The [independent recomputation](first-measurement/INDEPENDENT_AUDIT.md) verifies
all raw statistics, chronological pair ordering, exact rational gate tests,
and all 40 actual source/executable manifest entries without using the primary
analyzer.

**FP-policy limit:** the frozen core harness does not install JUCE's
`ScopedNoDenormals` policy, whereas production `processBlock` does. The first
run did not record MXCSR. A subsequent **untimed**, separate inspection linked
against the exact preserved bridge/core objects observed process-entry
`MXCSR=0x1f80` and FTZ=0/DAZ=0 throughout both arms' retired-PANEL setup/callback
(status flags changed to `0x1fa0`). See [inspection source/output](fp-policy-inspection/README.md).
This corroborates the unaligned default-FP scope; it is not retroactive
first-run telemetry and does not prove that denormals caused any outlier.
A separately identified plugin-aligned FP diagnostic may investigate the
difference, but cannot replace or retry this first result.

**Decision: recommend a separately reviewed, default-off opt-in integration
design.** The declared functional/compatibility and offline cost gates passed;
the extra idle cost and real-time/host qualification limits remain explicit.
There is no performance reason from this experiment to require a coefficient
cache before that design decision. No production activation is performed here.
Existing Legacy, Isolation and previously saved Source Performance sounds must
remain unchanged; a future integration needs its own versioned opt-in state,
mode-transition/GUI ownership tests, and target-native host qualification.

## Diagnostic auditions

`engine_continuity_audition.cpp` produces two-second 48 kHz float WAVs and
per-128-frame traces for both-source and both-PANEL fixtures, with noise controls
zero and no normalization or limiter. Build it separately against each replay
tree's `Engine.o`; define `CS80_BENCH_CANDIDATE=1` only for the candidate. Run
each executable with a **new** output directory. The
[saved trace metadata](audition-traces/metadata.json) records exact event times,
WAV hashes and peaks; compressed CSV traces are retained alongside it. WAVs are
reproducible rather than duplicated in Git. The phase seed aligns the first
sixteen baseline assignment draws purely to make the later gap/reassignment
comparison easier; it is not a hardware startup assertion. Listen at a sensible
level. These are diagnostic examples, not a blinded quality preference test.

See [circuit evidence](../CIRCUIT_REVIEW.md), [kernel audit](../AUDIT.md), and
[the preceding kernel experiment](../../VOICE_CIRCUIT_CONTINUITY.md). The next
decision is integrate/revise/reject based on functional, compatibility and
matched whole-Engine cost evidence, not indefinite quarantine.
