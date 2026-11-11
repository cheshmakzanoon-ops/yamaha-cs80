# Independent filter-envelope RC reference

`scripts/render_filter_envelope_reference.py` is a Python 3.10+ standard-library
analytic reference for one fixed IL/AL/A/D/R filter envelope with grounded sustain
S=0. It does not import, compile, or call production DSP, nor advance a sampled
recurrence. The existing amplitude renderer is unchanged; only its bounded input,
serialization, and output-ownership helpers are reused, never its envelope law.

**Evidence class: engineering inference, not a measured IG00152/IG00159 law.**
This solves the declared [RC model](ENVELOPE_RC_MODEL.md), including bidirectional
attack and retained-state retrigger. The source documents timing ranges and
control directions, not the overshoot ratio, exact sequence thresholds, or a
complete chip law. The [live-target model](LIVE_ENVELOPE_LEVEL_MODEL.md) supplies
context, but this reference excludes live parameter changes, pedals, voice
allocation, and stealing. It does not change runtime DSP, UI, parameters, patches,
or existing audio/performance acceptance gates. It makes no hardware-fidelity,
production-equivalence, or whole-voice claim.

## Render and feed the existing dynamic filter-to-VCA chain

All output directories and example input files must be new. Start playback at low
volume. A filter envelope is signed control data, **not** a VCA amplitude envelope.

```sh
python3 - <<'PY'
from pathlib import Path
import math
with Path('filter-events.csv').open('x') as f:
    f.write('event,time_s,action\n0,0,fresh_on\n1,.06,note_off\n'
            '2,.065,retained_retrigger\n3,.08,note_off\n')
with Path('amplitude-events.csv').open('x') as f:
    f.write('event,time_s,action\n0,0,note_on\n1,.06,note_off\n'
            '2,.065,retrigger\n3,.08,note_off\n')
with Path('signal.csv').open('x') as f:
    f.write('sample,value\n')
    for i in range(1601):
        f.write(f'{i},{.1 * math.sin(2 * math.pi * 440 * i / 16000):.17g}\n')
PY
python3 scripts/render_filter_envelope_reference.py \
  --events filter-events.csv --il -1 --al 2 \
  --attack .01 --decay .1 --release .02 --rate 16000 --samples 1601 \
  --hp-base-hz 200 --lp-base-hz 2000 --hp-depth .25 --lp-depth -.5 --q 1 \
  --out NEW_FILTER_EG
python3 scripts/render_amplitude_envelope_reference.py \
  --events amplitude-events.csv --attack .01 --decay .1 --sustain .3 \
  --release .02 --lc .8 --ei -.5 --rate 16000 --samples 1601 --out NEW_AMPLITUDE_EG
cmake -S . -B build/filter-envelope -DCS80_BUILD_PLUGIN=OFF -DCS80_BUILD_TESTS=ON
cmake --build build/filter-envelope --target cs80_truth_render cs80_vca_reference
python3 scripts/render_reference_chain.py \
  --filter build/filter-envelope/cs80_truth_render \
  --vca build/filter-envelope/cs80_vca_reference \
  --input signal.csv --controls NEW_AMPLITUDE_EG/controls.csv \
  --filter-controls NEW_FILTER_EG/filter_controls.csv \
  --rate 16000 --samples 1601 --ota-level 2.5 --gain unity \
  --wav-gain .1 --out NEW_FILTER_EG_CHAIN
```

The final `reference.wav` has explicit linear gain 0.1, no normalization/limiter.
Samples over full scale remain representable in its float32 WAV but a player can
clip them. LC/EI and filter depth are explicit uncalibrated assumptions. This
composes existing offline stages, not a jointly integrated whole voice.

### Optional declared endpoint mapping

Supply **all five** `--hp-base-hz`, `--lp-base-hz`, `--hp-depth`, `--lp-depth`,
`--q` options or none. Without them only EG and audits are exported. With them:

```
HP_Hz = hp_base_hz * 2^(hp_depth * EG)
LP_Hz = lp_base_hz * 2^(lp_depth * EG)
Q = explicitly supplied constant
```

EG is dimensionless signed control, depths are octaves per EG unit, bases/cutoffs
are Hz, and Q is dimensionless. This is a **declared endpoint mapping**, not a
measured voltage law or an assumed three-octave hardware calibration. HP/LP bases
are 25..20000 Hz, depths [-8,8], Q [0.35,9]. Each emitted mapped cutoff must also be
finite and inside 25..20000 Hz; invalid outputs are rejected before creating the
bundle, never clamped. Bounds are checked at emitted endpoints only. Unsampled
excursions during short events are not exported or bounded by this check.

The [existing dynamic chain](REFERENCE_CHAIN.md) interpolates cutoff **Hz** and
amplitude control endpoints piecewise linearly. Its between-row cutoff is not
`base * 2^(depth * analytic_EG(t))`, nor even the exponential map of PWL EG.
Subsample events, nonlinear shapes, and snaps can be smoothed or missed. Filtering
and VCA integration tolerances do not bound that reconstruction error. To study
the analytic event source at another rate, rerender both envelopes at that rate;
refining already exported PWL rows is a different experiment.

## Fixed inputs, events, and exact clock

All non-mapping CLI options in the example are required. Inclusive bounds:

- A 0.001..1 seconds; D/R 0.01..10 seconds. These exclude production's short-attack
  shortcut and time floors. D/R mean -60 dB of remaining span, not completion.
- IL/AL [-8,8] dimensionless signed levels. This is a bounded reference domain,
  not a panel-slider range or inferred voltage calibration. AL may be negative;
  attack may move up, down, or have zero span. S is always 0 and has no option.
- Rate: canonical integer 8000..384000 Hz; N: 2..100001 endpoints; endpoint span
  `(N-1)/rate` <=10 seconds. N endpoints mean N-1 intervals. An N-frame WAV plays
  for N/rate seconds. No integration tick occurs at t=0.

Event format is exactly `event,time_s,action`, followed by 0..4096 indexed rows.
Indices are canonical contiguous integers from 0. Times are nondecreasing decimal
rationals in `[0,(N-1)/rate]`. Header-only input stays in initial Idle/0. Actions:

- `fresh_on`: gate true; **assign IL immediately**, then attack AL. This applies
  even if a previous envelope was sounding. It is an explicitly requested fresh
  onset, not an allocator decision. The audit records any level discontinuity.
- `retained_retrigger`: gate true; attack AL from the current binary64 level,
  including a sounding release, held envelope, or untriggered Idle/0. It does not
  assign IL, infer whether a voice is active, or round through a float output.
- `note_off`: gate false; start/restart Release toward IL from the current level,
  unless already Idle. Completed-release Idle holds IL; untriggered/reset Idle
  holds 0. Idle note-off is inert. Repeated active note-off reanchors the same law.
- `reset`: immediately set Idle/0 and gate false; not an acoustic release.

Files are ASCII LF/CRLF with final newline, <=1 MiB, <=128 bytes per line, no
spaces/quotes/extra columns/blank rows/comments. Decimal tokens have <=64
characters and exponent magnitude <=300 with <=3 exponent digits. NaN, infinity,
hex, underscores, underflow to zero, duplicate options, and invalid bounds are
rejected. Parameters are binary64 except the exact rational attack duration;
manifest parameters record original decimal rationals and converted values.

Sampling uses exact rational `i/rate`; events use their exact decimal rationals.
CSV `time_s` is only a binary64 display. Audits retain exact numerators and
denominators. Events infinitesimally before/after an endpoint remain distinct.
At each requested time: finish all autonomous boundaries at or before it, apply
same-time events in file order resolving immediate boundaries after each action,
then read the endpoint. Remaining time after a boundary is never discarded.

## Closed forms and completion policy

Initial state: Idle, x=0, gate false, regardless of IL. With phase origin x0 at t0:

```
attack:  x = x0 + 1.5*(AL-x0)*(1-exp(-ln(3)*(t-t0)/A))
         V_infinity = x0 + 1.5*(AL-x0); tau = A/ln(3)
decay:   x = x0 * exp(-ln(1000)*(t-t0)/D)
release: x = IL + (x0-IL) * exp(-ln(1000)*(t-t0)/R)
```

Attack reaches AL at exactly rational `t0+A`, then enters Decay. The same law
works above/below AL and for negative levels. A start already equal to AL enters
Decay immediately. First-half attack uses `expm1` to preserve short elapsed
intervals; second-half attack evaluates the equivalent stable expression
`AL - 0.5*(AL-x0)*expm1(ln(3)*(end-t)/A)`. The exact rational remainder is converted
to binary64 only after subtraction. No arbitrary clamp is used; rounding a level
to AL just before the boundary does not end the phase early.

Decay snaps to S=0, then holds Sustain. Release snaps to IL, then holds Idle.
Both complete at absolute remaining span epsilon: **IEEE-754 binary32 `0.0008f`
promoted to binary64**, exactly `13743895/17179869184`, about
0.0007999999797903001. This deliberately preserves the declared literal rather
than substituting binary64 decimal 0.0008. A span already <=epsilon completes
immediately, including a zero span. Otherwise duration is
`T/ln(1000) * ln(abs(x0-target)/epsilon)`; `log1p` preserves durations for spans
only one representable value above epsilon.

Each positive logarithmic duration is computed once in binary64 and converted
exactly to a rational for comparisons. This represents a transcendental boundary;
it is not exact real arithmetic or a tolerance bucket. Binary64/libm rounding
can affect last bits of values/durations. At equality completion is authoritative
and right-continuous. Threshold audit `before_value` records the declared signed
threshold left limit (or origin for immediate completion), not another sampled
probe. These small explicit snaps make the full trajectory discontinuous.

Degenerate cases are intentional: IL=AL skips Attack but Decay still goes to 0;
AL=0 completes Decay immediately after Attack; IL=AL=0 reaches held Sustain/0
immediately. Gate and stage are independent. Retained current=AL skips Attack,
even when IL differs. A fresh-on IL assignment and subsequent zero-duration snaps
can occur at the same instant.

Production `FilterEnv` instead advances a full tick before sampling, uses strict
`<0.0008f` tests only on ticks, and discards fractional remainder when changing
phase. It has float parameters/output, different event/tick timing, and additional
live-target policies. This analytic reference uses continuous equality completion
and binary64 event values; it is not a byte/sample oracle for production.

## Artifacts, provenance, and cleanup

The new-only output directory contains `envelope.csv`, `events.json`,
`transitions.json`, exact `input_events.csv`, `renderer.py`, the exact compiled
helper source `render_amplitude_envelope_reference.py`, optional
`filter_controls.csv`, and last-written `manifest.json`. `after_action` in event
audits exposes the assigned IL/retained level before zero-duration completions;
`before` follows autonomous boundaries at that time; `after` follows the action
and its immediate completions. Autonomous transition audits remain separate.

Manifest records parameters, mapping, limits, clock/snap policy, count, and SHA-256
plus byte size of every other artifact. Replay `renderer.py` alongside its helper
snapshot to a new output path. No helper bytecode cache is created. Source hashes
identify captured bytes, not an authenticated interpreter/execution image. Same
runtime output is deterministic; different libm implementations may differ.

The aggregate bound is N+4*event_count+3 work units, <=116388; at most three
zero-duration boundaries occur per event. Bundle limit is 64 MiB. Generation and
validation finish before output creation. Existing directories/files/symlinks,
including dangling symlinks, are rejected. Exclusive writes and nonrecursive
cleanup remove only unchanged owned inodes/empty owned directories on ordinary
failure. Foreign files or replacement inodes remain. This is not crash-durable or
a security boundary against malicious concurrent filesystem mutation. Consumers
must require successful exit, completed manifest, and matching hashes.

## Verification

```sh
# Stdlib analytic/CLI coverage; one chain test is explicitly skipped.
python3 tests/test_filter_envelope_reference.py -v
# Includes generated amplitude + filter controls -> actual native chain -> WAV.
python3 tests/test_filter_envelope_reference.py \
  --filter build/filter-envelope/cs80_truth_render \
  --vca build/filter-envelope/cs80_vca_reference -v
ctest --test-dir build/filter-envelope --output-on-failure \
  -R '^cs80_(filter_envelope_reference|amplitude_envelope_reference|reference_chain|reference_chain_clock)_tests$'
```

Tests use separately expressed power/log formulas, signed upward/downward attacks,
negative targets, current above/below/equal AL, exact boundary neighbors, float
threshold neighbors, fractional remainder, off/retrigger/reset/ties, degenerate
levels, shared-time identity across 8..384 kHz, strict input/work/output bounds,
source/helper replay and hashes, collisions/FIFOs, and injected cleanup failures.
The native composition checks exact consumed generated controls, finite nonzero
audio, and every WAV sample against explicit gain 0.1. These establish the stated
software reference and composition, not independent hardware evidence.
