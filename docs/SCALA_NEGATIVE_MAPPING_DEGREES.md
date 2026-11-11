# Signed Scala keyboard-map degrees

Explicit negative KBM map entries now select negative scale degrees. They are
not the same as `x` (unmapped). Before this change, importing an otherwise valid
mapping with `-1` or `-2` failed with `invalid mapping entry 1`.

## Independent reference

Unmodified official Scala 1.88e Linux accepts both entries. With the nonuniform
five-note scale 100, 300, 500, 800, 1200 cents, map `[-1, 0]`, formal octave 3,
and middle/reference key 60 at 440 Hz, keys 61–63 are 554.365262, 659.255114,
and 739.988845 Hz. With `[-2, 0]`, they are 659.255114, 698.456463 and 880 Hz.
All 128 printed frequencies in each output agree with signed degree-first
mapping within the recorded decimal-output tolerance.

The [retained inputs, output and replay instructions](analysis/scala-negative-degrees/REPRODUCE.md)
contain no third-party executable or archive. This uses the same official binary
and provenance as the previous formal-octave reference. The known zero-formal-
octave discrepancy remains unchanged; this does not claim complete Scala conformance.

## Implementation and compatibility

The temporary parsed map stores an optional signed integer per entry. Only an
empty optional means `x` or an omitted entry. All signed `int` values are admitted
after wide-integer range checks; map arithmetic promotes them to `long long`.
Existing frequency-domain checks still reject tables outside the engine's safe
pitch range. Parsing and table publication remain transactional.

Nonnegative mappings keep the same arithmetic. Unmapped-note behavior, the local
unmapped-reference fallback, zero-formal-octave default, reference anchoring and
playable bounds are unchanged. Existing files with `x` stay unmapped. Session
storage remains the original SCL/KBM text, not the internal optional representation.
No audio callback or engine table layout changes. The optional storage is used
only while parsing/building a tuning table off the audio thread.

## Verification

The pre-fix core regression reported 36,619 checks / 12 failures, one for each
new signed-entry map configuration. The final matrix adds official frequency
anchors, negative entries across four formal-octave settings, all MIDI keys,
explicit `x` exclusion, and signed integer boundary/transaction tests. An older
assertion that all negative degrees are invalid was deliberately replaced, rather
than retaining the obsolete policy. Existing x sentinel assertions now use the
explicit empty representation.

The native processor fixture imports the mapping, round-trips session state and
checks its actual oscillator frequency in Legacy/Circuit and configured 8/16-
voice modes. It sounds one primary voice; this is not active-extension saturation,
GUI import automation, live MTS-ESP, real-DAW or CPU/deadline qualification.

The affected Release build completed 49 steps, then a final six-step rebuild
included the completed oracle-anchor assertions. The focused 17-suite run passed
17/17 in 63.27 seconds: tuning 39,219/0, robustness 2,821/0, frequency-domain
826/0, native Scala/session 103/0 and allocation 193/0. Selection included six
MTS adapter/processor fixtures and all six actual-module VST3 rate/mode tests.
After the run had passed its robustness test, review strengthened its saved
transaction fixture to contain both numeric -1 and nullopt; that target was
rebuilt and rerun separately, again 2,821/0. No shipping code changed during
qualification. One independent final selection passed 17/17 in 64.41 seconds;
another passed 16/17 in 64.80 seconds, exposing one program-change allocation.
Publication was held rather than discarding that failure. Its captured stack and
separate correction are recorded in [Program-display notification](PROGRAM_DISPLAY_NOTIFICATION.md).
The tuning-specific tests passed in both runs.
