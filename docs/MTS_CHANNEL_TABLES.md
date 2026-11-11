# Per-channel MTS scale/octave tuning

## Functional gap and correction

MIDI Tuning Standard scale/octave messages (08 08 and 08 09) previously applied
any nonempty channel mask to one global table and retuned every channel. The
Engine now maintains 16 channel tables and selects recipients with
`(ff << 14) | (gg << 7) | hh`: hh bit0 addresses channel1, gg bit0 channel8,
and ff bits0/1 channels15/16. Existing full-message validation occurs before any
channel changes, including reserved bits and the empty-recipient no-op.

Real-time scale messages retune targets only on addressed channels. Non-real-time
messages stage addressed tables for future notes without retuning sounding notes.
A later update to another channel does not activate that staged held-note tuning.
Direct notes, ARP-generated notes, scanner admission/projection, waiting-key
promotion and live retuning all select the note's channel table.

Global single-note/bulk messages retain their existing global addressing, applying
each note edit independently to every table. Unrelated pitches keep their channel
values. Explicit `setMtsTuning` (including MTS-ESP) remains a global replacement.
The no-argument table/pitch query defaults to channel1; explicit channels clamp
to1–16, so the existing channel0 direct-note convention uses channel1's tuning.
Reset/prepare retain the tables, as they retained the earlier global table.

## Verification

The initial six-boundary-channel regression reproduced **72 failures** against
unchanged `fa01675f` (22,881 total checks). After implementation, that exact matrix
passed. Expanded coverage now includes every single-bit mask, both message widths,
RT/NRT and Legacy/Circuit, addressed and nonaddressed held/future audio, mixed masks,
reserved bits, staged-channel isolation, global note-edit preservation, global
replacement, reset/prepare, and mapped flags through direct/scanner/ARP/waiting
routes. Final tuning suite: **23,473 checks, zero failures**.

Four local Release core CTests passed, **4/4 in32.19 seconds**: core, controller
reset, scanner ordering, and tuning/MIDI. This is not a full-suite, sanitizer,
external-MIDI-device, DAW or hardware-fidelity qualification.

The change is allocation-free at message application. In this local 64-bit build,
`sizeof(Engine)` grows from280,424 to290,024 bytes: **9,600 bytes per Engine**.
This is a memory measurement, not a throughput or whole-instrument CPU claim.

## Deliberate limits and next integration step

- Bank/program selection is still unsupported. Scale offsets retain the existing
  equal-tempered reference, not arbitrary selected tuning-preset-relative offsets.
- PURE mode still ignores tuning. Existing circuit release-node latching remains;
  updating logical pitch targets does not bypass that physical-model policy.
- Host SysEx dispatch source already calls the Engine parser at event positions,
  but this patch's tests are core-only. No new host integration pass is claimed.
- The core checkpoint left dormant host-engine synchronization untested. The
  subsequent [host follow-up](MTS_DORMANT_ENGINE_HOST.md) fixes that dispatch gap
  and records its separate real-processor transition tests and live-master limits.
- An active MTS-ESP master globally replaces engine tables each block,
  intentionally taking precedence over previously received local channel scales.
