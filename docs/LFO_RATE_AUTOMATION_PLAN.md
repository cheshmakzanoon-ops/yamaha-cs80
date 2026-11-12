# Proposed bounded modern LFO rate automation

This design was implemented and qualified against verified published86 source
`9e8ccd7c262fd50f063f95a9020ca1127d071267` (tree-equivalent local depth checkpoint
`7df771581c834c3534604fc09c329bbb4e70a593`). See
[bounded completed results](experiments/lfo-rate-automation/README.md).
The planned scopes below are the reviewed test design, not broader claims.

## Scope

Append P_lfo1Rate, P_lfo2Rate, P_lfo3Rate at lanes 86, 87, 88, total 89,
with literal prior86 prefix and IDs lfo1_rate/lfo2_rate/lfo3_rate preserved.
These existing parameters span 0.05..20 Hz and already pass through Engine's
continuous smoothing bank. Only roster entries may change in production.
Do not change DSP, float sine phase law, double shape phase, UI/faders, metadata,
defaults, waveform switching, phase reset, effects or controller handling.

The modern sine accumulates float rate/sampleRate every sample, independently
of the double phase used by other shapes. Its known low-rate rounding bias is
historical compatibility behavior, not something this task fixes. Rate curves
must not be validated as though phase follows only the current rate: phase
integrates the complete preceding smoothed rate history. Use scalar processing
of the same history as the native partition reference, not an analytic ideal
frequency or a freshly reset oscillator.

## Finite ordinary-input fixture

Reuse the bounded supported native renderer without changing its source.
Pin sine waveforms and pitch/filter/amplitude destinations. Four routes are
LFO1 alone, LFO2 alone, LFO3 alone, and all three together; active depth stays
0.65 and inactive depth/destination stays zero. Initial rates are 8, 11, 14 Hz.
Convert actual Hz endpoints using the existing normalized(parameter, Hz)
helper; never confuse normalized values with Hz. Hold initial values through
frame1023, then use 16 Hz at1535, 4 Hz at2047, 18 Hz at3071, 7 Hz at4095.
All endpoints remain inside the documented range, and rates remain positive.

Use 4096 stereo frames, source pairs13/13 and0/13, capacities8/16, and
44.1/48/96 kHz realtime/offline: 96 full cases. For each active route require:

- finite output, scalar block1 versus blocks64/127/256 within unchanged audio tolerance;
- exact1024-frame initial prefix against matched constant-rate history;
- positive held-window audio effect in frames[1024,3072);
- destination-off automation versus constant-rate history byte-identical;
- zero-depth automation versus constant-rate history byte-identical;
- empty and null queue renders byte-identical;
- Legacy automation matches final-point-at-block-start flattening exactly;
- PURE automation is byte-inert versus constant-rate history.

Cross-build frozen86 compatibility must compare actual PCM bytes for noqueue,
representative preceding86 automation (including existing depth), Legacy and
PURE captures. Four captures/case on each side yield384actual comparison pairs,
not768comparisons. Add a separately reviewed ordinary-input phase-continuity
unit assertion if source review shows the native history checks are insufficient.
Do not assert an ideal phase/frequency law beyond the existing float semantics.

## Qualification and exclusions

Freeze published86 binary/source identities before implementation; independently
review design, exact sources and finite runner before build/runtime. Record
candidate and baseline binary hashes before and after all native runs this time.
Preserve failures/interrupted evidence, all existing contracts/evidence and exact
roster prefix. Historical guard edits must retain literal prefix checks and be
reported source-reviewed-only unless separately run.

No broad host/scanner suites, malformed-input cases, benchmark replay, GUI,
sanitizer workaround or security reproductions. No random-wave, waveform-switch,
all-destination, DAW, cross-platform, CPU target or hardware-fidelity claim.
Publish only after independent actual-file/evidence audit. STATUS86 waits for
verified publication of the depth checkpoint, not this design.

Independent source-feasibility review accepted this bounded rate-only scope.
Returning to an earlier rate does not restore phase. Zero-depth/destination-off
inertness applies only while that routing remains off: phase still advances.
The4096-frame fixtures do not establish full-cycle or all-phase coverage at all
rates; they establish only these finite histories and block partitions.
