# Studio LFO destination context actions

## Scope and behavior

All **18** destination chips on the Studio modulation page now reuse the
unchanged `ParameterContextButton`: six chips for each of three LFOs. Each
six-chip group binds to one real APVTS enum (`lfo1_dest`, `lfo2_dest`, or
`lfo3_dest`), not six boolean parameters. The three groups have distinct host
parameter indices. No parameter, DSP, helper, fader, UiComponents or MemoryLid
implementation changes are included.

Right-click and exact Shift+F10 open the existing reset/MIDI-learn context.
Its section header names the actual group parameter, such as `LFO 1 DEST`,
and its current value. The custom tooltip and accessible description now
explicitly say reset and MIDI learn apply to the whole LFO destination,
shared by all six chips. Reset selects the parameter's actual default
(currently OFF), regardless of which chip opened the menu. Learn assigns a
CC to that group's destination enum. Pending learn, cancel and forget are
visible and usable from any sibling chip.

The original click callback, refresh signature, painting, focus order and
layout are unchanged. A normal click selects its destination; clicking the
lit non-OFF chip selects OFF. The OFF chip retains its existing explicit
write and balanced host gesture, including a repeated OFF click. Context
opening does not select the face under the menu. Return retains JUCE's
queued click, and Space/arrows remain unhandled. External parameter changes,
reset, undo/redo and learned MIDI still refresh the group's selected light.

PURE continues to mask modern routing and learned MIDI without disabling
stored destination editing. Reset and forget remain available; MIDI learn is
unavailable, and the tooltip identifies a retained CC as inactive in PURE.
The shared helper preserves effective disabled-state checks, including
ancestors, and guards queued activation and retained callbacks. Input
ownership is **per component**. This change does not introduce or claim
simultaneous input arbitration between sibling chips. MIDI-map state and
its generation guards remain shared at the processor level.

## Verification

The focused gate passes **511 checks with zero failures**. All **10**
focused and adjacent CTests pass sequentially, independently repeated against
the same final binary. Independent final-source review found no blocker. The
unchanged helper regression retains **122 checks with zero failures**.
Exact binary/source hashes
and raw logs are recorded in the
[evidence manifest](evidence/studio-lfo-context-2026-11-11/manifest.json).
The new focused gate constructs the real editor and discovers all 18 chips
independently of their binding, checks six identities per group and distinct
groups, verifies the group menu header and custom explanations, and tests:

- Context dispatch without selecting a chip or sending host notifications.
- Normal selection, lit-click OFF, repeated OFF, queued Return and sibling
  refresh after external changes.
- Disabled/ancestor-disabled input and delayed actions on actual chips.
- Actual-default reset from every face, one balanced gesture, no-op reset,
  and one-step undo/redo.
- Pending learn and sibling cancel, a newer learn in a different group,
  stale sibling forget, and shared mapping feedback.
- Real learned MIDI affecting only its group and no other host parameter,
  PURE inactivity, preserved stored editing/reset, and cross-chip forget.
- Editor destruction invalidating retained reset actions.

The existing Studio switch gate is also rerun for the unchanged helper's
presentation ownership, clear/rebind/destruction, held/queued activation,
nonzero defaults and MIDI-map generation guards. Its original two actual
controls are now collected by their stable names, so newly supported chips
do not invalidate the original integration count. The assertions are retained.

All **four** private source faults compile, link and complete normally with
behavioral failures: omitted chip binding (**234**), wrong LFO group (**108**),
context presses reaching ordinary dispatch (**90**), and omitted lit-click
OFF (**30**). Every variant runs 511 checks. A crash is not accepted as
detection. No working source or build object is mutated. This demonstrates
sensitivity to these faults, not general proof of defect freedom.

Qualification is headless direct component callbacks, queued messages,
actual processor MIDI, and offscreen component painting. No native popup,
physical input, DAW, hardware, screenshot-baseline, sanitizer, full-suite or
remote-CI result is claimed. The isolated native build uses the retained,
unchanged official JUCE 8.0.11 tree and GCC 14 Release/sysroot configuration.

## Reproduce

```sh
cmake --build build-native --target CS80Snapshots
ctest --test-dir build-native \
  -R '^cs80_studio_(lfo|switch)_context_tests$' --output-on-failure
python3 tests/test_studio_lfo_context_negative_controls.py \
  --build build-native --output /tmp/studio-lfo-context-negative
```
