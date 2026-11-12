# Tone-selector names follow Undo and Redo

## Reproduced behavior

At `b323b8e8`, selecting a hardware tone, MEMORY or PANEL recorded the source
parameter gesture but assigned its descriptive patch name outside history.
Undo restored the preceding source and sound while leaving the new `Tone I:`
or `Tone II:` name. A subsequent A/B capture or Save therefore used the label
of the undone selection. Clicking an already-selected source could also
replace a custom patch name without leaving an undo step.

The normal processor witness reported **41 failures in 305 checks**. Both ranks
and all 14 source choices showed the name mismatch while their selector, sound
and balanced host-gesture checks passed. Grouped actions and ordinary user-file,
restored-session and MIDI-program identities reproduced the same consequence.
No malformed file or native-window input was needed.

## Bounded correction

Source selection now records its before/after metadata within its existing
parameter gesture, before any value callback can flush the parameter action.
The shared metadata action was already used by Randomise. Its first perform
only records the action; the original `setPresetName` call still assigns the
name and sends the original final notification. Replay restores metadata and
uses the existing deferred program-display notification.

- The source gesture keeps its name and same-message-pass grouping. An outer
  user-edit scope still groups source changes with other edits.
- Deliberate selection still assigns the existing descriptive source name.
  Reselecting the current source with a custom name is a metadata-only undo
  step. Repeating an unchanged source/name adds no step and preserves Redo.
- Before/after metadata comes from the effective current identity, including
  a pending MIDI program. Only the new name differs; the current program and
  user-file/factory/unknown provenance stay intact.
- History recording is limited to an active message-thread parameter gesture
  outside history replay. Randomise retains its existing explicit-scope gate.
- Sound parameters, DSP, host IDs/order, physical setup, PURE, preset-loading
  policy, A/B persistence and general rename/save behavior are unchanged.

## Verification

The final focused test passes **307 checks**. It adds two exact no-op/Redo
checks to the initial witness. The A/B comparison uses the existing
range-scaled physical-value tolerance, rather than exact float equality across
normalization; metadata, source choices and modern configuration remain exact.
The existing A/B and Randomise history suites separately pass **106 checks**.

Eight focused/adjacent CTests pass:

- `cs80_hardware_source_history_tests`
- `cs80_patch_history_tests`
- `cs80_ring_curve_gesture_tests`
- `cs80_mod_matrix_route_actions_tests`
- `cs80_mod_matrix_routing_ui_tests`
- `cs80_controller_reset_host_tests`
- `cs80_studio_switch_context_tests`
- `cs80_parameter_combo_context_tests`

This is a fresh Release build with GCC and the existing local JUCE 8.0.11
checkout. The new history dispatch runs only the normal A/B and Randomise
fixtures, without the broader malformed-state suites. No native display
server, DAW, full UI smoke, cross-platform or release qualification is claimed.
Build/test commands, output and hashes are in
[`evidence/hardware-source-name-history/manifest.json`](evidence/hardware-source-name-history/manifest.json).
