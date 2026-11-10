# Advanced modulation and FX arrow gestures

The balanced keyboard-only slider adapter introduced for the Keyboard page is
now shared with the advanced MSEG, LFO, modulation-matrix, keyboard-response
curve and modern FX pages. Their 36 attached sliders previously used JUCE's
plain arrow handler, which changed parameters without begin/end host gestures
and allowed disabled or active-edit mutation through that handler.

The adapter is moved unchanged into `ParameterSlider.h` and reused by both the
Keyboard page and these pages. Ordinary JUCE constructors, pointer/text-box
behavior, styles, parameter attachments and ranges remain. Modified-key behavior
is unchanged. No main-panel relative-drag behavior is introduced and no DSP or
state format changes are made.

## Verification

A new native direct-component suite enumerates all 36 attached sliders across
five page families. It listens to actual processor parameters and verifies an
ordered begin/value/end sequence addressing one parameter, disabled rejection,
active-edit rejection, and balanced completion. To exercise each attachment,
the fixture explicitly enables controls that a selected FX model might disable;
this does not alter production enablement logic.

Baseline: 145 checks, 144 failures. Fixed: 145 checks, zero failures. The failure
count includes multiple assertions for each control, not 144 separate defects.
A freshly linked Release native invocation passed six registered CTests, zero
failures, 2.32 seconds: modern slider gestures, Keyboard tuning UI, context-menu
keyboard, typed-value units, undo/replay gestures and snapshot CLI admission.

This is direct component/processor verification, not native focus, physical key
repeat or external DAW automation-recording qualification. No rendered-UI pass
or resolution of the display-environment blocker is claimed.
