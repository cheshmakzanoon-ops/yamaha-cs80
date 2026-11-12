# Event-aware envelope/VCA with an explicit sine

`scripts/render_event_aware_envelope_sine_vca.py` is a separate Python 3.11+
standard-library reference. It extends the bounded composition experiment in
[EVENT_AWARE_ENVELOPE_VCA.md](EVENT_AWARE_ENVELOPE_VCA.md) to **one explicit sine
input**, preserving that existing constant-input exporter byte-for-byte.
Production sources, old exporters and contracts are unchanged.

## Scope and evidence

Input amplitude, frequency and initial phase are explicit constants, as are LC
and EI. LC and initial LI are nonnegative, so no gain-floor crossing occurs.
Envelope events use the existing analytic amplitude-envelope laws. There is no
arbitrary audio input, live-control automation, filter, production oscillator,
whole voice, WAV or listening claim. The bounded frequency range is not an
alias/antialias qualification.

LI branch/loading, coupling, offset/feedthrough and unit assumptions retain the
evidence distinctions in [IG00151_VCA_MODEL.md](IG00151_VCA_MODEL.md); envelope
laws retain [ENVELOPE_RC_MODEL.md](ENVELOPE_RC_MODEL.md). This is an independent
conditional model, not calibrated hardware truth or production equivalence.

## Analytic integration

Within a smooth envelope phase:

```
a = 1/tauLI; q = 1/tauAC; omega = 2*pi*input_frequency
LI(s) = LC + (LI0-LC)*exp(-a*s)
E(s) = target + (E0-target)*exp(-b*s)
input(s) = A*sin(phase_at_anchor + omega*s)
raw(s) = 2**EI * (input(s)+offset+feedthrough) * LI(s) * E(s)
dC/ds = q*(raw-C); output = raw-C
```

Offsets are the same float32 constants promoted to double as the constant
reference. `LI*E` has four exponential terms `c_r*exp(-r*s)`. DC offset forcing
uses the existing real convolution; sine forcing uses the imaginary part of
`exp(i*phase_at_anchor)*H`:

```
d = q-r+i*omega
H = exp((-r+i*omega)*h) * (-expm1(-d*h))/d  when q>=r
H = exp(-q*h) * expm1(d*h)/d               when q<r
H = h*exp(-q*h)                           when d==0
```

The complex `expm1(x+iy)` evaluates its real part as
`expm1(x)*cos(y)-2*sin(y/2)**2` and imaginary part as `exp(x)*sin(y)`.
All real exponential arguments in these kernel branches are nonpositive.
Equal real decay and coupling rates remain valid with nonzero sine frequency.

When `max(a,b,q,omega)*h <= .5`, the LI, envelope and offset-plus-sine factors
are expanded in a **total-degree-40** local power series on `u=s/h` and
integrated with coupling-weighted moments. This avoids subtracting four nearly
equal kernels when LI/envelope initially vanish. Sine coefficients use sin/cos
of the actual anchor phase; envelope coefficients use its actual anchor value,
not the older phase origin. The weighted moments use a 40-term exponential
series. These are roundoff-aware evaluations, not rigorous error certificates.

## Clock and discontinuities

The existing event-aware composition supplies its event ordering, phase
boundaries and boundary-only LI/coupling anchors. Queries do not feed sample
states back into those anchors. Decimal frequency and phase remain exact
Fractions: `input_phase + input_frequency*t` is reduced modulo one cycle before
floating sine evaluation. Trace `input_cycles` reports the unwrapped cycle
count. CLI phase is **cycles**, while integration omega and phase arguments
are **radians**. There is no sample accumulator or time-zero advance.

Autonomous boundaries precede same-time events; events retain file order.
LI/coupling remain continuous through reset and envelope snaps; raw/output may
jump at those exact endpoints. Extra output samples cannot change a shared
endpoint state. Initial coupling is explicit and may be nonzero.

## CLI and provenance

Every option is required. Events use the existing newline-terminated
`event,time_s,action` CSV (`note_on`, `note_off`, `retrigger`, `reset`). The output
directory must be new and its parent must already exist.

```sh
python3 scripts/render_event_aware_envelope_sine_vca.py \
  --events events.csv --attack .01 --decay .1 --sustain .3 --release .02 \
  --input-amplitude .4 --input-frequency 440 --input-phase .125 \
  --lc .8 --ei -.5 --initial-li .1 --initial-coupling .02 \
  --rate 16000 --samples 1601 --out NEW_ENVELOPE_SINE_VCA
```

Bounds: amplitude 0–1, frequency 0.1–2,000 Hz, phase [0,1) cycles; LC/initial LI
0–8, EI/initial coupling -8–8. ADSR bounds match the prior composition. Rates
8,000–384,000 Hz; 2–20,001 endpoints over at most two seconds; 1,024 events,
1 MiB event input and 32 MiB output bundle.

Outputs include `trace.csv` with explicit sine and cycle count; input events;
exact-clock event, transition, segment (including final partial phase) and
envelope transition audits; `renderer.py`, both imported reference source
snapshots under their import filenames; and `manifest.json` written last.
The manifest includes parameters, equations, scope and byte/SHA-256 identities.
Snapshots identify saved bytes, not an authenticated execution image. Floating
library final bits may vary. Existing owned-file helpers clean up only this
invocation's created files without recursive deletion.

## Bounded verification

```sh
python3 tests/test_event_aware_envelope_sine.py
```

CMake registers `cs80_event_aware_envelope_sine_tests` with timeout 120 seconds
only when tests are enabled, the platform is UNIX and discovered Python is at
least 3.11. Existing registrations are unchanged; this reference needs no
production executable. Implementation alone is not a passing result: bounded
runtime evidence and independent review must be recorded separately.
No broad regression, malformed-input, fuzz, native-GUI or CPU/benchmark
qualification is implied.

## Verified local result

One direct run of all nine bounded valid-input tests passed. Coverage includes
signed constant-envelope sine/DC coupling, refined independent real-valued RK4
on smooth and explicitly event-split intervals, short phase-zero intervals,
equal/near-equal decay and coupling rates, exact shared-grid queries, retained
event levels and the zero-amplitude prior-model identity. One ordinary CLI
example exported 81 rows; its final-endpoint event and every declared artifact
hash were verified. No failed outcomes or retries occurred.

Core-only CMake configuration succeeded and generated one registration; no build
or CTest execution is claimed. Python 3.12.14 is the only tested runtime; 3.11 is
the declared baseline, not separately qualified. Existing production sources,
exporters and all 20 earlier contracts remain unchanged. These observations do
not establish hardware fidelity, arbitrary-audio behavior, alias qualification
or a certified numerical error bound.

Exact commands, logs, source/interpreter hashes and the complete example bundle
are retained in [the verification evidence](experiments/event-aware-envelope-sine-vca/verification.json).
