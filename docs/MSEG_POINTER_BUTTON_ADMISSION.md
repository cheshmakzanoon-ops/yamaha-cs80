# MSEG pointer button admission

This follows the selection-drag and keyboard-deletion ownership fixes.

The editor previously treated right and middle buttons like left-button edits:
point drags changed levels, and double-clicks removed or inserted points. New
admission guards require a left, non-popup mouse event before changing focus,
undo transaction state, drag state, or envelope data. Normal left-button dragging
and double-click editing are unchanged. No active-drag guard is added to the
double-click handler because a valid left double-click may follow mouse-down.

The exact baseline fixture reported 31 checks and 6 failures. The rebuilt fix
reported 31 checks with zero failures. The added matrix covers right, middle,
and left buttons for point movement, deletion and insertion; insertion compares
its own pre-event point count so earlier deletion cannot hide an unintended edit.

Five native CTest suites passed: MSEG editor lifecycle, modern slider gestures,
Keyboard tuning UI, undo/replay gestures, and snapshot CLI admission. These are
direct JUCE component/processor callbacks, not physical mouse, OS popup, display,
external DAW, wrapper or full-suite qualification. Disabled-component lifecycle
and external session replacement are not addressed by this patch. No DSP or
serialization changes are included.
