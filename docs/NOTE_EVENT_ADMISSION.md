# Note-event admission before scanner side effects

Malformed note-off events previously consumed KAS scanner queue capacity. Enough
invalid releases could fill the queue and force a valid pending onset through
its overload flush before the next scan instant. An event that cannot identify
a valid note should not change another note's timing.

The engine now rejects note-offs with pitches outside 0–127 or channels outside
0–16 before they enter the scanner. Note-ons also reject channels outside this
range before voice allocation, arpeggiator admission, or scan queuing. The UI
note-on entry point returns false for the same rejected input rather than
claiming that it deferred an event.

Channel zero retains its existing direct-API behavior. Normal MIDI channels
1–16, the direct API's finite zero-velocity behavior, valid scanner overload
handling, valid UI release ordering, and poly-pressure broadcast semantics are
not changed. These guards protect direct engine callers/adapters; ordinary
MIDI wire messages already encode bounded notes and channels.

## Tests

Scanner-ordering regressions submit more than a queue's worth of invalid
releases, from both host and UI routes, around a valid pending onset. They check
that the onset remains deferred until rendering and that its valid release
still works. Invalid-channel onset tests cover both engine modes, scan enabled
and disabled, arpeggiator enabled and disabled, and host/UI entry points.

This is event-adapter correctness verification, not a new claim about measured
hardware scan timing, native-host qualification, or audio performance.

Recorded verification: the final scanner-ordering source linked against the
preceding unchanged core library produced **42 failures out of 883 checks**.
The fixed scanner target completed **883 checks with zero failures**. Core,
scanner-ordering, and tuning-MIDI CTests passed together (3/3, 18.65 seconds).
This is a targeted run, not a full-suite rerun.
