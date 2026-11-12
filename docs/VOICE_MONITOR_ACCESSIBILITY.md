# Voice Monitor accessible status

The painted Voice Monitor now has a read-only static-text accessibility child.
Its title describes settings mode or the selected LIVE physical slot/rank,
waiting/unavailable/inactive/held/pedal/release state, MIDI identity and the same
formatted envelope/frequency values shown on screen. Unrendered ranks do not
expose stale numeric values. Pending selection changes retain the explicit
waiting state rather than relabeling an old callback's values.

The summary uses the existing message-thread snapshot and 15 Hz refresh. It
makes no additional audio request, has no paint implementation, intercepts no
pointer input and adds no keyboard focus stop. Settings mode explicitly calls
its plots illustrations rather than measurements. The static-text title is
updated for on-demand reading; no repeated accessibility announcement is sent
for changing sample values.

The focused native fixture uses normal self-generated MIDI and checks the
virtual accessibility-handler role, noninterception/focus, waiting/active/
inactive/unavailable states, correct formatted callback values, selection
identity, timer-only refresh and close/reopen. JUCE requires a window handle
for normal handler discovery; the headless fixture calls the virtual handler
factory directly, so actual OS screen-reader discovery and reading remain
unqualified. This is an accessibility data exposure, not a screen-reader
compatibility or live-announcement claim.

The separate focus/observational gate retains its contract. All four optional
settings/LIVE images at 420 and 294 px are byte-identical to the pre-change
focused captures. No DSP, saved state, keyboard traversal or visible layout is
changed.
