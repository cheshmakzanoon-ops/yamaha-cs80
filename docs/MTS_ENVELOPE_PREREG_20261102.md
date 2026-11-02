# Supported MTS envelope validation — preregistration (2026-11-02)

Base: `2a8c726e4a055b7733dd8e314f334de4ac042c87`, source
`335930af02743a272b0da6ddd0544024794ea296`. Separate local parser repair;
no plugin/GUI/publication changes.

## Public protocol contract

The [original MIDI technical text](https://musescore.org/sites/musescore.org/files/2018-06/midituning.pdf)
printed1 defines SysEx data as seven-bit bytes. Printed2–5 and7–8 give the
complete supported layouts: original bulk01 is408 bytes; single02 is8+4N;
banked single07 is9+4N; octave08 is21; octave09 is33. Printed6–7 reserve bits2–6
of the first octave channel-mask byte as zero. These are complete-message
parser contracts, not a raw MIDI stream/interleaved realtime-byte decoder.

IMPORTANT: printed6 explicitly permits receivers to ignore original bulk01
checksum because historical manufacturers used ambiguous formulas. Preserve
that tolerance. Checksums for other dump IDs04/05/06 are outside this parser's
implemented message set; do not claim their support or invent a new checksum
requirement for01.

## Before production changes

The independent header-only fixture covers all eight existing variants: bulk
non-realtime, single realtime, banked single realtime/non-realtime, and both
octave widths realtime/non-realtime. For each: exact128-entry valid oracle,
broadcast/directed/wrong-device controls, every interior high-bit byte, every
framed short prefix, one through four excess data bytes, and broken framing.
Also test every reserved octave bit, zero-offset valid octave messages, fully
sized invalid128/255 counts, no-change pitch/mapping preservation, original01
checksum variants, and null input. Every rejection must leave all128 float
pitch bits and mapped flags unchanged. Existing 46 counted-message and432
realtime/preservation tests remain; no old criterion is removed or loosened.

Permitted repair: validate seven-bit payload, exact supported lengths and
reserved octave-mask bits before any table write, allocation-free. Do not add
bank/program/channel selection, new message types, tuning storage semantics,
retuning smoothing or DSP-law changes. The current global-table/per-channel
limitation remains explicit.

Register actual-header guard-deletion mutants for the seven-bit validation,
bulk length, counted length, octave length and reserved-mask guard. Each must
compile and fail its intended ordinary assertions; original/restored positives
must pass. Run focused Release and ASan/UBSan plus existing tuning regressions.
This is bounded protocol parsing evidence, not complete MTS/DAW conformance or
full final-source numerical qualification. Keep original source locks intact.

## Separate local sanitizer capability boundary

The first instrumented attempt aborted with LeakSanitizer reporting that it
cannot operate under ptrace; no test summary was recorded. Preserve that run
as unqualified, not a source-test failure or sanitizer PASS. The ordinary-only
qualifier mode may continue the original five guard mutants without claiming
sanitizer coverage. Separately rerun the IDENTICAL hashed instrumented binary
with detect_leaks=0, explicitly limited to address/undefined checks. Leak
checking and native sanitizer gates remain pending; no security/ptrace change
is proposed. Agent5 independently reviewed this diagnostic scope.
