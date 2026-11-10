# MTS non-real-time tuning admission

Base: `1cffe06` (published KBM fix). Implementation: `9dbbe2b`; final regression
oracle: `1ecd8d6`.

## Defect and bounded change

The parser already recognized non-real-time extended MTS messages 07, 08 and 09,
but `Engine::handleSysEx` retargeted every sounding voice after accepting any
message. A non-real-time update therefore changed held notes instead of only
storing tuning for future notes.

After complete message validation, these three non-real-time messages now update
the table without retargeting existing voices. For accepted single-note messages
02/07, live retargeting reaches only explicitly addressed entries. This matters
when a prior non-real-time update staged other keys: an unrelated later real-time
message must not activate their stored changes. An explicit entry applies even
when its pitch equals the stored table value; the `7F 7F 7F` no-change marker does
not. Duplicate entries retain the parser's existing final-table-value behavior.
The addressed-key bitmap is fixed-size stack storage and only validated bounded
entries are visited.

## Protocol basis

The MIDI Manufacturers Association's archived *MIDI Tuning Messages* document,
[pages 5–7](https://musescore.org/sites/musescore.org/files/2018-06/midituning.pdf),
distinguishes real-time and non-real-time bank single-note and scale/octave
messages. Non-real-time versions store changes without updating sounding notes.
The same document's checksum discussion permits receivers to ignore the original
bulk message checksum because historical implementations differ. Accordingly,
this change does **not** tighten legacy bulk checksum admission.

## Preserved limitations

- Original bulk-message behavior and direct `setMtsTuning`/MTS-ESP behavior are
  unchanged. Existing admission of a non-real-time 02 envelope is not altered.
- Source-model released voices retain their existing key-voltage latch behavior.
  Even an explicit live table update does not bypass that latch. Tests compare
  real-time continuation against that existing behavior, not a new promise that
  every released source voice changes frequency immediately.
- The existing single global table still does not implement independent program,
  bank or channel-specific tables. Scale/octave tests use the all-channel mask.
- No plugin GUI/native-host validation, full MIDI conformance, allocation
  instrumentation, full-suite run or realtime performance qualification is claimed.

## Regression scope

Tests use real engines in legacy and source-performance policies. All three
extended formats and both realtime flags cover held, pedal-held and released
voices. They check stored tuning, exact continuing audio against an independent
unchanged or explicit-live-table reference, existing release-latch behavior, and
new-note pitch. Mixed-message fixtures ensure an unrelated real-time update and
no-change marker cannot activate a staged held key, while an explicit same-value
entry can.

The final expanded test against the baseline core reports **22,321 checks with
38 failures**. The fixed release build reports **22,321 checks with zero failures**.
A fresh core-only CMake build passes all four targeted CTests: core, tuning MIDI,
tuning frequency-domain, and tuning robustness. This is scoped evidence, not an
uninterrupted full-suite result.

## Resumption verification

On 2026-11-10, the recovered release executable again passed all 22,321 tuning
checks; the core executable passed all 11,508 checks. The remote change since
this patch's base was a screenshot-only refresh (`8b333b9`), with no runtime
source changes. A broad CTest invocation could not run unbuilt test executables
in this targeted build directory; it is not evidence of a full-suite pass.

The resumption's four targeted CTests all passed. A rerun of the existing
sanitizer executable could not complete LeakSanitizer shutdown: the runtime
reported `LeakSanitizer does not work under ptrace`. This rerun is therefore
not claimed as a sanitizer pass; it does not replace earlier recorded validation.
