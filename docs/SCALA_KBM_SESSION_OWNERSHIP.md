# Scala keyboard mappings follow the processor session

2026-11-09. Narrow port of #41,
`1d860d1f332fcff5e427cd3612a53b30d080f356`, onto staged integration
`9c217df6b207eb2d5cb465c58bc79e5ebcd27fd9`. The old branch is not merged
wholesale. Current tuning admission checks, host/state schemas and DSP formulas
are unchanged; in particular, `src/core/Tuning.h` is untouched.

## Defect and correction

The tuning page kept its own SCL/KBM text. Reopening the page lost that copy,
although the processor still had the saved scale and the KBM button could appear
enabled. An open page could also use its old copy after another session replaced
or cleared the processor state. An empty scale description was incorrectly used
as evidence that no scale was loaded.

A mapping-only import now asks the processor for its current SCL source. Source
text and publication version are captured together under the existing lock;
parsing and table construction stay outside the lock. Before any publication
mutation, the version must still match. Replacement, clearing, or another
mapping-only import therefore cannot be overwritten by an older in-flight
mapping operation. A rejected operation preserves the text, description,
complete tuning table and publication version. Full scale loads retain their
existing API and behavior.

The page has no second SCL/KBM authority. Its asynchronous chooser still uses a
JUCE SafePointer lifetime guard and calls the same mapping method as the native
regression. Source presence is independent of the description, so an unnamed
scale remains usable. No new work or lock was added to the audio callback;
publication still uses its existing try-lock handoff.

The status label and presence flag are read separately. A concurrent restore can
produce a transient mismatched label until the next visible-page timer refresh
(250 ms). This is not an atomic UI snapshot guarantee. The action itself always
uses the independently guarded processor state.

## Verified behavior

The native pre-fix harness reproduced **13 failures in 29 checks** on the staged
baseline: reopen/restore imports, stale-source resurrection, unnamed-scale
presentation and stale publication. It exposed the existing chooser precondition
and editor-local callback through a behavior-preserving helper, and invoked the
old unguarded publisher for deterministic read/replace/publish interleavings.
It did not introduce the intended fix or require a native file dialog.

The fixed native suite passes **35 checks**, adding complete-state retention for
malformed, oversized and unrepresentable-frequency mappings, default mapping
restoration via an empty KBM, and rejection of a competing mapping-only publish
with unchanged SCL text. It also checks all host parameters remain unchanged,
session serialization preserves the source, and a reopened-page A4=444 Hz mapping
actually reaches the running Engine. The test uses heap-owned processors and
isolated temporary storage.

Seven focused CTests passed: the new tuning-page test, existing voice-activity,
keyboard-handoff and context-menu native tests, plus tuning MIDI, frequency-domain
and robustness tests. The new native test also passed 20 consecutive runs.
Fresh GCC UBSan + float-cast-overflow + library-assertion builds passed the
unchanged frequency-domain (826 checks) and robustness (2,785 checks) suites.
Those sanitizer results cover the tuning core tests, not the native plugin.

Independent review checked the snapshot/version protocol, existing audio handoff,
page ownership and tests, and independently reproduced both the pre-fix failures and the final 35/35
native result.
This is native Linux/JUCE component and processor verification, not automation
of a platform file picker, a DAW session, or a cross-platform release pass.

## Reproduction and evidence

With the repository's documented native dependencies (verified here with GCC
14.2.0 and JUCE 8.0.11):

```sh
cmake -S . -B build-scala -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCS80_BUILD_PLUGIN=ON -DCS80_BUILD_TESTS=ON
cmake --build build-scala --parallel 2 --target CS80Snapshots \
  cs80_tuning_midi_tests cs80_tuning_frequency_domain_tests cs80_tuning_robustness_tests
ctest --test-dir build-scala --output-on-failure \
  -R '^(cs80_keyboard_tuning_ui_tests|cs80_voice_activity_host_tests|cs80_keyboard_handoff_host_tests|cs80_context_menu_keyboard_tests|cs80_tuning_midi_tests|cs80_tuning_frequency_domain_tests|cs80_tuning_robustness_tests)$'
ctest --test-dir build-scala -R '^cs80_keyboard_tuning_ui_tests$' --repeat until-fail:20
```

The [evidence directory](evidence/scala-session-2026-11-09) contains source/result
hashes, the actual baseline output, the baseline-only helper patch and its
compatibility test fixture. To reproduce that negative control, create a
separate checkout of the pinned staged baseline, apply
`git apply --unidiff-zero baseline-test-harness.patch`, and copy `KeyboardTuningUiTests.baseline.h` to
`tests/KeyboardTuningUiTests.h`. Build its CS80Snapshots and run
`--keyboard-tuning-ui-tests`; the recorded result is exit 1, 29 checks and 13
failures. Do not apply that negative-control patch to the production checkout.

## Host save notification

Changed user imports and CLEAR now notify the host that the session needs saving,
while identical text, empty CLEAR and host restores stay silent. See
[Tuning state notifications](TUNING_STATE_NOTIFICATIONS.md) for the action boundary,
reentrant listener behavior and focused verification.
