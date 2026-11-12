# Copy and clear complete modulation routes

The Studio modulation matrix has an **Actions...** button on each row:

- **Copy route + amount to slot N** replaces the destination's source, via,
  destination, rank target and signed amount. The source row is unchanged.
- **Clear route and amount** resets all four routing fields to their defaults
  and the amount to zero, including a latent rank target on a global destination.

A complete route has one shared signed amount and a rank target, not separate
per-rank depth parameters. These actions copy that existing model without adding
parameters or changing DSP behavior. MSEG shapes/rates, other rows, and all other
parameters remain unchanged. There is no external clipboard or move operation.

An effective action is one named Undo step across both the session routing and
host parameter. A changed amount is sent in one balanced host edit gesture.
Routing-only changes emit no amount gesture. Undo and Redo restore both domains;
consecutive actions remain separate even before the next message-loop turn.
Identical copies and already-clear rows are disabled and do not consume Redo.
Host sessions, user presets and browser REVERT use the existing sound-state policy.

## Deferred menu authority

Opening the menu captures the current published routes and current normalized
host amounts, not the potentially older widgets. A menu authorizes one explicit
replacement of one row. It is not permission to overwrite later work.

Delivery is rejected after any routing/MSEG revision, parameter notification or
host gesture, a newer route menu, APVTS session-state redirection, page disable,
or destruction. This includes change-and-restore/Undo (ABA), no-value gestures,
and restoring an identical session. All parameter notifications conservatively
invalidate the menu, even when the parameter is unrelated to that row. Reopen
Actions after such a change. An action is unavailable during PURE CS-80, a
parameter gesture or another explicit user edit; an initially unavailable menu
cannot become authorized later. The existing undo state's active-gesture count
also covers gestures opened before this page was created.

The guards prevent delayed UI callbacks overwriting intervening work. They do
not introduce a transaction across arbitrary concurrent host parameter and
ModernConfig writers. The existing per-parameter read/publication contract and
DSP block adoption remain unchanged.

## Verification scope

`CS80Snapshots --mod-matrix-route-actions-tests` /
`cs80_mod_matrix_route_actions_tests` invokes real JUCE component menu callbacks
without a native desktop peer. It covers all 240 ordered slot copies, all 16
clears, negative/zero/positive amounts, routing-only and amount-only changes,
exact host gesture sequences, whole-state preservation, Undo/Redo, no-op Redo
preservation, delayed/ABA rejection, background parameter notifications, owner
lifetime (including host listeners closing the page during notification),
pending amount automation, session and user-preset round-trips, browser REVERT, and offscreen button bounds/names.

This is not qualification of physical mouse/keyboard popup selection, native
window rendering, screen readers, actual DAWs, or Windows/macOS runtime. No
Xvfb, sockets, ptrace, installation or external service was used. Existing
unrelated release limitations remain unchanged.

Local Release validation: **2,574 focused assertions passed**, plus two optional
PNG-export assertions. Fourteen selected CTests passed: route actions, routing
integrity, core matrix, core MSEG, MSEG text, point values, shape copy, editor
lifecycle, modern sliders, modern recall, preset REVERT, preset-file recall,
Undo/replay gestures and snapshot CLI. The final no-work build check confirmed
that the tests used the current sources in their fresh build directory.

Independent review repeated all 2,574 focused assertions and all fourteen
selected CTests against the exact final binary, and visually inspected the
aligned Actions column. Review found a reentrant lifetime issue before delivery:
a host amount listener could close the page mid-action. The callback now retains
the surviving processor reference and checks page lifetime before refreshing;
separate begin/value/end listener-destruction regressions pass.

Two private fault-control binaries also proved the fixture detects its intended
failures: omitting the destination amount write failed 1,320 of 2,574 assertions;
omitting the parameter-generation delivery guard failed 12. Both exited 1.
The controls were compiled/linked from separate source and object copies; the
qualified product source and binary hashes stayed unchanged.
