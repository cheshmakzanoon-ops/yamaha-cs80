# Event-aware analytic envelope and conditional VCA

`scripts/render_event_aware_envelope_vca.py` is a separate Python 3.11+ standard-
library reference for the first bounded envelope/VCA composition. It imports
only the existing independent `Envelope` model from
`render_amplitude_envelope_reference.py`; no production renderer is called.
Existing exporters, contracts and production sources are unchanged.

## What gap this addresses

The existing [sampled envelope](AMPLITUDE_ENVELOPE_REFERENCE.md) can feed the
[filter/VCA chain](REFERENCE_CHAIN.md), but that chain reconstructs the sampled
envelope linearly. Between-sample events, exponential curvature and stage snaps
are consequently not evaluated at their actual times within the VCA solver.

This new composition instead uses the analytic envelope continuously over each
smooth phase, splits at its actual events/stage boundaries, and solves the VCA
coupling convolution there. Output samples query that solution; they do not
become integration anchors. It is **not** a replacement chain, time-varying audio
solver, whole voice, new envelope law, production comparison or hardware result.

## Bounded model

Audio input, LC and EI are explicit constants. LC and initial LI must be
nonnegative, so LI cannot cross the gain floor within a segment. Negative
LC/LI, live control/ADSR changes, arbitrary audio, oscillators, filters, pedals
and voice allocation are excluded. Initial coupling is explicit and may be
negative. No unspoken silent prehistory or time-zero sample advance is applied.

The conditional equations follow `src/reference/IG00151Reference.h`:

```
LI(t) = LC + (LI0 - LC) exp(-t/tauLI)
E(t) = target + (E0-target) exp(-lambda*t)
raw(t) = 2^EI * (input + offset + feedthrough) * LI(t) * E(t)
dCoupling/dt = (raw-Coupling)/tauAC
output = raw-Coupling
```

`tauLI = (180k || 560k || 270k || 270k || 270k) * 10nF`;
`tauAC = 1uF * 100k`. The offsets are the existing float32 constants `0.0015f`
and `0.001f` promoted to double, not their ideal decimal substitutes. The
nonnegative restriction removes the only `max(0,LI)` crossing from this first
scope; it does not silently change the original conditional gain equation.

The product has at most four exponentials. For each exponential rate `r`, the
coupling integral uses

```
K(q,r,h) = exp(-min(q,r)*h) * (-expm1(-abs(q-r)*h))/abs(q-r)
K(q,q,h) = h * exp(-q*h)
q = 1/tauAC
```

Only nonpositive exponential arguments are evaluated. Coupling is
`C0*exp(-q*h) + q*sum(coefficient*K)`. At very short intervals, direct subtraction
of four nearly identical kernels can lose an initially zero LI/envelope signal.
When all dimensionless rates are at most 0.5, an equivalent local power-basis
calculation evaluates LI and envelope increments and their coupling integral
with 40 terms. `expm1` and `fsum` limit cancellation. This is a binary64
roundoff-aware evaluation of the analytic model, **not a rigorous error
certificate**; independent verification must retain its numerical scope.

### Evidence class

The LI capacitor/branch values are source-derived with an inferred ideal-source
loading assumption. AC coupling, multiplier/offset/feedthrough behavior and
unit mappings remain the declared inferences in [IG00151_VCA_MODEL.md](IG00151_VCA_MODEL.md).
Envelope overshoot, RC laws and snap thresholds retain the inferences in
[ENVELOPE_RC_MODEL.md](ENVELOPE_RC_MODEL.md), not a recovered hardware IC law.
Nothing here calibrates engine units to electrical volts or establishes CS-80
hardware parity.

## Clock and event semantics

- Sample times are exact `Fraction(i,rate)`; decimal event times are parsed as
  exact rational numbers with the existing parser.
- Autonomous boundaries precede events at an equal time. Same-time events retain
  file order; endpoint snapshots follow all events at that time.
- Attack duration is rational. Existing decay/release logarithmic durations are
  calculated once in binary64 and represented as exact rational values thereafter.
- The existing envelope resolves threshold snaps and zero-duration stages.
- LI and coupling remain continuous across events/snaps. Raw/output may jump
  immediately when the envelope jumps, including reset. Those jumps must not be
  smeared into fictitious pre-event forcing.
- State anchors change only at actual event/autonomous-phase boundaries. Querying
  additional output samples does not affect the state at shared sample times.

## CLI and artifacts

Every option below is required. The output directory must be new and its parent
must already exist. Input events use the existing `event,time_s,action` CSV
schema (`note_on`, `note_off`, `retrigger`, `reset`).

```sh
python3 scripts/render_event_aware_envelope_vca.py \
  --events events.csv --attack .01 --decay .1 --sustain .3 --release .02 \
  --audio-input .2 --lc .8 --ei -.5 --initial-li .1 --initial-coupling 0 \
  --rate 16000 --samples 1601 --out NEW_ENVELOPE_VCA
```

Bounds: rate 8,000–384,000 Hz; 2–20,001 endpoints spanning at most two seconds;
1,024 events; 1 MiB input events; 32 MiB output bundle. ADSR bounds match the
existing envelope tool. Audio input, EI and initial coupling are in [-8,8];
LC and initial LI are in [0,8]. EI is in octaves, envelope is dimensionless;
other quantities are uncalibrated engine units. Constant input is an explicit
forcing experiment, not an audible note or a calibrated DC input.

Outputs: `trace.csv`, exact-clock event/transition/segment audits (including the
final partial phase query), input event and reference source snapshots, then
`manifest.json` written last. The imported envelope snapshot keeps its original
`render_amplitude_envelope_reference.py` filename beside `renderer.py`. The manifest
contains parameters, constants, inference/scope descriptions and byte hashes for
all earlier artifacts. Owned-file cleanup follows the existing exporter and does
not recursively remove a directory. Snapshot hashes identify saved bytes, not
an authenticated execution image. No WAV, normalization or realtime comparison
is supplied.

## Running the bounded checks

Use Python 3.11 or newer from the repository root:

```sh
python3 tests/test_event_aware_envelope_vca.py
```

CMake registers `cs80_event_aware_envelope_vca_tests` with a 120-second timeout
only when `CS80_BUILD_TESTS` is enabled, the platform is UNIX, and the discovered
`Python3_VERSION` is at least 3.11. Older Python versions do not register this
new test; existing test registrations are unchanged. This standalone reference
check needs no production renderer executable.

## Verification status

Implementation alone is not a passing result. Runtime tests and independent
review must be recorded separately, including ordinary off-grid events,
right-continuous output and continuous states, equal-rate convolution,
independent smooth-segment integration and shared-grid endpoint identity.
No broad, malformed-input, fuzz, native-GUI or CPU/benchmark qualification is
implied by this work.

The retained validation run used Python 3.12.14. Python 3.11 is the declared
supported baseline, not an independently exercised runtime in this checkpoint.

## Verified local result

One direct run of the twelve bounded valid-input tests passed. One ordinary CLI
example exported 81 rows; the final-endpoint event and every declared artifact
hash were verified. Core-only CMake configuration generated one test registration;
no CTest execution or build is claimed. Production sources, existing exporters
and all 20 previous contracts remained unchanged. These checks do not establish
hardware fidelity or a certified numerical error bound.

Commands, logs, source/interpreter hashes and the complete example bundle are
recorded in [the verification evidence](experiments/event-aware-envelope-vca/verification.json).
