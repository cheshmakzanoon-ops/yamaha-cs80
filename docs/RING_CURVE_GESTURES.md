# Ring-envelope graph gesture ownership

The Studio **Modulation > SUB / RING** graph edits ATTACK and DEPTH from its A
node, or DECAY from its D node. Previously, it opened host gestures on mouse-down
and ended them only on mouse-up. Destroying Studio during a drag left those
host gestures open in the surviving processor and blocked Undo/Redo. Hiding the
page or an ancestor, disabling the plot or an ancestor, and detaching the view
also left the old drag active. A late drag could then change a hidden or disabled
control. Repeated mouse-down could replace the owned parameter list without
ending its previous gestures; non-left buttons also started edits.

The graph now ends an owned drag exactly once on normal release, destruction,
visibility/enablement loss, and parent-hierarchy changes. During a drag it listens
to its ancestors because JUCE does not forward an ancestor's visibility callback
to its children. It removes those listeners when the drag ends. Cancellation
retains the last admitted values, closes the host gestures, and permits Undo;
it does not silently revert the sound. Late drag/up events cannot restart it,
and a new valid drag works after the view becomes available again.

Only an enabled, visible plot receiving a left-button press can acquire a node.
A second mouse-down cannot replace an active node. Parameter identities, drag
mapping, ranges, drawing, DSP, preset/session formats, and recall policies are
unchanged.

## Verification

The regression finds the real production plot in a real Studio view, sends
component events, observes its actual APVTS parameter listeners, and exercises
processor Undo/Redo. On base `2ab60f4e46a89bd6e68107a818e8be4ddf7413a9`, the test
had **66 failures in 166 checks**. The final expanded suite passed **270/270**.

Coverage includes both nodes, normal release, Studio destruction, direct and
inherited disable, tab switching, Studio/root hiding, hierarchy detachment,
late events, restored editing, invalid mouse buttons and repeated mouse-down.
It also hides or destroys Studio synchronously from each ring parameter's
begin/value/end notification and verifies balanced ownership and subsequent
Undo/Redo. Begun parameters are retained independently of the component;
cancellation defers their ends until the current begin/value dispatch returns,
and no second write occurs after cancellation.

The focused gate also runs in the full UI smoke suite.

A fresh Linux Release build of `CS80Snapshots` used GCC 14.2 and official JUCE
8.0.11. Eighteen adjacent CTests passed sequentially in 8.45 seconds, including
modern recall,
Undo/replay, Studio selectors, parameter context menus, typed values, tuning,
performance controls, MSEG and matrix workflows, and CLI admission. Independent
source review approved the final lifetime handling, verified all four source
hashes and the exact binary, and repeated **270/270** focused checks and
**18/18** adjacent tests (8.34 seconds, both exits zero). That final review did
not independently rerun the retained 166-check baseline.

Retained commands, logs and source/binary hashes are in the
[evidence manifest](evidence/ring-curve-gestures-2026-11-12/manifest.json).

```sh
cmake --build build --target CS80Snapshots
ctest --test-dir build -R '^cs80_ring_curve_gesture_tests$' --output-on-failure
# Direct entry:
build/CS80Snapshots_artefacts/Release/CS80Snapshots --ring-curve-gesture-tests
```

These are offscreen, message-thread component/processor checks with the normal
plugin ownership rule that the processor outlives its editor. They are not a
native-peer, OS pointer
capture, physical multi-touch, external DAW automation-recording, or
cross-platform runtime qualification. No new screenshot, full visual-smoke
result, remote CI result, or release qualification is claimed.
