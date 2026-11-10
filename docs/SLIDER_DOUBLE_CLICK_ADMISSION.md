# Slider double-click admission

`FocusSlider` now accepts double-click resets only for an enabled control with a
left-button, non-popup event. Previously JUCE's inherited handler ignored the
button, so a right-button double-click could reset a parameter while the pointer
context-menu path was in use. Caption resets already had their own button guard;
this corrects the inner slider path.

Normal left-click handling remains delegated to JUCE. In particular this change
does not reject all active gestures: normal double-click delivery can follow a
mouse-down that has already opened JUCE's drag gesture.

## Evidence

The final direct-component regression fixture checks a rejected right-button
reset, a valid direct left-button reset with one balanced notification pair, and
a left down/double-click/up sequence that resets and ends its edit. With the
new override delegating unchanged to JUCE, the final baseline reported 62 checks
and one failure (right-button admission). With the guard, 62 checks pass.

The freshly linked Release native target passed four targeted CTests in one
invocation, zero failures, 2.62 seconds: context-menu keyboard, switch context,
typed-value units and undo/replay gestures. This is direct JUCE event dispatch,
not physical multi-button interaction, rendered popup or external DAW testing.
No DSP, parser or other slider gestures are changed.

The first verification attempt linked a stale test translation unit and reported
61 checks; it did not include the final down/double-click/up assertion and is
superseded. The snapshot translation unit was explicitly rebuilt for the exact
62-check baseline, then the guarded implementation was rebuilt and relinked for
the passing run above. The final binary contains the added sequence assertion.
