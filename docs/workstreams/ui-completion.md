# Handover completion: parameter editing and voice diagnostics

2026-10-31. Scope: the UI/host items in section 3 of
`CS80_PLAN_COMPLETION_AND_HANDOVER.pdf`. Acceptance criteria were committed before
implementation in `tests/parity/tolerances-v2/ui-completion.json` (`7332496`).

## Parameter contexts

`ParameterContext` owns the actions for a host parameter, independently of its
widget. `ParameterControl<Base>` intercepts the menu or typed-entry gesture before
JUCE's button/selector handling, so opening a menu cannot also toggle the sound.
Main-panel switches, Studio parameter switches, host-parameter combo boxes, tone
selectors, memory selectors, and the memory lid's sliders/switches expose MIDI
learn, reset, type, copy, paste and help. Menus and value-entry callbacks use weak
ownership and component safe pointers.

The binding is the exact host parameter ID. A memory miniature shares a display
formatter/help entry with its main-panel counterpart but writes the independent
memory-bank parameter. A tone selector writes the rank's existing source choice.
Choice text is taken from the displayed options; numeric units retain the existing
parameter formatter/parser. Each changed value sends one host begin/value/end
gesture. MIDI learn remains unavailable and inactive in PURE CS-80, and mappings
remain stored. Nonparameter commands such as loading a shape preset or opening a
file chooser are not exposed as MIDI-learnable host parameters. MSEG loop/trigger/
sync selectors and modulation-matrix routing edit session configuration, rather
than APVTS host parameters; their schema is unchanged and they are not claimed as
new MIDI-learn targets.

These are software editing operations; no hardware law or measurement is claimed.
The help for original controls continues to cite the existing evidence register.

## Undo/redo

The existing undo classifier is retained: host automation, MIDI-learned changes,
program loads and session restores do not enter the user's edit history.
`UndoableParameterState::ParameterChangeAction` replaces generic ValueTree property
actions for parameter edits. The action knows which parameter changes before the
APVTS sends its value callback, so replay brackets that parameter in one host
gesture. It sends no gestures for unrelated parameters. Repeated values in a drag
still coalesce, and replay is excluded from the classifier so it adds no new step.

Both session restore and user-preset restore now explicitly call `syncNow()` under
the existing tree lock before replacing the APVTS state. The two copy paths already
do so. This does not change `syncNow()`'s existing message-thread classification
contract.

## Engine taps and sixteen voices

The optional drawer now reads a coherent `VoiceMonitorFrame` published at the end
of `processBlock`. It never polls mutable `Engine` state from the message thread.
Every payload word is an atomic 32-bit value; a sequence counter validates the
whole frame. The audio writer never retries or waits, and a reader keeps its
previous display if three attempts overlap publication. Publication is off while
the drawer is closed. No telemetry is read back by the audio engine.

The drawer selects a physical voice and rank and shows the engine's oscillator
frequency, effective HP/LP cutoffs, and amplitude/filter envelope outputs. Its
curves are recent control-tap history sampled by the 15 Hz UI view; they are not a
transfer-response model or an audio waveform measurement. The former settings-only
envelope and approximate filter-response drawings have been removed. Slot reuse
is identified by the existing engine allocation age, exposed as read-only
`VoiceInfo::assignmentSerial`, so history does not connect two different notes.

The activity mask now includes physical slots 8–15 from the second instrument;
counts, lamps and accessible status cover all sixteen voices. The original mode
keeps its eight-lamp geometry. Pressure attribution uses the same bounded physical
slot protocol over both instruments. A UI strike allocated to the second engine
can apply pressure; a released token, a reused slot or a host takeover cannot.
PURE CS-80 still activates only the original eight-voice instrument.

The changes introduce no DSP law, parameter, default sound or V1 numeric change.
The sixteen-voice UI pressure correction enables the already implemented engine
pressure path for a previously unattributed UI key; it does not change host-MIDI
pressure semantics.

## Resource lifetime audit

The paint and engraved-text caches already cleared when the last plugin
LookAndFeel was destroyed. Shared panel typefaces and the wrinkle tile now clear
at that same point; the walnut-face map clears at the last editor, and the library
relief image clears at the last browser. Reference-counted images/fonts held by a
remaining component retain their own ownership. The images are regenerated from
the same deterministic recipes.

Editor, meter, drawer, Studio mount, tour-start, keyboard, browser and learn-watch
timer ownership was reviewed. TourOverlay itself has no timer; its owning Studio
mount stops its timer in its destructor. APVTS privately owns the undo-flush timer;
that JUCE lifetime path was not replaced. These changes reduce static JUCE resource
lifetimes. They are defensive cleanup, not evidence for the intermittent Linux
validator crash mechanism, which is documented separately by the integration
workstream.

## Verification entry points

The complete `CS80Snapshots --self-test` includes all existing tests and the new
parameter-context and engine-tap/ownership regressions. The additional
`--ui-completion-test` entry point runs the same completion tests plus existing
undo, ws3 host, and exact keyboard-order tests for focused mutation checks. It does not replace any CI
gate. `tests/KeyboardPressureAuditTests.cpp` retains its original eight-slot
storage bound and all 151 independent checks; the processor explicitly uses a
sixteen-slot specialization.


## Verification results (2026-10-31)

Local validation uses the Linux Release preset with official JUCE 8.0.11 and the
private Xvfb/Openbox harness. This workstream did not modify the dependency pin;
the integration workstream validates the later upstream Linux host fix. All
builds and tests here were constrained to CPUs 6–7, with two compiler jobs.

- `CS80Snapshots --ui-completion-test`: **0 failures**, both before mutations and
  after all mutated source files were restored.
- Original `KeyboardPressureAuditTests.cpp`: **151 checks, 0 failures**. Its
  original eight-slot capacity and storage assertions remain in force.
- Existing `CS80Snapshots --teardown-probe`: **exit 0**, through editor and
  processor destruction, JUCE shutdown and process static destruction.
- Final `CS80Snapshots --out build/ui-completion-smoke --self-test`:
  **UI SMOKE TEST PASSED, exit 0**. Every existing and new test ran. The main-panel
  golden images at 1600×1000 and 1120×700 both produced **0.0000%** changed pixels
  and a **0.00** worst block mean; the original tolerances stayed unchanged.
- Minimum-size target audit: **314 modern-layer controls, 0 undersized**, plus
  **0 other panel controls undersized** and the original **26 documented
  exceptions**. Keyboard order and contrast gates passed.
- The earlier full run found the old three-control monitor order; the new fourth
  control required an additional exact focus stop, not a relaxed comparison.
  `1c8aaa8` registered that order before its new test was evaluated. The final
  order assertion checks every named control, including channel II explicitly.
- `49504f1` registered the selector refresh requirement before its regression
  was evaluated. The fix updates choices and accessible text when opening or
  choosing a voice/rank, even if no new audio block is rendered.

### Independent mutation checks

Each temporary mutation was built and evaluated alone, returned exit 1, and was
restored before the next mutation. The final restored build returned exit 0.
No mutant was committed, and no gate was skipped or weakened.

| Behavior removed | Temporary change | Expected failures observed |
| --- | --- | ---: |
| Correct parameter binding | All contexts look up `P_volume` instead of their own host ID | 7 widget-family checks |
| Undo/redo gestures | Disable the replay gesture branch in `ParameterChangeAction` | 3 replay-gesture checks |
| Engine-tap data | Replace published rank taps with empty values | 6 exact publication/display checks across Legacy, Circuit and PURE |
| Second-instrument meters | Omit extension bits and active count | 2 count/meter checks |
| Second-instrument pressure | Omit extension slots from the ownership snapshot | 2 fresh-token/pressure checks |
| Voice-selector keyboard focus | Keep the selector hidden from the focus traversal | 1 exact four-control order check |
| Voice-selector capacity refresh | Disable rebuilding choices for a changed capacity | 2 exact sixteen-choice/selection checks |

The harness occasionally returned its documented environmental Openbox exit 70
before the executable started; the exact same command was rerun. Such attempts
were not counted as either a passing regression or a detected mutation.

The full capture set includes
`studio-engine-taps-sixteen-voices-1600x1000.png`. Its history comes from actual
engine snapshots spaced by 3200 rendered frames at 48 kHz, matching the 15 Hz
view. Neither the screenshot nor this diagnostic is hardware measurement evidence.
The integration workstream owns the legacy default/tail and V1 numeric byte gates,
full core/plugin CTest suites, current JUCE host validation and remote CI.


Reproduction commands after configuring the Linux preset with JUCE available:

```sh
cmake --build build --target CS80Snapshots -j2
scripts/run-ui-smoke-linux.sh build/CS80Snapshots_artefacts/Release/CS80Snapshots --ui-completion-test
scripts/run-ui-smoke-linux.sh build/CS80Snapshots_artefacts/Release/CS80Snapshots --out build/ui-completion-smoke --self-test
scripts/run-ui-smoke-linux.sh build/CS80Snapshots_artefacts/Release/CS80Snapshots --teardown-probe
```

The existing CTest `cs80_ui_smoke` invokes that same full `--self-test`; the focused
entry point is only a faster developer/mutation check. The local final log is
`build/ui-completion-full-final.log`; the screenshot set is
`build/ui-completion-smoke/`.
