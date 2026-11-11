# Map the formal octave in scale degrees

A nonuniform Scala scale can use a keyboard map whose formal octave degree is
not a whole scale period. Previously the instrument multiplied the pitch of that
formal degree by each mapping repetition, then added the mapped key's pitch.
That repeats a fixed pitch interval and can select the wrong scale degrees.

The corrected path first adds repetition × formal degree to the mapped degree,
then resolves that degree through the scale. Negative repetitions follow the
same floor-division convention. For whole-scale-period repetitions, the existing
arithmetic is retained exactly. Formal degree zero still uses this instrument's
existing scale-length default; map holes, reference anchoring, range admission,
transactional publication and session format are unchanged.

## Independent oracle

The unmodified official Linux command-line distribution identifies itself as
**Scala 1.88e**. It was run in scratch without installation or system changes.
Own inputs and observed output are retained under
[analysis/scala-formal-octave](analysis/scala-formal-octave/audit.json); no Scala
binary, archive or source is redistributed.

- [Official download page](https://www.huygens-fokker.org/scala/downloads.html)
- Distribution: `https://www.huygens-fokker.org/software/scala-18-pc64-linux.tar.gz`
- Archive SHA-256: `be57f66406ce0f266b9440f70238cf94cde5b426940699c9966d18bcc83c744a`
- Executable SHA-256: `3e2ac64f77b34719b441ea138453ceb0a9c38b8e6976931b3bbe66324206f51c`

The scale has degrees 100/300/500/800/1200 cents. A two-key map selects degrees
0/1, repeats by formal degree 3, and anchors MIDI 60 at 440 Hz. Scala assigns
notes 60–65 to degrees 0,1,3,4,6,7; below the root, notes 56–59 map to -6,-5,-3,-2.
All 128 printed frequencies match degree-first extension within display precision;
only three match the old fixed-interval repetition. For example MIDI 63 should
be 698.456463 Hz rather than the old 622.253967 Hz.

The retained [formal-3 output](analysis/scala-formal-octave/formal3-oracle.txt)
was generated using `LOAD`, `LOAD/MAPPING`, `SHOW SCALE`, `SHOW MAPPING 54 68`
and `SEND/FREQUENCY` after directing output with `FILE`.

The same executable's formal-zero behavior contradicts its own help: it repeats
the same two pitches despite labeling zero as the scale's highest degree. That
[separate observation](analysis/scala-formal-octave/formal0-oracle.txt) is not used
to change or qualify our zero default. This is a bounded positive-formal-degree
correction, not a claim of complete compatibility with every Scala version.

## Regression contract

The initial core fixture reproduced **35,071 checks / 1,516 failures**. It includes
concrete official frequencies, reference notes 45/60/69/93, positive and negative
mapping repetitions, zero and several positive formal degrees. The native
processor test checks the actual note-63 oscillator against the official oracle
in Legacy/Circuit and configured 8/16-voice modes, with one sounding note.
This is not active-extension saturation, external DAW or hardware qualification.
Existing affected sessions will use the corrected degrees when loaded; no sound
compatibility claim is made for sessions relying on the previous incorrect law.

The final Release/Linux run passed **15/15 focused CTests in 63.26 seconds**:
expanded tuning/MIDI **36,607/0**, native Scala/session **71/0**, MTS fixtures,
six actual-module VST3 rate/mode registrations and callback allocation **193/0**.
All affected targets were rebuilt. Explicit exact-equality checks retain prior
whole-period arithmetic. This is not a full-project, cross-platform, live-master
or external-DAW qualification, and it is not a CPU-budget improvement.
