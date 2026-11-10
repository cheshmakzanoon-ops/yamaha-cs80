# Native processor/component checkpoint: c8c3dbf

Tested source: `c8c3dbf44c2aa18ae6719340cc9fef82aefb1015`, November 10, 2026.
The current local Release `CS80Snapshots` target was checked with Ninja (no
outstanding rebuild work), using the retained official JUCE 8.0.11 source and
staged Linux dependencies. Plugin and test build options were enabled.

One serial CTest invocation ran the 13 direct processor/component suites below:

- `cs80_undo_replay_gesture_tests`
- `cs80_mpe_released_target_host_tests`
- `cs80_mpe_member_onset_host_tests`
- `cs80_sixteen_voice_release_host_tests`
- `cs80_controller_reset_host_tests`
- `cs80_voice_activity_host_tests`
- `cs80_keyboard_handoff_host_tests`
- `cs80_switch_context_tests`
- `cs80_context_menu_keyboard_tests`
- `cs80_typed_value_unit_tests`
- `cs80_keyboard_tuning_ui_tests`
- `cs80_transient_control_tests`
- `cs80_performance_wheel_keyboard_tests`

**13/13 passed, zero failures**, 15.67 seconds elapsed. No selected test was
skipped or replaced by a later retry. The context-menu suite completed 62 checks
and typed-value suite 116 checks; both had zero failures. The 950-case formatter
witness now has 950 accepted cases and digest `8d7e609da2f21897`, following the
[pan typed-value correction](PAN_TYPED_VALUE_ROUNDTRIP.md).

This complements the [full core checkpoint](CORE_INTEGRATION_D00D8BF.md) and
the recent [menu lifecycle](KNOB_CONTEXT_LIFECYCLE.md),
[inline editor](INLINE_VALUE_EDITOR_LIFECYCLE.md),
[MIX admission](MIX_PERCENTAGE_PAIR_ADMISSION.md), and
[caption reset](CAPTION_RESET_GESTURE.md) regressions.

## Limits

These are actual compiled JUCE components and processor adapters, not a full
native test run. The VST3/AU wrappers, plugin validators, display-driven UI smoke,
teardown/locale checks, screenshots, sanitizers and real DAW sessions were not
run by this invocation. Tests called “host” exercise processor fixtures, not
an external DAW. Nonfatal Fontconfig cache warnings remain; rendered font
appearance and OS focus/input behavior are not qualified. No new installer,
signing, release, hardware-fidelity or audio CPU claim follows from this result.

## Display-driven attempt (blocked)

The unchanged `cs80_ui_smoke` and `cs80_teardown_probe` gates were also attempted,
separately from the passing 13-suite invocation. Both failed before application
startup: Xvfb could not create local display sockets and `xlsatoms` could not
open the display. A minimal local Unix-socket creation probe returned EPERM;
the supported escalated test attempt did not remove that restriction. These
are failed/environment-blocked attempts, not passing or skipped UI tests.

Missing Openbox runtime dependencies were locally extracted from official Debian
packages, verified against the retained official package-index SHA256 records;
no system package, security setting, or existing desktop was changed. That
resolved dependency discovery but not socket creation. A display-capable
execution environment is still needed for the unchanged gates. No rendered UI,
clean teardown, locale, or operating-system interaction claim follows.
