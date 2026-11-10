# Empty MTS channel masks are no-ops

Base: `8d97c92`.

Scale/octave tuning messages (08 and 09) with three zero channel-mask bytes
previously rewrote the global tuning table. Real-time variants also retuned
sounding voices; non-real-time variants unexpectedly changed future notes.

After complete wire validation, an empty mask now returns the parser's existing
false/no-applied-change result without altering stored pitch or mapping flags.
Other message families and nonempty-mask behavior are unchanged.

The archived MMA [MIDI Tuning Messages specification, pages 6–8](https://musescore.org/sites/musescore.org/files/2018-06/midituning.pdf)
defines the three bytes as a bitmap with one bit per recipient channel and five
reserved bits. An empty recipient set therefore has no channel to update. This
bounded correction does not implement separate per-channel/program/bank tables;
the existing global-table limitation for partial nonempty masks remains.

Regression covers both formats, real-time/non-real-time headers, complete stored
tables including unmapped flags, Legacy/Circuit held-note audio and subsequent
note audio. Positive all-channel messages must still update the table. The
baseline reports 22,593 checks with 28 failures. Fresh Release validation passes
all four targeted CTests: core, tuning/MIDI, tuning frequency-domain and tuning
robustness. No full-suite, native-host, sanitizer or realtime-cost claim is made.
