# MTS realtime/non-realtime held notes — preregistration, 2026-11-02

The MIDI Association's [MIDI Tuning specification](https://midi.org/midi-tuning-updated-specification),
[original technical text in a public mirror](https://musescore.org/sites/musescore.org/files/2018-06/midituning.pdf),
printed pages 4-7, distinguishes realtime 7F from non-realtime 7E messages. A
non-realtime banked single-note message changes future-note tuning but must not
retune notes already sounding. The scale/octave variants carry the same
realtime/non-realtime distinction. Non-realtime bulk loading is a setup transfer.
This is public protocol evidence, not a measurement of a physical CS-80.

Before changing Engine::handleSysEx, test all eight already supported message
variants (single realtime; banked single realtime/non-realtime; one-byte and
two-byte octave realtime/non-realtime; bulk non-realtime). Run each on Legacy
and Circuit at 44.1, 48 and 96 kHz: 48 scenarios. Hold note69, submit a complete
message, then start note70. Both voices must retain their correct identity;
realtime updates reach the held note, non-realtime updates do not, and every
update reaches the subsequent note. The relative-frequency oracle is independent
of the parser and uses the existing tuning suite's 0.001-semitone tolerance.
Noise, drift, dispersion, detune, glide and initial bend are neutral only in this
new protocol fixture. No shipped preset or old test is changed.

This focused repair must leave MTS-ESP/setMtsTuning's live retuning unchanged,
leave pure-CS80 tuning isolation and all old state formats unchanged, and not
pretend that per-channel tables, bank selection or complete MTS conformance have
been implemented. All counted-payload and KBM tests remain required.

Base: b87e457 (previous separate parser repair). Native and full numerical
qualification must subsequently use the actual composed source. Original
source-locked audit registrations remain immutable. A successful focused test is
not a claim of realtime scheduling, audible transition or all-DAW acceptance.

## Supplementary preservation controls, registered before their first run

The original 288-check protocol fixture produced 24 non-realtime held-note
failures before the fix, and then passed with the fix in Release and ASan/UBSan.
Before freezing the evidence, add 144 explicit preservation checks across the
same two engines and three rates: direct live `setMtsTuning` versus realtime
SysEx, each in modern and PURE CS-80 mode. Modern mode must retune both held and
future notes; pure mode must retain equal-tempered pitch while retaining the
accepted table for future mode selection. Voice ownership is unchanged. These
are supplementary positive controls, not retroactively preregistered RED cases;
the original 288 checks, tolerance and production fix remain unchanged.
