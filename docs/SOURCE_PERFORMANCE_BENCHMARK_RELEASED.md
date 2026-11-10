# Source Performance benchmark: released-member expression

The performance benchmark previously required live MPE pressure and bend to
follow every member message during pedal sustain. That expectation predates
the deliberate released-member targeting policy: once a note is physically
released, later member messages prepare future notes rather than modifying
that released voice, even while CC64 keeps its audio sounding.

On core `215b9006c168f55aad088112b690f73f022fac6c`, the old assertion fails at
sustain callback 1: member 0 retains pressure 0.1 and bend -1.2 semitones,
while the benchmark demands the newly sent 0.1014 and -1.1952. This is a
benchmark expectation error, not evidence of a synthesis regression.

The corrected benchmark records the known sent pressure and bend immediately
before its sustain Note Offs. It requires those values throughout sustain,
while continuing to send changing member messages. Physically held notes
still must track the live values. Channel identity, pedal-held/not-held state,
voice counts, tail completion, finite output and source-compilation checks are
unchanged. No production DSP or numerical acceptance tolerance changes.

## Verification

- 48 kHz / 128 frames, performance quick suite, 256 held callbacks:
  15 cases, 36 rows, 16,800 member-state checks and 564 pedal-held callbacks.
- 44.1 kHz / 256 frames, all-selector performance suite, 64 held callbacks:
  39 cases and 100 rows completed.
- Negative control restoring the live pressure expectation during sustain:
  exit 2 at `Released MPE member expression changed under sustain`.
- Negative control expecting zero pressure for physically held notes:
  exit 2 at the existing live-member assertion.

These are benchmark-integrity checks, not a CPU improvement or real-time
qualification. Earlier archived measurements are immutable historical results,
not reruns with these released-expression expectations.
