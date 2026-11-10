# ARP mode transitions discard old scanned gestures

Base: `6034f5c`.

Enabling ARP already defines a new performance gesture: direct voices are
released, waiting direct keys and the held input set are cleared. Queued KAS
scanner events previously survived that transition. A queued direct onset could
become an unintended ARP source; a queued host release could cancel a fresh ARP
key, and toggling ARP on/off before rendering could resurrect an old direct key.

Discard the old scanner queue when the effective ARP mode changes. New events
after the parameter transition are retained. This does not change ordinary
scan-option disable behavior, which still flushes its events in order, nor does
it reset scanner phase, discard controller state, or alter ARP clock/gate laws.
PURE's masked ARP parameter changes are not effective transitions.

The regression matrix covers 44.1/48/96 kHz, Legacy/Circuit policies, scan off/on,
and host/UI keyboard origins. It checks stale onset rejection, fresh ARP onset
and release, stale-release isolation, and rapid ARP on/off before rendering.
The expanded baseline scanner-ordering suite reports 811 checks and 15 failures.
These tests cover software event boundaries, not measured hardware timing.

Fresh Release validation passes all 811 scanner-ordering checks and all four
targeted CTests: core, scanner ordering, scanner overflow, and tuning/MIDI.
No native-plugin, sanitizer, full-suite or realtime-performance claim is made
for this increment. The previous full-suite checkpoint is separate.
