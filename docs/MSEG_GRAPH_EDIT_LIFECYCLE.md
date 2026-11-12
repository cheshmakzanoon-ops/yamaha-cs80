# Preserve recalled MSEG state during graph editing

## Defect and bounded fix

The graph previously kept a private shape for the entire point, curve or loop
marker drag. It ignored change notifications until release, then wrote that
whole cached shape on the next pointer event. A session recall, Undo, shape
preset, Sync/Bipolar change or another editor could therefore be overwritten
by an old gesture. A queued refresh also left double-click and Delete acting
on the previous point indices.

Each graph now retains the coherent modern-state revision that it displayed.
A later publication cancels an existing drag and discards its point selection
and frozen time axis. The latest shape is displayed without another write.
Mouse-down, drag, double-click and Delete/Backspace check this before using a
cached point index, even if the asynchronous change notification is still
queued. A mouse-up also refreshes a pending publication when no final drag
event occurs. Start a new gesture or select a point again to continue editing.

The graph's own commits advance its remembered revision, so ordinary repeated
drag events and their change notifications do not interrupt the gesture.
Disabling the graph cancels its gesture, including disable/re-enable before
another pointer event. Existing selection-change cancellation is retained.
JUCE delivers a double-click callback after mouse-up; that follow-on callback
also stays canceled until a fresh mouse-down, so release-time refresh cannot
revive an old click against new point indices.

Revision invalidation deliberately covers the whole modern configuration,
including other MSEG slots and matrix routes. An intervening change followed
by a return to identical values still cancels the old gesture. This is a
message-thread lifecycle guard, not an atomic compare-and-swap against a host
thread replacing state concurrently with an individual UI write. No audio
model, parameter layout, state schema or preset policy changes.

## Verification scope

The existing `CS80Snapshots --mseg-editor-lifecycle-tests` fixture exercises
real processor and graph callbacks without a desktop peer. New cases cover
all three slots and all four drag kinds, before and after change delivery:
shorter replacement topology, mode/unit changes, other-slot/routing edits,
Undo, edit-and-restore and processor session recall. They verify no stale
write, immediate display refresh, a fresh successful gesture and Undo back
to the external state. Separate cases cover input arriving before refresh,
own-change notifications, release without another drag, disable/re-enable,
and the actual mouse-up → double-click callback ordering with fresh recovery.

Against unchanged `8cfd6e8d`, the initial fixture failed 446 of 793 assertions.
The first correction passed the identical 793 assertions. Ten additional
callback-order assertions were added after source review of JUCE's dispatch.
The retained baseline binary and fixture are separate from final validation.

This does not qualify physical mouse capture, OS focus, DAW hosting, native
Windows/macOS execution or the full GUI smoke suite. No Xvfb/socket setup is
part of these focused checks.

Final local Release validation passed all 803 lifecycle assertions and all 16
selected CTests: core MSEG, transport/retrigger rates, mono routing, combined
interactions, core matrix, MSEG text serialization, modern recall, Undo/replay,
complete route actions, field-scoped routing, point values, shape copy,
editor lifecycle, modern slider gestures and snapshot CLI admission.
The adjacent component fixtures also passed 1,201 point-value assertions,
215 shape-copy assertions, 2,574 route-action assertions and 481 routing
assertions. This is a fresh Linux Release build, not sanitizer qualification.

Independent review approved the bounded change, verified the exact final
binary, repeated all 803 lifecycle assertions and nine adjacent callback
suites (all exit 0), and found no source blocker. It did not repeat the full
16-suite selection or qualify physical GUI or concurrent host-state writes.
