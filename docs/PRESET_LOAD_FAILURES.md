# User-preset load failures

## Reproducer and contract

Open the library with a saved user patch listed, then move its `.cs80xml` file
out of the preset folder or replace it with malformed XML. Without refreshing
the browser, press LOAD, press Return, double-click its row, or select it with
AUDITION enabled. Previously the processor silently rejected the file while
the browser still called `onPresetLoaded` and showed no error. The previous
sound remained active, with no explanation for why the selected sound did not
load.

The processor now returns a `juce::Result` for user-preset recall. Missing or
non-file paths, failed XML reads/parses, and XML with a root other than the exact
`CS80_PARAMS` state type are failures before any live-state change. The browser
shows `Not loaded:` with the patch name and reason; its tooltip identifies the
full file path. It updates the selected row's inspector and actions, keeps the
live sound and loaded identity, and emits no success callback. The row remains
available to retry. A successful load replaces the failure status with `Loaded`
and emits the normal callback exactly once.

Success uses the same normalization and APVTS replacement as before. This does
not tighten admission of valid-root old or partial preset files, change the
schema, change host parameter order or audio processing, or alter USER preset
recall policy. In particular, PURE, oversampling quality, listening output, and
tuning source remain live; the documented recalled pedal/foot/output-level
controls retain their existing behavior. Save/overwrite, rename, trash,
duplicate-display-name identity, and browser REVERT semantics are unchanged.

## Regression

`tests/PresetLoadTests.h` creates an isolated temporary catalogue and drives the
production ListBox selection, LOAD callback, Return handler, and double-click
handler. It replaces the file only after the row has been populated. Missing,
malformed, unrelated-root, wrong-case-root, and empty files each run with both
manual LOAD and AUDITION. Assertions cover the full parameter tree and preset
metadata, callback count, named failure/path, selection and retry availability,
repeated failed attempts, repair without refresh, loaded marker recovery, and
live PURE/setup preservation. The same test runs in the full UI smoke suite.

```sh
cmake --build build-native --target CS80Snapshots
ctest --test-dir build-native -R '^cs80_preset_load_tests$' --output-on-failure
```

The separate `cs80_preset_file_recall_tests` CTest runs the existing
`testPresetRecallHost` and `testPresetFileHost` suites unchanged. They cover
successful user-file recall, tuning and pedal/output policy, legacy migration,
duplicate parameter normalization, malformed values, factory-index bounds, and
export/reimport. It does not create a native editor.

The focused CTest uses real JUCE browser components without a native peer. It
is not a physical mouse, native filesystem-permission, screen-reader, DAW, or
cross-platform GUI qualification. An unreadable file follows the same failed
XML-read branch, but the fixture does not modify filesystem permissions.

## Local evidence

The original implementation at `fc9c1ef7`, with only the new test/harness edits,
failed **40 of 112 checks**: four assertions for each of ten LOAD/AUDITION
cases. State/metadata preservation checks passed. See the retained
[baseline log](experiments/preset-load-failures/baseline.log).

The first fixture used the truncated text `<CS80_PARAMS` for the malformed
case. JUCE 8.0.11 unexpectedly accepts this as a root with no parse error; the
existing recall then normalizes defaults. That initial run failed **44 of 112
checks**, including extra state and loaded-identity failures. This is a separate
parser-admission issue, **not fixed here**. The bounded test was corrected to
`<CS80_PARAMS><`, which the same parser rejects as `tag name missing`. Retained
[initial log](experiments/preset-load-failures/initial-parser-probe.log),
[standalone probe](experiments/preset-load-failures/parser-probe.cpp), and
[parser output](experiments/preset-load-failures/parser-observation.log) document
the distinction. No production parsing or normalization policy was changed to
make either baseline pass.

The final Linux Release source passes **112/112 browser checks**, the existing
file/recall host suites pass **2,784/2,784 checks**, and all **12 focused/adjacent
CTests** pass sequentially. A no-op rebuild confirmed the final binary is
current. The first adjacent attempt found a stale executable missing the new
focused CLI entrypoint, because the harness was edited during an in-flight
compile. That [11/12 run](experiments/preset-load-failures/stale-harness.log) is
retained; the harness was explicitly recompiled before the final complete
rerun. No product regression is attributed to that build-state error.

The final executable SHA-256 is
`48f281ee92f48b8f3c203b4e32d1ddd0bdd0727623343918773300ad8f09a617`.
The isolated build used GCC 14.2 and the repository-pinned official JUCE 8.0.11.
No other build cache was reconfigured. No full native-peer UI smoke, sanitizer,
pluginval, macOS/Windows, DAW, or remote CI pass is claimed.

Independent final review found no blocker, verified the exact executable and
all seven changed source hashes, and reproduced all **12/12 CTests**, including
**112/112 browser** and **2,784/2,784 existing file/recall** checks, exit 0.
[Independent review](experiments/preset-load-failures/independent-review.json).

[Exact source/binary hashes and results](experiments/preset-load-failures/manifest.json).

The separate [complete XML admission change](PRESET_XML_ADMISSION.md) now closes
the truncated/mismatched-document issue described above at user-file LOAD.

A subsequent [loaded-identity fix](PRESET_LOADED_IDENTITY.md) distinguishes
same-name user files while preserving the failure/retry contract described here.
