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
