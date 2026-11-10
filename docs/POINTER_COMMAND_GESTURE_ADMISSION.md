# Pointer command admission during slider edits

The `FocusSlider` pointer handler now rejects a new mouse-down while a slider
edit gesture is active, before changing its pointer position or release-swallow
state. This matches the existing keyboard context-menu policy: a menu or typed
value command cannot interrupt or nest an active edit. Fresh right-click and
Command/Ctrl-click commands retain their existing behavior.

## Regression evidence

The native direct-component fixture dispatches pointer context and value-entry
commands inside a real JUCE `ScopedDragNotification`, then repeats the commands
after that gesture has ended. It checks suppressed callbacks, the original
balanced begin/end notifications, and fresh commands without new gestures.
Before the guard, the expanded context suite reported 55 checks and 3 failures;
afterward all 55 checks pass.

The freshly linked Release `CS80Snapshots` target passed these four CTests in
one invocation (4/4, zero failures, 2.69 seconds):

- `cs80_context_menu_keyboard_tests`
- `cs80_switch_context_tests`
- `cs80_typed_value_unit_tests`
- `cs80_undo_replay_gesture_tests`

This is callback/gesture-lifecycle coverage, not an OS multi-button drag,
rendered-popup, external DAW, or plug-in wrapper qualification. No DSP behavior
or parser behavior changes. The earlier broader native checkpoint is documented
in [the integration report](NATIVE_COMPONENT_INTEGRATION_782019D.md).
