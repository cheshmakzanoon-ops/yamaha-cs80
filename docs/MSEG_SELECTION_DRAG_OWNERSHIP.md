# MSEG selection owns its drag

Switching the MSEG selector previously replaced the displayed shape without
ending the editor's active point, curve or loop-marker drag. A subsequent move
from that old gesture could therefore modify the newly selected envelope, or
resume against the original envelope after switching away and back.

`setMsegIndex` now cancels the active drag, clears its point/segment index and
releases the frozen drag time-axis before selecting and reloading the envelope.
No rollback is performed: edits already committed before the selection remain.
Fresh gestures still edit the new selection normally. This change does not
claim protection against every unrelated session/state replacement during a
drag; it specifically addresses explicit MSEG selection.

## Verification

The new direct native fixture covers point, curve, loop-start and loop-end
marker gestures, each with A-to-B and A-to-B-to-A selection changes. It checks
that the subsequent old move/release changes neither stored envelope, then
verifies a fresh point drag still reaches the selected shape.

Baseline: 16 checks, eight failures. Fixed: 16 checks, zero failures. One freshly
linked Release native invocation passed five selected CTests, zero failures,
1.32 seconds: MSEG editor lifecycle, modern slider gestures, Keyboard tuning UI,
undo/replay gestures and snapshot CLI admission.

These are real component/state callbacks, not physical mouse, native focus or
external DAW tests. No DSP algorithm, shape serialization, interpolation or
preset/state format changes are made.
