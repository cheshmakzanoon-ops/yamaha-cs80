# Atomic MIDI tuning message validation

Date: 2026-11-04. Baseline: `db24a471` (main).

## Failure and fix

`tuning::applyMtsSysEx` previously applied complete entries from a truncated
single-note tuning message even when its declared count required more entries.
The function returned success, so `Engine::handleSysEx` retuned sounding voices
from the incomplete message. The parser also accepted trailing data and status
bytes in positions reserved for seven-bit data.

The parser now checks the complete format-specific length and every interior
data byte before any table mutation. Supported formats are bulk tuning dump
(`08 01`), single-note tuning (`08 02`, `08 07`), and one-/two-byte octave tuning
(`08 08`, `08 09`). Validation is bounded, allocation-free, and does not copy the
tuning table. A malformed message leaves all pitch values and mapping flags
unchanged. No valid-message pitch arithmetic or DSP equations change.

## Regression coverage

The existing `cs80_tuning_midi_tests` target now includes:

- 2,774 malformed frames across eight real-time/non-real-time fixtures. These
  include every prematurely closed prefix, excess payload bytes, and four
  high-bit values in every interior position.
- Exact preservation of a non-default table, including unmapped keys, on every
  rejection.
- Accepted fixtures with checked decoded values, the no-change sentinel,
  all-no-change and empty updates, a 127-entry message, and the highest legal
  key/pitch value.
- An Engine-level regression with a sounding note. An incomplete two-note
  message must return false, preserve its tuning table, and produce
  bit-identical subsequent audio to an untouched control Engine. A later valid
  message must still retune that note successfully.

The final frozen tests run against the unchanged baseline fail 22 checks,
including all three Engine rejection/state/continuation assertions. With the
fix, all 131 tuning/MIDI checks pass. The existing core suite also passes all
11,508 checks. A separate GCC 14.2 build with AddressSanitizer and
UndefinedBehaviorSanitizer (`-O1 -fsanitize=address,undefined
-fno-omit-frame-pointer`) passes the same 131 checks. Leak detection was
disabled for that run; it is not a leak-check result.

## Reproduction

```sh
cmake -S . -B build-core -DCMAKE_BUILD_TYPE=Release \
  -DCS80_BUILD_PLUGIN=OFF -DCS80_BUILD_TESTS=ON
cmake --build build-core --target cs80_tuning_midi_tests cs80_tests --parallel 2
ctest --test-dir build-core \
  -R '^(cs80_tuning_midi_tests|cs80_core_tests)$' --output-on-failure
```

## Compatibility and limits

This is wire-format validation, not a claim of complete MIDI Tuning Standard
conformance. Existing program/bank selection, channel-mask behavior,
real-time/non-real-time application semantics, and bulk checksum handling are
unchanged. The updated standard expressly permits ignoring the original
`08 01` checksum because of historical ambiguity. No plugin, DAW, or hardware
qualification is claimed by these core tests.

Format references: [MIDI Association specification page](https://midi.org/midi-tuning-updated-specification)
and its [specification text, archived by MuseScore](https://musescore.org/sites/musescore.org/files/2018-06/midituning.pdf).
The latter describes seven-bit frequency data on page 1, exact count-derived
single-note message size on page 3, the bank-select extension on pages 4–5, and
the original bulk checksum exception on page 6.
