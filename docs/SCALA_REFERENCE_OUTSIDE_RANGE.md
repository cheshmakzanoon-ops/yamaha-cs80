# Preserve the Scala reference mapping outside the playable range

A KBM reference key can lie outside the range of keys the instrument admits. The
reference still identifies a mapped scale degree and frequency. Previously,
`buildTable` used the playable-note admission helper to resolve that reference.
When first/last bounds excluded it, the code incorrectly substituted the local
linear fallback and shifted every remaining playable note.

For the existing seven-note just-major/white-key fixture, A4 is the 440 Hz
reference. Restricting playback to MIDI 60–64 must leave the mapped C at 264 Hz.
The old reference fallback instead targeted 176 Hz. Narrowing the playable range
must not redefine the tuning anchor.

The fix separates map-degree calculation from playable-range admission. Reference
calculation ignores first/last bounds but still honors explicit mapping holes;
ordinary note admission retains those bounds. The existing local linear fallback
for an `x` reference is preserved. This fallback is not a Scala conformance claim:
Scala flags an unmapped reference as an error. Parser acceptance, missing-map
behavior, unmapped-note fallback pitches and session format are unchanged.

## Basis and scope

- [Scala keyboard mapping help](https://www.huygens-fokker.org/scala/help.htm#mappings)
  defines first/last retuning limits separately from reference note/frequency.
- [Surge tuning-library reference mapping](https://github.com/surge-synthesizer/tuning-library/blob/main/include/TuningsImpl.h#L661-L688)
  resolves the tuning reference through the mapping independently of playable
  first/last limits. It is supporting implementation evidence, not a new dependency.

This change preserves this instrument's existing treatment of out-of-range keys
as unmapped. It does not broaden its complete Scala/KBM compatibility claims.
Existing sessions that relied on the incorrect out-of-range anchor will sound
correctly retuned when loaded; this is an intentional correctness change.

## Regression evidence

The initial core regression reproduced **24,955 checks / 18 failures**. With the
fix, that fixture passed. The final expanded contract varies reference notes
45/69/93, ranges above/below/including the reference, explicit map holes and size-0
linear mappings. It requires exact unchanged pitches for every still-playable
note and unchanged range/mapping admission.

A real-processor regression covers Legacy/Circuit and 8/16-voice configurations,
with three simultaneous notes (not active-extension saturation),
restricted-mapping session roundtrip, identical audible output for admitted notes,
and rejection of the out-of-range reference as a played note. These tests do not
claim external-DAW, live-MIDI-device or hardware-fidelity qualification. The fix is
in off-audio table construction, not a CPU-budget optimization.

The final Release/Linux focused run passed **15/15 CTests in 65.06 seconds**:
tuning/MIDI, the Scala session fixture, MTS fixtures, six actual-module VST3
rate/mode cases and callback-allocation checks. Expanded tuning/MIDI reported
**31,962 checks / 0 failures**; Scala session admission reported **47/0**.
All selected targets were rebuilt before this run. This is a focused integration
checkpoint, not a full-project or cross-platform qualification.
