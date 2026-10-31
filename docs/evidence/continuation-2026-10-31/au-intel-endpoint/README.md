# Open native Intel AU endpoint investigation

2026-10-31. Source `56b85819eea8d939d08faebb028f212e475825ed`
compiled on native macOS Intel, then
[run 41238033893, job 124645832711](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41238033893/job/124645832711)
failed 4 of 91 CTests. Cold-idle timing and smoothing failures predate their
already published repairs. The additional failures are the native AU host
fixture and its restored mutation control.

Both report **“AU publishes the current buffer endpoint, not a future ramp
endpoint.”** The same native host's maximum audio differences are
`5.82077e-11` for ramps, `2.55357e-7` for immediate events, and zero for
16-voice mode. The AU macro/platform fixtures, UI smoke and teardown pass.
The missing parameter IDs and numerical readbacks prevent the old log from
distinguishing incorrect publication from a numerical comparison mismatch.
Native arm64 success does not settle that question.

## Read-only source trace and diagnostic proposal

The AU queue clips a crossing ramp at the current render's last frame.
`HostAutomationCurve::commit()` submits that endpoint as Float32 through
`cs80CommitAuParameter()`. JUCE's `AudioParameterFloat::setValue()` stores a
physical Float32 using the parameter's `NormalisableRange`. Its
`getValue()`, and therefore AU `GetParameter`, normalizes the stored physical
float again. The fixture currently compares this readback with the independent
double curve, at the registered **1e-7** normalized limit.

This makes physical storage/inverse-normalization rounding a concrete
alternative to an endpoint-timing defect. For example, pulse width has a
physical range of 0.5 to 0.9; filter cutoffs use a nonlinear range. It is not yet
a proven explanation for the native failure. A stale or incorrectly published
endpoint and approximate-equality setter suppression remain alternatives.

The diagnostics-only change to `tests/AuAutomationHostTests.cpp` retains the
original assertion verbatim. It prints actual/expected values, absolute frame
and block/segment positions, per-lane counts and worst errors. The physical
values it prints are explicitly labelled **projections from normalized values**;
the host has no direct read of the processor's internal physical float. A
JUCE-style range round trip is included to test the numerical hypothesis.
No production source, event schedule, threshold, assertion or mutation changes.

The next native Intel run must supply numerical evidence before a corrective
contract or implementation is evaluated. No threshold relaxation is proposed.

## Evidence

- [Full original native job log](native-before-full-job.log)
- [Provenance and diagnostics plan](diagnostic-plan.json)
- [Original CI artifact 13057572684](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41238033893/artifacts/13057572684)

This record is an open software-protocol investigation, not hardware evidence
or a final Intel qualification.
