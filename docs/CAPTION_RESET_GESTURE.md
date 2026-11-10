# Caption/readout double-click reset

The knob/fader slider already resets through a balanced host edit gesture.
Its surrounding caption/readout previously called `setValue` directly, so the
same user action outside the slider changed the parameter without matching
begin/end automation notifications. That handler also treated popup-button
double-clicks as resets.

A caption/readout reset now uses the existing `resetToDefault` path, producing
one balanced attached-parameter gesture. It accepts an ordinary left-button
double-click originating on the control itself, rejects popup events and
disabled controls, and does not interrupt an active slider edit. Slider-origin
events remain excluded so the parent does not duplicate JUCE's own reset.

The native callback tests observe actual processor parameter notifications in
order, check the resulting default value, and verify popup, disabled, active
edit, and slider-origin isolation. No default values, parameter IDs, DSP,
layout, or drawing change. Direct event tests do not qualify physical OS input
routing or real-DAW automation recording.

## Verification

The final context test matrix on the preceding implementation completed
**51 checks with four behavioral failures**. The fixed native target completed
**51 checks with zero failures**. Context-menu lifecycle, switch context, typed
values and undo replay CTests passed together (4/4, 2.63 seconds).

The retained official JUCE 8.0.11/Linux Release build was used. Nonfatal
Fontconfig cache warnings remain; these direct component/processor checks
are not screenshot, cross-platform or real-host qualification.
