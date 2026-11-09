# Keyboard operation of performance wheels

Prepared against main `31a6d5b` on 2026-11-09. The custom pitch/modulation wheels
and expression pedal previously had no keyboard-focus or keyboard-edit path.
This change makes those controls operable without a pointer. It does not replace
their existing processor mailboxes with host parameter automation.

## Controls and ownership

- Tab can focus an enabled wheel. A focus ring identifies it; pointer clicks do
  not grab keyboard focus from computer-keyboard playing.
- Up/Right increase and Down/Left decrease by 2% of full travel. Shift uses a
  ten-times-finer 0.2% step. Home/End select the lower/upper limit.
- Pitch is spring-loaded. When none of the six adjustment keys remain down,
  keyboard focus leaves, or Escape cancels the active keyboard gesture, it
  returns to zero. Modulation and expression retain their latched values.
- Ctrl/Alt/Command combinations and unrelated keys are left to existing routes.
  An inherited disabled state also blocks edits. Disabling a control cancels
  its active input; an idle/restored value does not acquire input ownership.
- A pointer can take over from keyboard input, but keyboard input cannot
  overwrite an active pointer gesture. Stale pointer drag/up cannot modify or
  release a keyboard-owned gesture. Focus loss only releases keyboard ownership.
- Key-state notifications are not consumed, so the editor's QWERTY listener can
  also observe note releases. Existing direct strip-hide and owner-destruction
  cleanup now handles keyboard ownership as well as pointer ownership.

The controller and pedal descriptions retain their domain explanations and add
keyboard instructions. The normal pointer mapping, audio laws, host parameter
values and state format are unchanged. Keyboard actions use the same UI
mailboxes/physical expression API as pointer actions. An active pitch release
has the existing shared pitch-reset semantics, not new host-versus-UI ownership
arbitration. This is keyboard access, not a complete screen-reader value/action
interface or a claim that every custom control is accessible.

## Local evidence

GCC 14.2, Linux Release, official JUCE 8.0.11:

- The 52-check native regression on main plus tests failed **25 checks** before
  this change, including keyboard reachability and actual keyboard-to-engine
  pitch and physical-pedal updates.
- The fixed source passed **52/52** checks. It covers steps/fine steps, limits,
  repeated adjustments, modifier/disabled guards, key-up/focus/owner cleanup,
  pointer takeover, idle values, latched values, descriptions, Legacy/Circuit
  processor routing, held-note preservation and unchanged host parameters.
- Eight focused CTests each passed ten consecutive runs: wheel keyboard,
  transient lifecycle, voice-activity core/host, keyboard-pressure handoff,
  parameter keyboard menus, typed units and Scala/tuning UI state.

The tests deliver component events directly and simulate key-state notification
with no physical adjustment keys down. They do not exercise simultaneous
physical keys, real Tab focus traversal, platform auto-repeat, OS pointer
capture or screen-reader output. Native-peer/DAW qualification remains separate;
the full local visual smoke attempt stopped before plugin execution with
`xlsatoms: unable to open display ":109"`. No new
screenshots, remote CI result or release qualification is claimed here.

With a configured JUCE plugin build:

```sh
cmake --build build --target CS80Snapshots
ctest --test-dir build -R '^(cs80_performance_wheel_keyboard_tests|cs80_transient_control_tests)$' --output-on-failure
```

Direct entry: `CS80Snapshots --performance-wheel-keyboard-tests`. It creates
real components and processor instances, but no native editor peer. The same
regression also runs in the full `cs80_ui_smoke` suite on a usable display.
