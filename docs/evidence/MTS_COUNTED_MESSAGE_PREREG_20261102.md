# Atomic counted MTS message parsing — preregistration, 2026-11-02

The public [MIDI Association specification index](https://midi.org/midi-tuning-updated-specification)
identifies the tuning messages and bank extensions. The Association's original
technical text is also preserved in this [public PDF mirror](https://musescore.org/sites/musescore.org/files/2018-06/midituning.pdf):
printed pages 2-3 define the number of four-byte note entries and total message
length; the bank extension adds one header byte. The test does not change pitch
encoding, valid message behavior, channel routing or checksum policy.

Before implementation, reproduce the current parser's partial application of a
counted message that has valid F0/F7 framing but fewer payload bytes than its
count requires. Formats 08/02 and 08/07 each declare two entries; test every
payload prefix of zero through seven bytes. A rejection must leave every pitch
and mapping flag in a distinctive 128-note caller table unchanged. Also test
count127 with only one complete entry. Exact complete-message controls check two
changed notes, a second-entry no-change marker, untouched other notes, and a
zero-count no-op.

A complete-message parser must validate the required counted payload before its
first table write. This scope does not claim complete MTS conformance, fix other
invalid data-byte/checksum handling, introduce per-channel tables, or change the
handling of a surplus trailing payload. No numerical acceptance threshold or
original fixture is weakened.

Base is the separately reviewed KBM repair tip `70072be`; original tests and
all earlier evidence remain intact. Require red/green focused tests, sanitizer
coverage with environment limitations explicit, and unchanged original tuning/
MIDI tests before integration. Any core-header change requires later exact-source
qualification rather than silently changing the original frozen audit request.

## Observed focused qualification

Test-only `1e513e655db13a245121aa393d8a690c53feddf3` reproduced **20 failures
out of 46 checks** against unchanged parser source `2029aa3b`. For both formats,
payloads containing one complete note and zero to three bytes of the second
partially changed the table. Count127 with one complete note did the same.

Implementation `0b6301a154b120833be23c343b70fdc00d9dada4`, source tree
`7157d3a2ee6e0ec57e7413a22f203ab981ed377d`, adds only the required full-payload
length check before the loop. The count is an unsigned byte promoted to `int`,
so its multiplication by four cannot overflow the supported ABI. No allocation,
pitch conversion or extra audio-thread table copy is introduced.

GCC 14.2.0, shared lock, 2026-11-02 11:21:56–11:22:11 UTC:

- focused Release: **46/46**;
- focused ASan + UBSan: **46/46**, leak detection explicitly disabled because
  this container's ptrace restriction prevents LeakSanitizer inspection;
- preceding KBM input boundary regression: **61/61**;
- unchanged original tuning/MIDI/voice modes: **81/81**.

The focused executables were compiled with `-std=c++20 -O2 -Isrc`, or
`-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all` for the sanitizer
case. The original suite used its existing `CS80_TUNING_DATA_DIR` definition and
exact PR6 Engine/FactoryPresets objects. This is not a fresh whole-engine
sanitizer result. Native platform and composed-source qualification remain
pending. Raw logs and hashes are in `mts-counted-message-20261102/`.
