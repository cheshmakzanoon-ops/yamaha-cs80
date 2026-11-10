# Live ARP mode and octave edits

Base: `03cb6ac`.

The stored ARP sequence was rebuilt on key changes, but not on live mode or
octave-range parameter edits. A held chord could therefore keep ascending after
selecting descending or played order, and octave changes could remain inaudible
until a key was pressed or released.

Capture the previous effective mode and octave range, then rebuild held input
only when one changes while ARP stays enabled. The current voice/gate and step
clock are preserved. The next scheduled step begins the newly constructed order.
Effective ARP enable/disable transitions keep their existing separate lifecycle;
unchanged parameters do not reset sequence position. Existing stack-bounded
sequence construction and source channel/velocity metadata are reused.

The regression exercises descending, played and up/down order, octave expansion
and reduction, in both Legacy and Circuit policies at 48 kHz. It checks the
current gate remains intact, explicit subsequent note sequences, unchanged-set
bit-exact audio, and an edit partway through a gate retaining the original
next-step deadline. No new key event is sent to make the edits take effect.
The pre-fix expanded sequence matrix reports 22,469 checks with 54 failures
(before adding four explicit mid-gate/deadline assertions).

Final fresh Release tests pass all 22,473 tuning/MIDI checks and all three targeted
CTests: core, scanner ordering, and tuning/MIDI. No full-suite, native-host,
sanitizer, hardware or realtime-cost qualification is claimed for this increment.
