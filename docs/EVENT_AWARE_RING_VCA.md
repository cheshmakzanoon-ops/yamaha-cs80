# Fixed-carrier RING into event-aware envelope/VCA

`scripts/render_event_aware_ring_vca.py` is a separate Python 3.11+ standard-
library composition of the declared offset ring multiplier and the independent
[event-aware envelope/VCA](EVENT_AWARE_ENVELOPE_SINE_VCA.md). Existing exporters,
production sources and contracts are unchanged.

## Bounded scope and units

Both input and carrier are explicit constant-frequency sines with explicit
initial phases. Amplitude and ring amount are fixed. This does not generate a
converter voltage, SPEED/depth/ring envelope or production smoothing. The RING
output is passed numerically **1:1 in uncalibrated engine units** to the
conditional VCA. This mapping is an explicit experiment assumption, not a
recovered circuit gain or proof of whole-voice connectivity.

The RING offsets retain the existing double constants in
`src/reference/RingModulator.h` and their [Step 2 evidence](workstreams/ws4-circuits.md).
VCA float32-promoted offsets and inferred laws retain
[IG00151_VCA_MODEL.md](IG00151_VCA_MODEL.md); amplitude envelope laws retain
[ENVELOPE_RC_MODEL.md](ENVELOPE_RC_MODEL.md). No hardware calibration, production
equivalence, whole-voice fidelity, listening or alias qualification is claimed.

## Decomposition and one-state integration

With `x=A*sin(wi*t+pi)`, `c=sin(wc*t+pc)`, amount `m` and declared offsets
`sx=.0005`, `sc=.005`, `so=.001`:

```
ring_output = (1-m)*x + m*((x+sx)*(c+sc)+so)
DC = m*(sx*sc+so)
signal sine amplitude = A*(1-m+m*sc)
carrier sine amplitude = m*sx
lower cosine = +(m*A/2)*cos((wi-wc)*t+pi-pc)
upper cosine = -(m*A/2)*cos((wi+wc)*t+pi+pc)
```

Cosines become signed-amplitude sine terms by adding 1/4 cycle. When `fi<fc`,
first negate the **cosine** phase difference, then add 1/4 cycle while keeping
the lower amplitude positive. The upper amplitude stays negative. Exact
Fraction frequency/phase clocks avoid a per-sample oscillator accumulator.

The RING DC plus downstream VCA offset/feedthrough are added **once**. The four
tones and DC feed one LI state and one coupling state:

```
raw = 2**EI * (ring_output+VCA_offset+VCA_feedthrough) * LI * envelope
dC/dt = (raw-C)/tauAC
output = raw-C
```

The inherited constant-reference boundary/event logic anchors states only at
actual envelope events or autonomous stage boundaries. Each smooth segment
expands LI×envelope into four real exponentials. DC uses the existing real
convolution; tones use the reviewed complex kernel from the sine reference.
Initial coupling contributes its homogeneous decay exactly once, not once per
tone. The instantaneous trace ring output uses the direct multiplier, not the
decomposition; tiny arithmetic-order differences are possible.

For `max(LI rate,envelope rate,coupling rate,all tone angular frequencies)*dt`
at most .5, a total-degree-40 LI×envelope×audio polynomial is integrated against
the coupling kernel. Audio coefficients include the DC once and signed tone
sin/cos derivatives at the actual anchor phase. The short expansion and
nonpositive-real complex-kernel factoring are roundoff-aware binary64 methods,
not certified error bounds.

LI/coupling remain continuous through reset and envelope snaps; raw/output can
jump at their true endpoints. Same-time events retain file order after any
autonomous boundary. Query samples never replace anchors, so extra samples
cannot alter shared-grid endpoint states. No time-zero tick is added.

## CLI

All options are required. Events use the existing newline-terminated
`event,time_s,action` CSV. Destination must be new with an existing parent.

```sh
python3 scripts/render_event_aware_ring_vca.py \
  --events events.csv --attack .01 --decay .1 --sustain .3 --release .02 \
  --input-amplitude .4 --input-frequency 130 --input-phase .125 \
  --carrier-frequency 200 --carrier-phase .25 --amount .8 \
  --lc .8 --ei -.5 --initial-li .1 --initial-coupling .02 \
  --rate 16000 --samples 1601 --out NEW_RING_VCA
```

Amplitude/amount are 0–1; phases [0,1) cycles. Input and carrier frequencies
must each be at least .1 Hz, differ by at least .1 Hz and sum to at most 2,000 Hz.
Both frequency orders are supported. Coincident and near-coincident/DC
degeneracies are **excluded**, not claimed as universally handled. Frequency
limits are scope limits, not antialias certification.

LC/initial LI are 0–8, EI/initial coupling -8–8. ADSR bounds match prior tools.
Rate 8,000–384,000 Hz; 2–20,001 endpoints spanning at most two seconds; 1,024
events, 1 MiB event input, 32 MiB output. No arbitrary audio, live controls,
negative-LI floor crossing, filter or whole-voice rendering is supplied.

Artifacts include `trace.csv` with input/carrier cycles, direct input/carrier/
ring output and envelope/VCA states; exact-clock event/transition/segment audits;
input event bytes; renderer and all imported Python reference snapshots under
their import filenames; and a final manifest with equations, explicit unit
mapping, parameters, constants, limitations and byte/SHA-256 identities.
Snapshots identify bytes, not authenticated execution; platform libm may differ.
Owned-file cleanup follows existing helpers without recursive directory deletion.

## Bounded verification

```sh
python3 tests/test_event_aware_ring_vca.py
```

CMake registers `cs80_event_aware_ring_vca_tests` with a 120-second timeout only
when tests are enabled, UNIX and discovered Python >=3.11. No production
executable is required. Implementation is not a test result: retained runtime
and independent review must qualify the exact source. Independent checks should
use the undecomposed direct multiplier in their ODE oracle, both frequency
orders/nonzero phases, zero amount, zero input amplitude, nonzero initial
coupling, reset splits and exact shared clocks. No broad, malformed-input, fuzz,
GUI, CPU/benchmark or hardware qualification is implied.

## Verified local result

One direct run of nine bounded valid-input tests passed. Independent checks use
the direct offset-ring equation rather than its tone decomposition, with both
input/carrier frequency orderings and distinct phases. Real-valued RK4 refinement
checks cover smooth/short intervals and an explicitly split reset. Additional
checks cover initial coupling and offsets counted once, amount-zero identity,
zero-amplitude feedthrough, exact shared-grid queries and event continuity.
One ordinary 81-row CLI example passed; its final-endpoint event and every
artifact hash were verified. No outcomes were retried or discarded.

Core-only CMake configuration succeeded and generated one test registration;
no build or CTest execution is claimed. Python 3.12.14 is the only tested runtime;
3.11 is a declared baseline, not separately qualified. Production sources,
existing exporters and all 20 earlier contracts remain unchanged. These bounded
observations are not hardware fidelity, arbitrary-audio or alias qualification,
and provide no certified numerical error bound.

[Verification evidence](experiments/event-aware-ring-vca/verification.json)
retains exact commands, logs, source/interpreter identities and the complete
small example bundle.
