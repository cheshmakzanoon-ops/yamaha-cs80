# Interrupted pitch-wheel and ribbon gestures

## Bounded change

Prepared against main `8ade562` on 2026-11-09. A spring pitch-wheel gesture
previously remained bent if the controllers strip was hidden or destroyed
without a mouse-up. Closing a performance deck could likewise leave a pointer-
or keyboard-owned ribbon bend active.

The owners now explicitly cancel their live control gestures while their
callbacks and processor references are still alive:

- `ControllersStrip` cancels on its own visible flag becoming false and in its
  destructor. This covers the direct strip visibility changes used by Studio.
- `PerformanceDeck` cancels its ribbon and expression-pedal gesture in its
  destructor.
- Cancellation sends neutral only for an actively owned spring wheel or
  ribbon. An idle/restored wheel does not reset incoming host pitch. Latched
  modulation and expression keep their values without a new value event.
- Trailing drag/up events cannot revive a cancelled gesture. A new mouse-down
  starts a new one. Ribbon pointer takeover also clears the prior keyboard
  ownership, so losing keyboard focus does not release the new pointer bend.

Release uses the existing processor UI mailboxes and smoothing. It is applied
when audio next processes the message, not synchronously to Engine from the UI.
No audio algorithm, saved-state format, host parameter or normal bend mapping
changes. There is no new audio-thread work or source-specific pitch arbitration:
an active UI release retains the same shared pitch-reset semantics as its
ordinary mouse-up. Protecting an idle host bend is not ownership isolation for
simultaneous host and active UI gestures.

This is not a general native pointer-capture or window-lifecycle solution.
Ancestor-window hiding/minimizing without a direct strip visibility change or
owner destruction, multiple simultaneous pointer sources, OS/DAW event
forwarding, and transitions into/out of PURE are not qualified by this change.
The tests do not claim that every host destroys an editor when it is closed.

## Evidence

Native Linux Release build with GCC 14.2 and official JUCE 8.0.11:

- The same 30-check regression on main plus tests, before the fix, failed
  **11 checks**. Failures included stuck oscillator pitch after strip hide and
  stuck ribbon ratio after owner destruction in both Legacy and Circuit modes.
- The fixed build passed **30/30** checks, including active/idle cleanup,
  idempotence, hide/show/restart, trailing events, keyboard-to-pointer takeover,
  latched controls, held-note preservation, unchanged parameters, and actual
  host MIDI pitch preservation when idle views are hidden/destroyed.
- Seven focused CTests each passed ten consecutive runs: this regression,
  voice-activity core and host tests, keyboard-pressure handoff, keyboard
  context menus, typed-value units, and Scala/tuning UI state.

The regression constructs real JUCE components, delivers component events and
processes real processor callbacks. It does **not** open a native editor peer.
The separate native-peer gate was attempted but stopped before invoking the
plugin with `xlsatoms: unable to open display ":109"`; the sandbox cannot open
the required display sockets. No full visual smoke, OS capture-loss, screen-
reader, DAW or cross-platform pass is claimed. Screenshot provenance is
unchanged.

With a configured native plugin build:

```sh
cmake --build build --target CS80Snapshots cs80_voice_activity_tests
ctest --test-dir build -R '^cs80_transient_control_tests$' --output-on-failure
ctest --test-dir build -R '^(cs80_transient_control_tests|cs80_voice_activity_tests|cs80_voice_activity_host_tests|cs80_keyboard_handoff_host_tests|cs80_context_menu_keyboard_tests|cs80_typed_value_unit_tests|cs80_keyboard_tuning_ui_tests)$' --repeat until-fail:10 --output-on-failure
```

Direct entry: `CS80Snapshots --transient-control-tests`. The same regression is
also included in the full `cs80_ui_smoke` suite; that suite still requires a
working native display and remains a separate gate.
