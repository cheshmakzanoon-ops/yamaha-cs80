# Studio switch context actions

## Bounded scope

The Studio original-waveform switch (`sub_original_mode`) and stereo/mono
output selector (`output_mode`) now offer right-click and exact Shift+F10
access to reset and MIDI learn/cancel/forget. Both are single-parameter
ButtonAttachment controls. Their paint methods, layout, titles and existing
attachments are unchanged.

Other segmented Studio controls were audited but are not included here.
Render-policy cells show an effective state derived from two stored requests,
with LEGACY clearing both. LFO destination chips share an enum, and clicking
the lit destination selects OFF. Those interactions deserve their own scoped
context design rather than treating each face as an independent boolean.
No UiComponents, MemoryLid, fader, processor or DSP source is changed.

`ParameterContextButton` is a separate button base. It follows the reviewed
ParameterComboBox menu/identity design without changing that helper. The
processor must outlive the control and its external attachment, whose actual
APVTS ID must match the context binding. Reset obtains the parameter's real,
representable default, sends one balanced host gesture and creates one undo
step. Repeated reset at the default is a no-op.

## Interaction ownership

- Context presses never reach ordinary button activation. Output's selected
  half is recorded only after ordinary input is admitted, stays available to
  `clicked()` before attachment listeners run, and is cleared after release.
  A blocked opposite-half context press cannot replace a held ordinary half.
- An open context blocks duplicate menus, ordinary pointer/key input and
  rebinding until dismissal. Held ordinary pointer or JUCE button-down state
  blocks context opening and delayed actions. Mixed-button releases preserve
  the original left sequence and do not strand it.
- Original-waveform Return retains JUCE's queued activation. A pending-click
  counter keeps context, rebind and delayed edits out before delivery, and a
  safe queued callback drains ownership even if disabled before dispatch.
  JUCE's normal visual flash is released by its paint/timer cycle. Ordinary
  mouse presses are not blocked merely by that visual flash.
- Plain/unmodified output arrow/Space/Return behavior remains immediate.
  Modified Return retains JUCE's queued fallback and uses the same pending-click
  ownership tracking. Focused physically held activation keys are also
  considered busy for context admission.
- Effective disabled state includes disabled ancestors. The baseline output
  key callback returned handled=true while disabled, although JUCE already
  prevented parameter mutation. This change makes those keys unhandled; it
  does not claim a previously mutating disabled control.
- Safe component pointers and captured binding generations protect callbacks
  after destruction, clear and A-to-B-to-A rebinding. MIDI-map generations
  invalidate stale learn, cancel and forget, including same-target/CC ABA.
  Learn rechecks PURE at invocation. Cancel/forget recheck their target/CC.
  This is delayed UI callback protection, not a new atomic MIDI-map protocol.
- Tooltips retain custom explanations and show the current parameter text,
  learning target, mapped CC and PURE inactivity. Context actions do not add
  an unrelated help card or a continuous typed-value editor.

## Verification

The dedicated native gate passes **122 checks with zero failures** and
constructs both actual Studio controls with real ButtonAttachments. It checks both output halves, same-side no-ops,
external parameter updates after release, normal keyboard behavior, balanced
reset gestures, undo/redo, real learned MIDI isolation and PURE inactivity.
A presentation-only fixture covers context dismissal, held/queued activation,
clear/rebind, disabled ancestors and delayed callback lifetime. Actual editor
destruction also invalidates retained actions.

Nine focused/adjacent CTests pass sequentially: Studio switch context, Studio
combo context, existing switch context, context-menu keyboard, typed-value
units, transient controls, modern slider gestures, undo-replay gestures and
snapshot CLI admission. Independent final-source review found no blocker;
an exact-binary repeat passes all nine and the focused 122 checks. The final
binary SHA-256 is
`66a3f3d46fb51743712db2d2b05b68327e0ebcb67c5293ad7bf9a302cadb0f10`.
These are focused regressions, not a full project-suite result.

All **14** private source faults compile, link and finish normally with
behavioral failures: either missing factory binding (1 each), omitted binding
generation (4), PURE guard (1), MIDI-map generation (3), delayed activation
guard (8), delayed disabled guard (5), pending keyboard guard (7), reset no-op
(2), hard-coded OFF default (11), right-click routing (9), disabled-key admission
(23), bypassed output queued-key fallback (2), and uncleared output pressed half
(6). The missing-factory variants run 90 and 83 checks because the unbound
control's cases are skipped after the failing binding assertion; all others
run 122. Crashes are not counted as detection. This demonstrates sensitivity
to those faults, not general proof of defect freedom.

[Raw logs, baseline probe, fault summary and content hashes](evidence/studio-switch-context-2026-11-11/manifest.json).

This is headless direct-callback, queued-message and offscreen-component-paint
qualification. Held JUCE state is exercised directly. No OS popup, native
editor peer, physical keyboard/mouse capture, screen reader or DAW forwarding
result is claimed. Registered external Button shortcuts are outside this
qualification; these two actual controls register none. No full-suite,
screenshot refresh, pluginval, sanitizer or remote CI result is claimed.

The isolated native build uses GCC 14 Release and the unchanged official JUCE
8.0.11/sysroot configuration used by the prior Studio combo work. Private
source-fault relinks do not modify the working tree or build objects; every
accepted detection must compile, link and exit normally with behavioral
failures, not crash.

## Reproduce

```sh
cmake --build build-native --target CS80Snapshots
ctest --test-dir build-native -R '^cs80_studio_switch_context_tests$' --output-on-failure
python3 tests/test_studio_switch_context_negative_controls.py \
  --build build-native --output /tmp/studio-switch-context-negative
```
