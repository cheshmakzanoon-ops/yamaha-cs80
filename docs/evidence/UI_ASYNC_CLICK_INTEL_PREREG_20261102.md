# Asynchronous policy-click CTest preregistration (2026-11-02)

## Source and observed failure

The unchanged host-candidate source `e3302c694bbfc591ea448a86c49520dc66b0feca`
failed only macOS Intel CTest `cs80_ui_smoke` (test 105) in native build
[41386459806](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41386459806),
job `125088274724`. The other 110 tests passed. The two diagnostic assertions were:

1. `source toggle sends one balanced host gesture and enables its stored request`
2. `readiness status refresh does not mutate APVTS or notify the host`

Earlier Intel native runs `41269271356`, `41373060591`, `41338140448`,
and `41339574562` passed. This is not proof that the failure is harmless;
its exact event ordering must be examined before changing any DSP, host state,
or test acceptance.

## Hypothesis and API evidence

`tests/UiReadinessTests.h` registers its host-gesture listener, invokes
`sourceToggle->triggerClick()`, pumps the JUCE message loop for only 60 ms,
then removes that listener. The subsequent refresh probe listens for *any*
parameter change. In the pinned JUCE 8.0.11 source, `Button::triggerClick()`
calls `postCommandMessage(clickMessageId)`; the click is asynchronous. A
command delivered after the fixed 60 ms window could miss the gesture observer
and be counted as a forbidden state mutation by the next refresh probe.

The hypothesis is specifically **test synchronization**, not a demonstrated
production UI or APVTS defect. Source and the parameter attachment remain
unchanged until independently qualified.

## Frozen acceptance before any synchronization edit

- Retain the original exact assertion: the stored source request must become
  ON, with one balanced begin/end gesture and an actual value notification.
- Register the observer *before* `triggerClick()`. Await the exact observed
  result by pumping the ordinary JUCE message loop, with a maximum two-second
  deadline (matching this fixture's other readiness waits). No injected timer
  callbacks, direct parameter edits, extra render, or additional GUI thread.
- Keep the observer alive for a further ordinary 160 ms message-loop flush so
  a duplicate or unbalanced callback cannot be relabelled as a pass.
- Preserve `RefreshProbe::unchanged()` and every other original UI assertion.
  Do not turn failure into a warning or rerun a failed test to claim success.
- Negative controls must reject an omitted gesture, an unbalanced gesture,
  and two complete gestures. The existing real switch still supplies the
  positive control; no synthetic event substitutes for it.

## Qualification boundary

The fix is acceptable only if targeted source tests and a fresh native
macOS Intel `cs80_ui_smoke` pass at the *exact candidate SHA*. One green CI
run is evidence for that source, not proof of arbitrary scheduling or real-DAW
qualification. The immutable Legacy/V1/V2 sound gates and the CPU/click
requirements are not modified by this UI harness repair.
