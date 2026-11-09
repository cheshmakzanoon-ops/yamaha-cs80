# Context actions for parameter-backed illuminated switches

## Scope and product goal

The roadmap calls for accessible MIDI learn and context actions on controls
other than rotary knobs/faders. At `e82466f`, parameter-backed `LedButton`
switches had ordinary pointer/keyboard activation but no equivalent context
menu. This UI-only increment adds it to the existing main-panel, editor and
Studio LED factories and the eight memory SAW/PULSE switches.

Right-click, or Shift+F10 while a switch is focused, offers:

- MIDI learn, cancellation of this switch's pending learn, and forgetting its
  current mapping;
- reset to the bound host parameter's default;
- the existing control-help card for core parameters, also available with F1.

The tooltip retains its custom explanation and ON/OFF state, adds the menu
shortcut, and reads the current learned CC/waiting state. The eight memory
switches offer learn/reset using their own APVTS parameter IDs and actual host
indices. They do not silently map to the similarly named channel-row
parameters. Memory switches do not get a misleading channel-row help card.

**Remaining work:** custom Studio switches, policy/segmented selectors, combo
boxes and memory miniature faders are not covered by this increment. It does
not complete context actions for every editor control. This is software
interaction work, not new memory-panel electrical modeling or a hardware
fidelity claim.

## Interaction and lifecycle contract

- A bound popup down/drag/up sequence is consumed without toggling or starting
  automation. It is still consumed when a held activation or JUCE's visual
  button flash prevents opening a menu. Falling through to JUCE's ordinary
  `Button::mouseDown`/`mouseUp` would otherwise turn it into a click.
- Shift+F10 requires Shift without Ctrl/Alt/Command and an enabled, not-down
  switch. Unhandled keys retain the ordinary JUCE button path. Existing
  Space handling is not replaced; actual OS key/focus routing is a separate
  qualification. Return and ordinary pointer activation are exercised below.
- Binding the context does not replace the `ButtonAttachment`. Factories keep
  its ID and the context ID aligned. Rebinding during a down state is rejected.
- Asynchronous menu actions use a `SafePointer`, the processor/parameter
  identity and a binding generation. Deleted, cleared or rebound controls
  cannot redirect old actions, including A-to-B-to-A rebinding. The processor
  must outlive its editor controls, as for the existing parameter attachments.
- Inherited disabled state and an in-progress activation are checked again
  when an action executes. A previously opened learn action also checks PURE
  at action time. Existing mappings remain stored and inactive in PURE through
  the unchanged processor path. Reset/help are not blanket-disabled in PURE:
  ordinary enabled physical controls retain their ordinary editing behavior.
- At action time, cancel rechecks that this target is still pending, and
  forget rechecks that the observed CC is still mapped to this parameter.
  These reject sequentially stale menu actions. They are checks of the
  existing atomics, not atomic conditional transactions: a concurrent MIDI or
  state update can occur after a mapping/PURE check. This increment does not
  redesign `MidiLearnMap` or claim stronger cross-thread guarantees.
- Effective reset writes the actual parameter once inside one explicit host
  gesture, using its normalized default. It does not also notify the button
  attachment, which would duplicate the gesture. The attachment observes the
  parameter update normally. One undo/redo restores/reapplies the reset.
  Reset already at default is a no-op without a redundant gesture.

The changes are confined to UI construction, pointer/keyboard dispatch and
menu callbacks. No core DSP, MIDI routing, parameter IDs, state serialization,
controller reset or macOS helper implementation changes. No drawing geometry
or screenshot asset is changed. This does not claim new whole-plugin CPU,
DAW, accessibility, hardware, or cross-platform qualification.

## Verification

Frozen source/tests: `5246d11679eefe54aadbe3cabcbb289b6f209373`, based on
`e82466f3dba50d1d0227f4c8e588bce1bd2ba7de`. Local Linux x86-64, GCC 14.2.0,
Release, official JUCE 8.0.11 at
`c352e2489000ffeb9e397051377d75eef2efe641`.

- **171 checks, zero failures** in the new native `--switch-context-tests`
  mode, registered as `cs80_switch_context_tests`.
- Six focused/neighbor CTests pass: switches, existing slider context-menu
  keyboard access, typed values, keyboard tuning, transient controls and
  performance-wheel keyboard controls.
- Tests use real JUCE components, APVTS attachments, processor MIDI dispatch
  and the actual editor's control tree. They cover ordinary pointer and Return
  activation; popup down/drag/up and held-activation suppression; modifiers;
  disabled parents; unbound/momentary controls; custom tooltips; binding
  generation/clear/destruction; PURE entry after menu creation; stale
  cancel/forget; exact host gesture order and undo/redo; and isolated MIDI
  learn/reset for both channel waveforms and all eight memory waveform IDs.
- Six private source faults all compile, link and finish normally, then fail
  behaviorally: omitted memory binding **16**, omitted PURE guard **5**,
  omitted generation guard **3**, omitted cancel-target guard **1**, omitted
  forget-mapping guard **1**, and allowing a blocked popup to fall through to
  ordinary activation **1**. Each run completes all 171 checks. These are
  fault-injection results, not a claim that the unmodified old binary supports
  the new test API.
- `src/core`, processor implementation/header and all 70 screenshot-directory
  files remain byte-identical to the base. No controller-reset or macOS helper
  file was changed. The earlier core/audio compatibility evidence is not
  rerun or expanded by this UI test.

The gate constructs no native peer, opens no popup and does not invoke the
help callout. It tests menu actions directly and observes popup dispatch.
Native popup presentation, pointer capture/focus, physical Space handling,
screen readers and DAW forwarding remain separate gates. There is no new
sanitizer, full GUI smoke, pluginval, remote CI or screenshot-refresh claim.
The existing intermittent Linux validator/GUI failure remains unresolved.

[Evidence and content hashes](evidence/switch-context-2026-11-09/manifest.json)
include the raw focused, CTest and fault results.

```sh
# A normally configured JUCE native build:
cmake --build build-native --target CS80Snapshots
ctest --test-dir build-native -R '^cs80_switch_context_tests$' --output-on-failure

# GCC/Clang + Ninja only; private object relinks, without modifying working files:
python3 tests/test_switch_context_negative_controls.py \
  --build build-native --output /tmp/switch-negative
```
