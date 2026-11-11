# Optional filter-envelope production comparison

`scripts/compare_filter_envelope_production.py` compares the unchanged
[independent filter-EG reference](FILTER_ENVELOPE_REFERENCE.md) with actual
`cs80::FilterEnv` reconstruction calls. `src/tools/filter_production_trace.cpp`
includes `core/Dsp.h`, calls its public API, and exports the actual returned
state. It contains no copied envelope recurrence. This is an offline, core-only
stage qualification, following the amplitude comparison's bounded snapshot and
endpoint contract with **filter-specific event and state semantics**.

It measures differences between two implementations of an inferred RC law.
It is **not a hardware-accuracy, whole-voice, legacy-parity, performance, or
release gate**. Runtime DSP, UI, faders, presets, the standalone references, and
existing acceptance criteria are unchanged. There is no fitted tolerance,
alignment adjustment, exclusion window, or aggregate pass/fail score.

## Run

```sh
cmake -S . -B build/filter-comparison \
  -DCS80_BUILD_PLUGIN=OFF -DCS80_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build/filter-comparison --target \
  cs80_filter_production_trace cs80_filter_production_oracle

python3 scripts/compare_filter_envelope_production.py \
  --production build/filter-comparison/cs80_filter_production_trace \
  --events filter-events.csv --il=-.7 --al=1.3 \
  --attack .0137 --decay .02 --release .03 \
  --rate 8000 --samples 801 --out NEW_COMPARISON
```

Example `filter-events.csv` (including the final newline):

```csv
event,time_s,action
0,0,fresh_on
1,.025,note_off
2,.035,retained_retrigger
3,.075,note_off
```

The runner requires POSIX and Python 3.10+. The native helper uses C++20 and no
JUCE. `--production` must identify a helper built from this checkout. The output
must be new, including no pre-existing symlink, with an existing parent.
Reference options and validation remain unchanged. Optional `--hp-base-hz`,
`--lp-base-hz`, `--hp-depth`, `--lp-depth`, and `--q` must be supplied together.
Their original `filter_controls.csv` is preserved under `reference/`; it is not
compared with a production filter or used to synthesize production audio.

## Scope and exact event clock

The fixed parameters are IL and AL in [-8,8], A in [.001,1] seconds, D and R in
[.01,10] seconds, and the fixed decay/sustain target 0. This domain excludes the
native A<=.0002 fast-onset assignment and live parameter editing. No pedals,
voice reuse policy, voice allocation, engine scheduling, or mode switching is
simulated. IL/AL here are signed control levels, not calibrated panel positions.

There are N endpoints at exact rational `i/rate`, for N-1 production ticks.
Events use the reference's exact decimal parser. Every `event_time * rate`
must be an integer. Even a time infinitesimally above or below an endpoint,
with an identical binary64 display, is rejected before executing either child.
The standalone reference still accepts arbitrary exact decimal event times.
There is no quantization or silently changed event meaning.

1. At row 0, apply its ordered events, then export. **No tick at t=0.**
2. At row i>0, call `tick(rate, true, true)` for the preceding interval, then
   apply all events at i in file order, then export `getState()`.
3. Include final-endpoint events. No post-event tick is inserted to make stages
   agree. Gate is an independent diagnostic, not inferred from level or stage.

Events map directly to the existing production API:

- `fresh_on`: `noteOn(IL, AL, A, D, R, 0, false)`. Assign float IL and restart
  Attack, even if the envelope is already sounding. Gate becomes true.
- `retained_retrigger`: the same call with `true`. Preserve the **private double
  capacitor-state proxy**, restart Attack from that level, and set gate true.
  Unlike Adsr, this does **not** round the origin through the public float.
- `note_off`: `noteOff()`. If not Idle, restart Release toward IL and reset its
  elapsed time, including a repeated off during Release. Idle off is a no-op.
  Gate becomes false in either case.
- `reset`: `reset()`, giving Idle, level 0, elapsed 0, gate false.

Initial/reset Idle stays at 0, even if IL is nonzero. Completed-release Idle
stays at IL. No live `setLevels` or `setTimes` calls are made, so live target
tracking is outside scope. A fresh zero-span attack, including IL=AL!=0, stays
in production Attack until a later tick. The continuous reference completes
that attack immediately. This disagreement is measured, not hidden.

## Why levels and stages differ

The continuous reference completes phase boundaries at their times and advances
the remaining fractional interval into subsequent phases. Production checks
comparators once per tick, clamps at that tick, and discards the attack boundary
remainder. Both use the same promoted binary32 `0.0008f` settle threshold;
production uses strict `<` comparisons, while the reference completes at
continuous equality, including immediate completion when a span is already
small enough. Small residual spans and zero-span attack can therefore give
stage differences even when exported levels agree.

Native IL/AL/A/D/R are parsed directly from decimal to binary32, with exact
mathematical domain checks before rounding. The report records the requested
original token, exact rational, reference binary64, and actual passed float.
Negative levels, subnormal nonzero floats, and exact halfway decimals are
supported; nonzero-to-zero float underflow is rejected. Integer rates in
8000..384000 are all exactly representable in binary32. Signed zero is
numerically equivalent in the comparison, with its requested token retained.

For example, A=.001 rounds upward as binary32. At 8 kHz, a nonzero-span
reference attack finishes at row 8, but this production implementation reaches
Decay at row 9. Production at row 9 is clamped to AL with elapsed 0; the
reference has already decayed through the fractional remainder. Tests establish
this particular example, not an allowed delay or blanket tolerance.

The public value and elapsed diagnostics are floats; internal reconstruction
state is double. `production_elapsed` is the actual public field, not inferred
phase age: Attack-to-Decay and events reset it, but Decay-to-Sustain and
Release-to-Idle do not, and it increases while Idle. Retained retrigger keeps
private state. Its test includes an independently derived case that
specifically distinguishes it from an incorrect public-float round trip.

## Artifacts and descriptive metrics

- `reference/`: all original standalone artifacts, byte-identical for the same
  inputs and parameters. It contains its unchanged manifest, renderer,
  amplitude IO-helper source, audit, transitions, envelope, and optional mapping.
- `production.csv`: `sample,time_s,envelope,stage,elapsed,gate,last_event`.
- `comparison.csv`: `sample,time_s,reference,production,difference,reference_stage,production_stage,production_elapsed`.
- `production_metadata.json`: actual floats, normalized input byte count/hash,
  samples/events, compiler ID/version, configuration, C++ language version, and
  compile-bound source hashes.
- Original `input_events.csv`, exact integer-endpoint `production_events.csv`,
  frozen `production_executable`, Python renderer/comparison/IO-helper sources,
  and native source snapshots under `sources/`.
- `manifest.json`: completion marker, clock, event mapping, limitations, metrics,
  and byte count/SHA-256 for every other artifact. Written last.

Every metric uses **all N rows**, with equal per-row weight: t=0, event rows,
threshold snaps, and the final endpoint. For `e=production-reference`, report
maximum absolute error, `sqrt(fsum(e*e)/N)`, reference RMS, RMS divided by
reference RMS (JSON null if reference RMS is zero), and stage mismatch count.
No rows are removed or aligned; zero-reference RMS gets no artificial epsilon.
Tests independently recompute each metric from every comparison CSV row.

## Provenance, bounds, and failure behavior

Inputs are frozen before children execute. The reference executes from its
copied renderer and copied amplitude IO-helper source; production executes from
its copied binary. Native `filter_production_trace.cpp`, `Dsp.h`,
`TruthInputSha256.h`, and `CMakeLists.txt` hashes embedded at configure time must
match source snapshots at comparison time. Changes trigger reconfiguration;
stale helpers fail closed. Existing standalone source files are not modified.

These are byte identities, not authenticated build attestations. Python,
compiler flags, system libraries, and libm are not bundled or fully identified.
The comparison source snapshot is not an authenticated executing-code image,
and rebuilding from snapshots need not produce identical binary bytes or
last-bit results. State comparisons do not qualify physical circuit accuracy.

Bounds: 2..100001 endpoints, 8000..384000 Hz, at most ten seconds, 4096 events,
1 MiB input/source snapshots, and the reference's bounded work. Native event
input is canonical LF-only CSV with a final newline. Child stdout is capped at
64 MiB, stderr at 16 KiB, each execution at 60 seconds, and the complete bundle
including executable and duplicate reference snapshots at 64 MiB.

Native output is buffered until all inputs and states validate. The runner
rejects stale provenance, malformed/nonfinite/truncated child output, nonzero
child exits, unexpected reference artifacts, and hash mismatches before
publishing. It uses exclusive writes and removes only owned unchanged inodes
and empty owned directories on ordinary failure. Foreign replacements are
preserved. This is not crash-durable or a malicious-filesystem security boundary;
require successful exit, completion manifests, and valid artifact hashes.

## Verification

```sh
python3 tests/test_filter_production_comparison.py \
  --renderer build/filter-comparison/cs80_filter_production_trace \
  --oracle build/filter-comparison/cs80_filter_production_oracle -v
ctest --test-dir build/filter-comparison --output-on-failure \
  -R '^cs80_(filter_production_comparison|amplitude_production_comparison|filter_envelope_reference|amplitude_envelope_reference|reference_chain|reference_chain_clock)_tests$'
```

The broader CTest selection also needs the amplitude trace/oracle, truth renderer,
and VCA reference targets built. Tests keep separate direct-native API plumbing
checks and independent power/log analytic-law checks. Coverage includes signed
up/down/equal targets, nonzero IL=AL, fresh/retained onset, private-state retention,
Attack off, release/Idle/reset, repeated off, ordered ties, final events, exact
alignment, fractional attack, strict threshold neighbors, rates/limits,
standalone bytes including optional mapping, every metric/hash, direct decimal
float rounding, malformed input/output, budgets/timeouts, collision/FIFO guards,
snapshot isolation, stale hashes, and owned-inode cleanup.
