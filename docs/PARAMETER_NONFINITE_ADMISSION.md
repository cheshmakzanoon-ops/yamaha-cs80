# Reject nonfinite physical parameter commands

The public `Cs80AudioProcessor::setParamValue` convenience API previously passed
NaN and positive/negative infinity through parameter normalization and host
notification. Native regression reproduced changed parameter state and emitted
value notifications for these malformed commands. This is not an intentional
sentinel API.

An early finite-value check now rejects them before normalization or notification,
leaving the previous parameter value intact. Valid parameter indices, finite
values, ordinary host notifications and finite out-of-range clamping retain their
existing behavior. This guard covers this physical-value API, not every host
parameter or serialized-state entry point.

## Regression result

The exact native fixture covers volume, discrete tuning mode, pitch-bend range and
HPF cutoff, including all three nonfinite values, listener notifications, valid
updates and both finite out-of-range endpoints. Before the guard it reported
**40 checks / 24 failures**; with the guard it reports **40 checks / zero failures**.

After rebuilding the processor and actual VST3 module, eight registered native
tests passed **8/8 in 47.04 seconds**: the new admission test, deterministic
MTS-ESP processor fixture, and six real-module automation/SysEx rate-mode tests.
Existing tolerances were unchanged. This is focused Linux Release verification,
not a full native suite, GUI/DAW/live-service, sanitizer or performance qualification.
