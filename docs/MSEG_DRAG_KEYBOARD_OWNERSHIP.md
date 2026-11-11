# MSEG deletion during pointer edits

Based on main `0de5a08e`.

## Reproduced defect

Delete or Backspace during an active point drag removed the selected point while
retaining the drag's point index. A subsequent pointer move then edited the next
point instead. The regression fixture reproduced 22 checks with 6 failures on the
unchanged implementation: both keys failed rejection, original-point continuity,
and normal deletion after release.

## Correction

The MSEG editor rejects keyboard editing while any pointer drag is active. It
preserves the current gesture and all already committed changes; it does not
cancel or roll back the drag. After mouse-up, Delete and Backspace retain their
existing selected-point deletion behavior. This also prevents topology changes
while curve and loop-marker drags own their indices.

## Verification

The rebuilt native fixture passed 22 checks with zero failures. Five registered
CTest suites passed (5/5, 1.39 seconds): MSEG editor lifecycle, modern slider
gestures, Keyboard tuning UI, undo/replay gestures, and snapshot CLI admission.
The new regression directly covers both deletion keys during a point drag,
continued editing of the original point, and deletion after release.

This is direct JUCE component/processor callback verification, not physical
keyboard/pointer, display-server, external DAW, plugin-wrapper, or full-suite
qualification. External session replacement during a drag remains a separate
ownership concern. No DSP or serialization format changes are included.
