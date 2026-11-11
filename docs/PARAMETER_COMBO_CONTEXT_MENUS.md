# Studio parameter choice context actions

## Scope

The AdvancedPanel/Studio choice factory previously created plain JUCE
ComboBoxes without parameter context actions. It constructs four controls:
FX 1 type, FX 2 type, FX 3 type, and arpeggiator mode.

`ParameterComboBox` adds right-click and exact Shift+F10 context access for
these four choices. The factory retains its existing ComboBoxAttachment
ownership, item lists, layout, titles and ordinary choice handling. It now
enables keyboard focus deliberately so these choices can be reached and
operated from the keyboard.

Each context binds the actual APVTS ID and obtains its host parameter index
from that parameter. The menu offers reset to that parameter's representable
default and MIDI learn, cancel and forget. Discrete choices do not get a
continuous typed-value editor or an unrelated generic help card. Reset is a
single balanced host gesture and undo step; resetting an already-default
choice is a no-op.

Tooltips retain any custom explanation and read the current parameter value,
learning state, CC assignment and PURE inactivity at request time. The
noneditable text label passes pointer/hover handling to the ComboBox so the
label's copied tooltip cannot hide the live text. Label routing is reapplied
when JUCE replaces the label after a look-and-feel/hierarchy change.

## Delayed actions and interaction ownership

- Safe component pointers, binding generations, processor/ID identity and
  actual host indices protect delayed actions after destruction, clearing
  and A-to-B-to-A rebinding. The processor must outlive the control and its
  external attachment, as in the existing editor ownership model.
- Effective disabled state, held ordinary pointer input and an active
  ordinary choice popup block context dispatch and delayed edits. Context
  presentation blocks duplicate menus and ordinary selection input until
  dismissal. A blocked mixed-button release does not strand pointer ownership.
- Learn rechecks PURE at invocation. MIDI actions capture the map generation
  before reading their identities, so a newer learn/mapping edit, including
  same-target or same-CC ABA, makes them inert. Cancel and forget also check
  their target/CC. A map change elsewhere conservatively requires reopening
  the menu. This is UI callback protection, not a new atomic MIDI-map protocol.
- Normal left-click/Return choice-popup dispatch, arrow navigation and
  ComboBoxAttachment notifications remain JUCE's implementation. The context
  actions do not replace or rebind the attachment themselves.

## Verification

The focused native gate passes **119 checks, zero failures**. It constructs
the actual editor and finds all four expected bindings, then exercises each
real attachment, arrow/selection notifications, balanced reset gestures,
undo/redo, learned MIDI isolation and PURE processing. A separate nonzero
discrete-default fixture prevents a hard-coded first-item reset from passing.
It also covers custom/live tooltips, label-area hit targets, exact shortcut
modifiers, queued ordinary popup dispatch, context dismissal/repetition,
disabled ancestors, clearing, deletion, rebinding and stale MIDI identities.

Eight registered focused/adjacent CTests pass sequentially: parameter combo
context, switch context, context-menu keyboard, typed-value units, transient
controls, modern slider gestures, undo-replay gestures and snapshot CLI
admission. An independent exact-link run also passes all eight tests and
repeats the 119-check focused gate without failures. These are focused
regressions, not a full project-suite result.

Ten private source faults compile, link and terminate normally with behavioral
failures: omitted factory binding (4), binding generation (5), PURE guard (2),
map generation (3), ordinary-popup action guard (2), delayed disabled guard
(5), reset no-op (5), hard-coded first-item default (2), right-click routing
(12), and label-hover routing (5). The missing-factory mutant completes 55
checks because the four missing bindings cannot run their per-control cases;
the other mutants complete all 119. Crashes are not counted as detection.
These results demonstrate sensitivity to those injected faults, not general
proof of defect freedom or old-binary compatibility.

The isolated build uses GCC 14 Release and official JUCE 8.0.11 at
`c352e2489000ffeb9e397051377d75eef2efe641`, reusing the unchanged JUCE/sysroot
configuration from the existing native VST3 audit. No core, processor,
UiComponents, MemoryLid, pending fader work, frozen tolerance or screenshot
baseline was changed.

This is a **headless direct-callback and component-routing qualification**.
The test replaces popup presentation, pumps JUCE's queued ordinary popup
callback, and invokes context actions directly. It does not open an OS popup,
create a native editor peer, exercise physical keyboard/mouse capture, verify
screen-reader operation, or qualify a DAW's forwarding behavior. No full UI
smoke, pluginval, sanitizer, remote CI or screenshot-refresh result is claimed.

[Raw logs, fault summary and content hashes](evidence/parameter-combo-context-2026-11-11/manifest.json).

## Reproduce

```sh
cmake --build build-native --target CS80Snapshots
ctest --test-dir build-native -R '^cs80_parameter_combo_context_tests$' --output-on-failure

# GCC/Clang + Ninja: private mutant files/objects/executables, no working edit.
python3 tests/test_parameter_combo_context_negative_controls.py \
  --build build-native --output /tmp/combo-context-negative
```
