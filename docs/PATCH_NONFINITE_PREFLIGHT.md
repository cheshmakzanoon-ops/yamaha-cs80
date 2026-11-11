# Preflight malformed physical patch recall

Single-parameter rejection did not cover `applyParameterValues` and its shared
`applyValues` implementation. That path reset hardware sources and published
parameters one by one before recognizing any malformed input. A patch containing
NaN or infinity could therefore partially replace the sound and reset source
selections.

A preflight now checks every recall-eligible value before any source reset or
parameter publication. If one is nonfinite, this operation leaves the current
parameter values and hardware sources unchanged. Physical-setup parameters and
parity policy are deliberately ignored, exactly as they already are during recall;
nonfinite data in those unused fields does not reject an otherwise valid sound.
Finite recall/clamping behavior is unchanged.

The native regression exercises NaN and both infinities in a sound parameter while
other fields request changes. It checks all current parameter values, both hardware
source selections and absence of partial volume notifications. Positive controls
verify valid recall still applies sound values and resets sources while preserving
setup/policy fields.

Exact expanded fixture before preflight: **52 checks / 9 failures**. After:
**52 checks / zero failures**. Eight rebuilt native registrations passed **8/8 in
47.21 seconds**, comprising admission, deterministic MTS processor and six actual
VST3 automation/SysEx rate-mode tests. Existing tolerances were unchanged.

This guards this parameter/source recall operation, not arbitrary metadata changes
performed by callers, host state deserialization, concurrent recall transactions or
all external input paths. It does not constitute full native/GUI/DAW/live-service,
sanitizer, performance or hardware qualification.
