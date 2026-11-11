# Independent amplitude-envelope RC reference

`scripts/render_amplitude_envelope_reference.py` is a Python 3.10+ standard-library
reference for one fixed-parameter amplitude ADSR. It solves the inferred RC
segments analytically at explicit event and sample times. It does not call, import,
compile, or reproduce a production sample recurrence. No native build is needed
for the envelope exporter.

**Evidence class: engineering inference, not a measured IG00152/IG00159 law.**
The basis is [the declared RC model](ENVELOPE_RC_MODEL.md): a 1.5 attack overshoot,
attack comparator at 1, and ln(1000) decay/release settling. The public source
supports timing ranges and control directions, not these exact charging laws,
sequence thresholds, or comparator tolerances. This is an independent numerical
implementation of a declared assumption, not independent hardware evidence.
The [live-target model](LIVE_ENVELOPE_LEVEL_MODEL.md) describes additional
production policies; this first reference deliberately excludes live edits.

This advances the offline envelope reference only. Runtime DSP, patches, host
parameters, UI, and existing audio/performance/parity acceptance gates are unchanged.
An optional [actual-production comparison](AMPLITUDE_PRODUCTION_COMPARISON.md)
uses a separate command without changing this renderer or its exact event clock.
No joint whole-voice solver is supplied.

## Reproducible render and filter-to-VCA example

All output paths in this example must be new. Start playback at low volume.

```sh
# Fixed ADSR, two attacks, with the second starting on the sounding release.
python3 - <<'PY'
from pathlib import Path
with Path('amplitude-events.csv').open('x') as events:
    events.write('event,time_s,action\n'
                 '0,0,note_on\n'
                 '1,.06,note_off\n'
                 '2,.065,retrigger\n'
                 '3,.08,note_off\n')
PY
python3 scripts/render_amplitude_envelope_reference.py \
  --events amplitude-events.csv \
  --attack .01 --decay .1 --sustain .3 --release .02 \
  --lc .8 --ei -.5 --rate 16000 --samples 1601 --out NEW_ENVELOPE

# Optional: feed the exact generated control CSV to the existing native chain.
cmake -S . -B build/reference -DCS80_BUILD_PLUGIN=OFF -DCS80_BUILD_TESTS=ON
cmake --build build/reference --target cs80_truth_render cs80_vca_reference
python3 - <<'PY'
import math
with open('amplitude-signal.csv', 'x') as signal:
    signal.write('sample,value\n')
    for i in range(1601):
        signal.write(f'{i},{.1 * math.sin(2 * math.pi * 440 * i / 16000):.17g}\n')
PY
python3 scripts/render_reference_chain.py \
  --filter build/reference/cs80_truth_render \
  --vca build/reference/cs80_vca_reference \
  --input amplitude-signal.csv --controls NEW_ENVELOPE/controls.csv \
  --rate 16000 --samples 1601 \
  --hp-hz 200 --lp-hz 2000 --q 1 --ota-level 2.5 --gain unity \
  --wav-gain .1 --out NEW_ENVELOPE_CHAIN
```

The envelope file contains no audio. The optional chain's `reference.wav` uses
an explicit linear gain of 0.1, without normalization or limiting. The chain
preserves samples above full scale; a player/device can clip them. LC is an
explicit constant uncalibrated control and EI is an explicit constant in octaves.
Envelope is dimensionless [0,1]. Neither LC nor EI is inferred from velocity,
touch, volume, hardware voltages, or voice state.

**Interstage limitation:** the existing [filter-to-VCA chain](REFERENCE_CHAIN.md)
receives the sampled `controls.csv`, then interpolates it piecewise linearly.
It does not evaluate this analytic envelope within its solver steps. Subsample
events, slope changes, and tiny snaps are represented only by those sampled
endpoints; they can be smoothed or missed between endpoints. The reference filter
LP is likewise reconstructed from endpoints. This composition is not jointly
continuous-time integrated, a full instrument model, or a chain-wide error bound.
The existing [endpoint-clock study](REFERENCE_CHAIN_CLOCK_STUDY.md) refines the
provided PWL data, not this analytic envelope's event trajectory. To study this
trajectory at another rate, rerender its event source at the new rate/count.

## Input and exact clock contract

Every CLI option shown for the envelope render is required. There are no hidden
parameter defaults. Rates are canonical integers 8000..384000 Hz, with 2..100001
endpoints and an endpoint span `(N-1)/rate` no longer than 10 seconds. N endpoints
mean N-1 intervals. A downstream N-sample WAV's playback duration is N/rate.
The exporter has no sample-rate recurrence and performs no integration tick at t=0.

Fixed parameters:

- A: 0.001..1 seconds; D and R: 0.01..10 seconds. The documented ranges also
  exclude production's <=0.0002-second short-attack shortcut.
- S: [0,1]; LC and EI: [-8,8]. All bounds are inclusive and no clamping occurs.
- The fixed parameters apply to every event. Live A/D/S/R changes, pedal state,
  key allocation, stealing, filter EG, and engine lifecycle are outside scope.

Event header is exactly `event,time_s,action`, followed by 0..4096 rows:

- Canonical contiguous indices starting at 0, and nondecreasing times in
  `[0,(N-1)/rate]`. A header-only file produces idle zero endpoints.
- Actions are exactly `note_on`, `note_off`, `retrigger`, or `reset`.
- LF or CRLF, including the final newline, is required. ASCII only, no quoting,
  spaces, blank lines, extra fields, or comments; maximum 128 bytes per line.
- Numeric tokens use finite representable ASCII decimals, at most 64 characters,
  optional signed exponent with at most three digits and magnitude <=300. Hex,
  underscores, NaN, infinity, underflow to zero, oversized exponents, and duplicate
  CLI options are rejected. No arbitrary-precision exponent allocation occurs.

Sample clocks are the **exact rational** `i/rate`. Event decimals are converted
to exact rationals, not first rounded to float. This distinguishes an event just
before a sample from one just after it, even if their binary64 times would match.
CSV `time_s` is only the 17-significant-digit binary64 display of that clock;
indices and manifest rate define the exact sample time. Audit JSON carries each
clock's exact numerator/denominator plus a binary64 display.

At any time t, ordering is:

1. Finish every autonomous phase boundary at or before t, advancing all remaining
   time through subsequent phases. No fractional interval remainder is dropped.
2. Apply events at t in their file order, resolving each event's zero-duration
   phase completions immediately.
3. Read the endpoint at t after all events at t. The final endpoint's events are
   included; no event beyond that endpoint is accepted.

The state is right-continuous at snaps. The event audit's `before` means **after
all autonomous boundaries at that event time**, but before that event action.
`after` means after the action and its immediate boundaries. It is not the value
at the preceding sample, nor necessarily the left limit at a threshold. Use the
separate transition audit to inspect threshold jumps.

## Closed forms and boundary choices

The initial state is Idle, x=0, gate false. Every exponential is evaluated from
the current phase's start value and start time. Sampling more often cannot change
the stored state or move a phase boundary.

For an attack starting at x0<1:

```
x(t) = x0 + 1.5*(1-x0)*(1-exp(-ln(3)*(t-t0)/A))
V_infinity = x0 + 1.5*(1-x0)
tau_attack = A/ln(3)
```

The comparator ends Attack at exactly `t0+A` in the rational clock, setting x=1
and entering Decay. It does not wait for a rounded exponential to cross 1.
A start already equal to 1 enters Decay immediately. An `expm1` form avoids
cancellation for very short elapsed intervals. In the second half of Attack an
algebraically equivalent expression is used:

```
x(t) = 1 - 0.5*(1-x0)*expm1(ln(3)*(end-t)/A)
```

The nonnegative remaining fraction is computed from exact rational clocks before
conversion to binary64. This avoids values rounding above 1 just before the end,
without broadly clamping results or advancing phase early when a level rounds to
1. The exact comparator clock still determines phase completion. All levels and
transcendental evaluations are binary64; parameters other than the attack clock are used as binary64 values.
An input S extremely close to 1 can therefore round to 1, explicitly visible in
manifest `parameters` alongside its original decimal rational.

Decay starts at 1 and follows `S + (1-S)*exp(-ln(1000)*elapsed/D)`. Release from x0
follows `x0*exp(-ln(1000)*elapsed/R)`. Thus D/R mean time to 0.001 of the original
remaining span, **not** total phase duration. Numerical completion is explicit:

- Decay snaps to S when the remaining absolute span reaches 0.0005, after
  `D/ln(1000) * ln((1-S)/0.0005)` seconds, then holds Sustain.
- Release snaps to 0 at absolute level 0.0002, after
  `R/ln(1000) * ln(x0/0.0002)` seconds, then holds Idle.
- A starting span already <= its threshold completes immediately. This includes
  S=1, S sufficiently close to 1, and note-off at zero or a tiny early attack.
- Threshold equality is completed, not deferred. These deliberate small jumps
  mean the complete piecewise trajectory is not globally value-continuous.

Each positive logarithmic duration is computed once in binary64 and converted
exactly to a rational duration before addition to the phase start clock. Its
numerator/denominator are auditable in `transitions.json`. This is an explicit
floating-point representation of a transcendental boundary, not exact real
arithmetic or a tolerance bucket. Threshold-audit `before_value` is the declared
mathematical left-limit threshold (or origin for an immediate completion), not an
extra sampled probe just before the boundary. Different libm implementations may
change the last bits of durations/values. The closed form needs no h/h2 integration
study; that does not bound the downstream PWL envelope approximation.

Events:

- `note_on` and `retrigger` both set gate true and restart Attack from the current
  binary64 level. `retrigger` is an explicit audit label, not a different math law.
  A held note-on also restarts. The level is not rounded through a float output.
- `note_off` sets gate false and starts/restarts Release from the current level
  unless already Idle. Repeated note-off is accepted. With unchanged R the
  exponential law is unchanged apart from binary64 reanchoring rounding.
- `reset` sets Idle/0 and gate false. It is an explicit discontinuous reset, not
  an acoustic release.
- Gate is independent of phase: held S=0 can be Sustain at zero with gate true.

### Difference from production `Adsr`

The production circuit path advances by a full tick before returning a sample,
checks comparators only on those ticks, clamps at the first qualifying tick,
and discards its remaining sub-tick interval at phase changes. Decay/release use
strict `<` tests against float thresholds; this reference uses continuous boundary
completion at equality against the declared binary64 decimal thresholds. Production
`noteOn` copies its public float level back to the double state. This reference
retains its binary64 event-time level. These deliberate clock/threshold/rounding
differences preclude exact equality, even though the declared RC segment law is
shared. Existing production tests validate their own sample-quantized behavior.

## Outputs, provenance, work limits, and cleanup

- `envelope.csv`: `sample,time_s,envelope,stage,gate,last_event`. `last_event=-1`
  until an event is applied. Stage names are lowercase; gate is 0 or 1.
- `controls.csv`: exactly `sample,lc,ei,envelope`, directly accepted by the
  existing chain at the same rate/count. Values use 17 significant digits.
- `input_events.csv`: exact source bytes, preserving spelling and line endings.
- `events.json`: ordered event actions, exact times, and before/after level,
  stage, gate and phase-start snapshots.
- `transitions.json`: every completed autonomous phase boundary in order,
  including immediate ones, with exact represented times and declared left/right
  levels. Only transitions through the final endpoint are included.
- `renderer.py`: source snapshot, runnable standalone to reproduce the render.
- `manifest.json`: schema `cs80-amplitude-envelope-reference-v1`, completion
  marker, clock/parameters/model/scope, limits, and SHA-256/byte count of every
  other artifact. The manifest is not self-hashed.

Events and the renderer source are each read once with a 1 MiB byte limit; FIFO
and other nonregular inputs fail without blocking. The source hash identifies
the snapshot read by this run, not an authenticated executed-code image or bundled
Python/libm runtime. Same inputs/options/source on the same runtime produce the
same bytes even at another output path. The artifact bundle has a 64 MiB limit.
Work is bounded by N endpoint visits plus at most one event and three autonomous
boundaries per event, with a small initial allowance: `N+4*events+3 <= 116388`.
No unbounded simulation time loop or numerical refinement loop exists.

The output parent must exist. The destination must be new, including no existing
directory, ordinary file, or dangling symlink. All parsing, math, and bundle-size
checks finish before exclusive destination creation. Files use exclusive creates;
the completion manifest is written last. On ordinary failure only owned files
with unchanged device/inode identity and empty owned directories are removed.
Unrelated additions or replacement inodes are retained. No recursive deletion or
existing-file overwrite is used. A crash/concurrent mutation can leave partial
files: consumers must require successful process exit, a completed manifest and
matching hashes. This is not a crash-durable transaction or a malicious concurrent
filesystem-mutation security boundary.

## Tests

```sh
# No native build needed. The one native-chain test is explicitly skipped.
python3 tests/test_amplitude_envelope_reference.py -v

# Full suite, including actual generated controls -> filter/VCA -> float32 WAV.
python3 tests/test_amplitude_envelope_reference.py \
  --filter build/reference/cs80_truth_render \
  --vca build/reference/cs80_vca_reference -v
ctest --test-dir build/reference --output-on-failure \
  -R '^cs80_amplitude_envelope_reference_tests$'
```

The suite uses separately expressed power-law formulas for known zero/nonzero
attacks, decay/release -60 dB, retained release retrigger and off-during-attack;
regresses a retained attack rounding above 1 near its exact boundary and checks
float-adjacent and much smaller rational offsets without premature transitions;
checks independent logarithmic threshold times and remainder handling; covers
zero/sustain-one, immediate tiny-span snaps, reset/gate edges, equal-time and
subsample events and the final endpoint; proves common-time bit identity across
8..384 kHz queries; checks short/long allowed times, maximum endpoint/event bounds,
deterministic snapshot replay and hashes, malformed/bounded input, output collisions,
FIFO rejection and injected failure cleanup preserving unrelated replacements.
The full test passes generated controls through the actual existing chain and
verifies finite nonzero audio plus every WAV float sample against the explicitly
selected gain. This is composition verification, not hardware/whole-voice parity.
