# Optional amplitude-envelope production comparison

`scripts/compare_amplitude_envelope_production.py` compares the published
[independent analytic reference](AMPLITUDE_ENVELOPE_REFERENCE.md) with the actual
`cs80::Adsr` reconstruction path in `src/core/Dsp.h`. Its native helper calls
`noteOn`, `noteOff`, `reset`, `tick(rate, true, true)`, and `getState`; it does not
copy the production envelope equations. Both tools are offline and core-only.
Runtime DSP, UI, faders, presets, and existing acceptance gates are unchanged.

This is a descriptive numerical diagnostic of two implementations of an inferred
RC law. **It is not measured hardware accuracy, whole-voice parity, or legacy
mode parity.** A small error is not proof of the physical model, and a phase
mismatch is not by itself a defect. No fitted tolerance or generic pass/fail
criterion is supplied.

## Run

The original standalone command needs no native build and continues to accept
arbitrary exact decimal event times. Comparison is an explicit, separate opt-in:

```sh
cmake -S . -B build/envelope-comparison \
  -DCS80_BUILD_PLUGIN=OFF -DCS80_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build/envelope-comparison --target \
  cs80_amplitude_production_trace cs80_amplitude_production_oracle

python3 scripts/compare_amplitude_envelope_production.py \
  --production build/envelope-comparison/cs80_amplitude_production_trace \
  --events amplitude-events.csv \
  --attack .01 --decay .1 --sustain .3 --release .02 \
  --lc .8 --ei -.5 --rate 16000 --samples 1601 --out NEW_COMPARISON
```

`amplitude-events.csv` can be the fixture in the standalone reference guide.
The destination must be new and its parent must exist. All reference flags
remain required; their bounds and duplicate-option rejection are unchanged.
The comparison runner currently requires POSIX, Python 3.10+, and the local
helper built from the same source checkout. The helper itself uses standard
C++20 and has no JUCE dependency.

## Exact endpoint contract

There are N endpoints at exact rational `i/rate`, spanning `(N-1)/rate`, with
N-1 production ticks. Event times are parsed as exact decimal rationals. Every
`event_time * rate` must be an integer. A subsample event, even infinitesimally
either side of an endpoint with the same binary64 display, is rejected before
running either render. There is no event rounding, resampling, or compensation.
This restriction applies only to comparison, not the independent reference.

- At row 0, apply events at zero in file order and export. Do not tick.
- At row i>0, advance the previous production state through one full interval
  with `tick(rate, true, true)`, then apply every event at i in file order,
  then export the public state.
- Final-endpoint events are included. `note_on` and `retrigger` call the same
  production method with the same fixed float parameters. `note_off` calls
  `noteOff`, including repeated note-offs. `reset` calls `reset`.
- Gate is a separate diagnostic: on/retrigger set true, off/reset set false.
  It is not inferred from stage or level. `last_event` is the latest event
  index applied, or -1 before any event.

This deliberately adapts production ticks to an endpoint experiment. It does
not claim to reproduce an engine block's MIDI scheduling or voice lifecycle.
No extra tick is applied after an event to make its stage match the reference.

## Why the traces differ

The reference finishes continuous-time phase boundaries at or before a time,
advances the fractional remainder into later phases, and then applies events.
Production observes comparators only once per tick, clamps at that tick, and
discards the remaining interval at the attack-to-decay boundary. The reference
can enter Sustain or Idle immediately for a sufficiently small remaining span;
production may need one or more later ticks. Its strict comparisons use float
thresholds rather than the reference's declared binary64 decimal thresholds.

Production A/D/S/R are parsed directly into the actual binary32 API type.
The manifest puts their passed float values beside the requested decimal,
exact decimal rational, and reference binary64 value. The integer rate and its
passed float are recorded too (every permitted integer rate is exactly
representable in binary32). Binary32 nonzero-to-zero underflow is rejected for
comparison, including extremely small positive S. Representable subnormals are
allowed. The standalone reference's larger binary64 numeric domain is unchanged.

On `noteOn`, including retrigger, production copies its public float output back
to its internal double state. The reference retains its binary64 event-time
level. Neither this parameter quantization nor the note-on float round trip is
corrected away. There is no delay alignment or excluded transient window.

`production_elapsed` is the actual public float diagnostic from `getState()`.
It is not a reconstructed phase age. Attack-to-decay and events reset it, but
decay-to-sustain and release-to-idle do not. It can keep increasing in Idle.
The reference's phase-start audit remains in its own unchanged output files.

For a concrete clock control, `.001` seconds rounds upward as binary32. At
8 kHz, the reference ends attack at row 8; this production implementation stays
in Attack at row 8 and clamps into Decay at row 9. At row 9 the reference has
already advanced into Decay, whereas production is at 1 with elapsed 0. Tests
assert this specific case, not a blanket one-tick tolerance for all cases.

## Artifacts and metrics

- `reference/`: all seven original reference artifacts, byte-identical to a
  standalone invocation with the same parameters and input. This includes its
  unchanged manifest, control CSV, event audit, and source snapshot.
- `production.csv`: `sample,time_s,envelope,stage,elapsed,gate,last_event`.
  State value/elapsed are public floats printed as exact round-trippable
  binary64 values; stage names are lowercase. Time is the binary64 display of
  the exact `i/rate` clock, not an accumulated timer.
- `comparison.csv`: `sample,time_s,reference,production,difference,reference_stage,production_stage,production_elapsed`.
  Difference is production minus reference at the same endpoint.
- `production_metadata.json`: actual passed float parameters, normalized input
  byte count/hash, row/event counts, compile-bound source hashes, compiler
  ID/version, build configuration, and C++ language version.
- `input_events.csv`: exact original bytes. `production_events.csv`: normalized
  `event,sample,action` input with exact integer endpoints, not quantized times.
- `production_executable`, both Python source snapshots, and `sources/`: exact
  executed native binary, reference source, comparison source, and build-bound
  native helper, `Dsp.h`, SHA-256 helper, and CMake source snapshots.
- `manifest.json`: completion marker, parameters, clock, limitations, metrics,
  and SHA-256/byte counts of every other artifact. It is written last.

Metrics use **all N rows**, including t=0, event rows, threshold snaps, and the
last endpoint, with equal per-row weights. For differences `e[i]` and reference
levels `r[i]`, they are:

- Maximum absolute error: `max(abs(e[i]))`.
- RMS error: `sqrt(fsum(e[i]^2)/N)`.
- Reference RMS: `sqrt(fsum(r[i]^2)/N)`.
- Relative RMS error: RMS error divided by reference RMS; JSON `null` when
  reference RMS is exactly zero. No arbitrary epsilon denominator is added.
- Stage mismatch row count, separately descriptive.

The test recomputes every metric from every exported comparison CSV row. No
error is hidden by alignment, dropping rows, normalization, or a fitted gate.

## Provenance, limits, and failure handling

The runner snapshots inputs before execution and passes only the frozen inputs
to children. It executes the reference from its copied source and production
from its copied binary. All four native source hashes embedded at CMake configure
time must match the source snapshots read for this comparison. Source changes
trigger CMake reconfiguration; a stale binary fails rather than silently using
new source labels. This conservatively includes the whole CMake file.

Hashes identify bytes and do not authenticate a build. Compiler flags, linked
system libraries, Python and libm are not bundled or fully identified, and the
comparison source snapshot is not an authenticated executing-code image.
Compiler/libm differences can alter last bits or boundary decisions. Rebuilding
the snapshot sources is not a promise of a bit-identical executable.

The reference retains its 2..100001 endpoints, 8000..384000 Hz, at most ten
seconds, at most 4096 events, 1 MiB input/source files, and bounded work. The
native normalized protocol has the same event bound and 1 MiB byte cap, LF-only
canonical rows, and a final newline. The runner bounds stdout to 64 MiB, stderr
to 16 KiB, and each child to 60 seconds. The complete published bundle, including
the binary and nested reference, may not exceed 64 MiB. Nonfinite states,
parameters, JSON, or CSV numbers fail closed. A nonzero child exit or partial
output cannot produce a completed comparison.

Native output is fully buffered before stdout. The runner validates all
artifacts and metrics before creating the new destination, uses exclusive
creates, and publishes its manifest last. On ordinary failure it removes only
its tracked unchanged inodes and empty owned directories, never recursively
deleting foreign content or overwriting existing destinations. A crash or
concurrent filesystem mutation can leave partial data: require successful exit,
completed manifests, and verified artifact hashes. This is not a crash-durable
transaction or a malicious-filesystem security boundary.

## Verification

```sh
python3 tests/test_amplitude_production_comparison.py \
  --renderer build/envelope-comparison/cs80_amplitude_production_trace \
  --oracle build/envelope-comparison/cs80_amplitude_production_oracle -v
ctest --test-dir build/envelope-comparison --output-on-failure \
  -R '^cs80_amplitude_production_comparison_tests$'
```

Coverage includes a separate direct-Adsr native oracle for every state row;
independently expressed power-law segment and logarithmic boundary controls;
no-tick t=0, tied off/on and reversed ordering, final-row events, off in Attack,
held/release retriggers, reset, S=0/1, threshold neighbors, fractional attack
duration, fast/slow limits, 8/384 kHz and the maximum row count. Exact decimal
float-halfway inputs guard against an accidental intermediate double rounding.
Bundle tests prove unchanged standalone bytes, exact metric recomputation,
snapshot isolation, stale source rejection, malformed/nonfinite/bounded inputs,
partial child output rejection, collision/FIFO rejection, and owned cleanup.
