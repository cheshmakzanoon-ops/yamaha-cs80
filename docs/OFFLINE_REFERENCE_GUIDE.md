# Choosing an offline reference

These are **bounded research tools**, not production audio, a jointly solved
whole voice, hardware calibration or closure of the
[improvement roadmap](IMPROVEMENT_PLAN.md). Choose the forcing model you intend
to study before comparing numbers. Each linked page supplies the full CLI and
its evidence; this guide does not replace those contracts.

## Which tool?

| Question | Tool and run instructions | Accepted forcing |
|---|---|---|
| What happens at exact envelope events with a fixed audio level? | [Constant envelope/VCA](EVENT_AWARE_ENVELOPE_VCA.md) | Explicit constant audio in [-8,8]; fixed LC/EI, explicit initial LI/coupling. |
| What changes when that audio is one continuous sine? | [Sine envelope/VCA](EVENT_AWARE_ENVELOPE_SINE_VCA.md) | Amplitude 0–1, frequency .1–2,000 Hz, phase [0,1) cycles; otherwise the same envelope/VCA scope. |
| What does the declared fixed-carrier RING multiplier feed into that envelope/VCA? | [RING envelope/VCA](EVENT_AWARE_RING_VCA.md) | Input amplitude and ring amount 0–1; input/carrier frequencies each >=.1 Hz, sum <=2,000 Hz, difference exactly zero or >=.1 Hz; phases [0,1) cycles. |
| How does a directly specified converter-node voltage trajectory affect ring phase? | [RING time reference](RING_TIME_REFERENCE.md) | PWL voltage 0–10 V, fixed input sine with positive frequency <=2,000 Hz, amplitude/amount 0–1 and explicit phases [0,1). No envelope/VCA composition. |
| How much does sampling one fixed sine/envelope trajectory into native PWL forcing change its results? | [Endpoint-clock diagnostic](ENVELOPE_ENDPOINT_CLOCK_DIAGNOSTIC.md) | Fixed .02 s experiment at 8/16/32 kHz, not a configurable renderer; native realtime comparison disabled. |

The two RING tools are not interchangeable: direct-voltage RING integrates the
inferred exponential converter; RING envelope/VCA takes explicit fixed carrier
Hz and does **not** generate converter voltage, SPEED/depth/envelope behavior or
production smoothing. Exact coincident input/carrier frequencies are supported
in the current RING/VCA exporter by folding the lower sideband into signed DC.
Strictly nonzero frequency differences below .1 Hz remain excluded.

## Shared clocks, units and bounds

For the constant, sine and RING envelope/VCA exporters:

- Attack .001–1 s, decay/release .01–10 s, sustain 0–1; fixed LC and initial LI
  0–8, EI and initial coupling -8–8. Nonnegative LI avoids gain-floor crossings.
- Events are `note_on`, `note_off`, `retrigger`, `reset`. Exact rational clocks
  preserve file order at equal times, after autonomous envelope boundaries.
  Endpoint snapshots are right-continuous; LI/coupling remain continuous even
  when reset/snap causes raw/output to jump.
- State anchors change only at events/autonomous boundaries, not query samples.
  There is no time-zero tick. Sine phase clocks are rational before trigonometric
  evaluation; exponential/logarithmic laws still use binary64, not exact reals.

All four standalone exporters accept 8,000–384,000 Hz, 2–20,001 endpoints
spanning at most two seconds, 1 MiB input and 32 MiB output. Envelope exporters
allow at most 1,024 events. Direct-voltage RING instead requires 2–1,024 strictly
increasing knots from zero through the exact final sample time; phase anchors
change only at knots. Its voltage and phase are continuous at knots.

Input/output/LI/LC/coupling are uncalibrated engine quantities; EI is octaves,
envelope unitless, phase cycles and frequency Hz. Converter-node voltage in the
RING time reference is explicitly volts under its declared inferred law. The
RING→VCA numeric mapping is explicitly 1:1, **not** a calibrated circuit gain.
No arbitrary-audio, live-control, whole-voice or alias qualification follows.
See [VCA assumptions](IG00151_VCA_MODEL.md), [envelope assumptions](ENVELOPE_RC_MODEL.md)
and [RING evidence classification](workstreams/ws4-circuits.md).

Outputs require a new directory with an existing parent. Read each manifest and
its source/input hashes alongside the trace. Exported phase/state audits explain
which continuous model was evaluated; a hash identifies bytes, not authenticity
or proof of a source-to-binary build. Python 3.11 is the declared baseline;
these retained checks tested **3.12.14 only**.

## What has actually been checked?

Counts below describe **individual retained direct runs**, not an aggregate
current-repository suite. Configuration creating a CTest registration is not
execution of that test. The standalone exporter checkpoints configured core-only
CMake but did not build or run CTest.

| Retained checkpoint | Direct bounded result and evidence |
|---|---|
| Constant envelope/VCA | [12 tests + one 81-row example](experiments/event-aware-envelope-vca/verification.json): analytic cases, independent integration, events and shared clocks. |
| Sine envelope/VCA | [9 tests + one 81-row example](experiments/event-aware-envelope-sine-vca/verification.json): real-ODE refinement and signed sinusoidal cases; high-frequency shared-grid identity is not independent high-frequency accuracy qualification. |
| Direct-voltage RING | [9 tests + one 81-row example](experiments/ring-time-reference/verification.json): phase quadrature, shared clocks and signed finite-window projections. |
| Historical noncoincident RING/VCA | [9 tests + one 81-row example](experiments/event-aware-ring-vca/verification.json), pinned to its earlier renderer; not evidence for the later coincidence branch. |
| Current exact-coincidence RING/VCA extension | [Unchanged 9-test regression + 6 targeted tests + one 81-row example](experiments/ring-vca-coincident/verification.json): same/opposite/quadrature phases, direct undecomposed ODE checks and DC/state accounting. |
| Endpoint-clock diagnostic | [One diagnostic, three native invocations, 6 retained-bundle tests](experiments/envelope-endpoint-clock/verification.json). The existing native reference target alone was built; no CTest or production plugin run. |

The current coincidence evidence identifies renderer SHA-256
`4e43484b64fcddc3cc8e42b0bc494ddc2b834c800bd741d32e96e35b98bd0911`.
The historical noncoincident checkpoint identifies
`7409f88f1ae9289d8d24f19d53a555ac14423fb47357db902f8fb4b881ea97eb`.
Other evidence likewise pins its own inputs: snapshots, documentation or CMake
hashes need not equal today's files. Do not silently relabel an old result as a
new-source qualification. The links retain commands, logs and provenance.

The endpoint diagnostic keeps two quantities separate: same-forcing native
coarse/fine numerical refinement, and native-fine minus event-aware analytic
values. The latter combines forcing interpolation, event placement and residual
numerical/arithmetic effects, not pure interpolation error. All 161 common
endpoints per rate are included; boundary buckets are descriptive. Observed
rate trends do not establish universal monotonic convergence. Its committed
numerical subset explicitly omits the native executable and is **not a complete
replay bundle**; the manifest records the omitted binary's identity.

## Bounded questions still unanswered

These are scope gaps, not promised work or established defects:

- Can the RING/VCA reference safely admit strictly nonzero differences below
  .1 Hz while retaining stable, independently verified phase/DC behavior?
- How do endpoint-clock discrepancies behave on another explicitly scoped
  event/phase trajectory? The retained diagnostic covers one fixed trajectory,
  not arbitrary events or a universal convergence rule.
- What additional model and evidence would justify time-varying controls or
  arbitrary audio, currently excluded by the analytic compositions?

The [roadmap](IMPROVEMENT_PLAN.md) separately leaves whole-note truth rendering
and stage research open. None of these reference checks closes native-host,
hardware/DAW, production CPU or release qualification in [STATUS.md](STATUS.md).
