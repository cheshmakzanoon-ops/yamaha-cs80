# Preset browser opening-state REVERT

## Reproduced fault and bounded correction

At `ce5896c6`, the browser saved only core parameters, name and program index.
REVERT loaded a factory program, then applied those core values. Both operations
selected PANEL. A hardware factory tone or MEMORY selection could therefore
come back with the opening name but different audio. Changes only to hardware
sources or any of the four MEMORY banks did not enable REVERT.

The browser now captures a typed, bounded snapshot: core parameters, two
hardware source selectors, all 104 valid MEMORY parameters, name and program
index. Detection compares these same recall-eligible fields; MEMORY changes
count even when the bank is not selected. The existing core relative comparison
tolerance (1e-4 times max(1, opening magnitude)) also applies to MEMORY values.
Opening or reopening captures a new baseline; catalogue refresh does not.

A narrow processor recall validates the complete recalled domain before writing
anything, applies core values without an intermediate PANEL reset, restores
MEMORY and source values directly, then publishes the captured name/index.
Each parameter is host-notified once, without creating source/MEMORY user edit
gestures. Like the existing preset-load boundary, an actual REVERT synchronizes
parameter state and clears undo/redo history. A no-op does neither. No processor
DSP state, host ID, parameter order, or ordinary preset loading policy changes.

## Deliberate boundaries

- REVERT retains its prior physical/setup and PURE exclusions. The player's
  current pedal position, jack/output configuration, quality, tuning source and
  other `isPhysicalSetupParameter` values stay live.
- Ordinary **user-file** recall has a different existing contract: saved pedal
  and output-switch values are recalled, while PURE, oversampling, output jack
  and tuning source stay live. That policy is unchanged and separately tested.
- Modern-layer MSEG shapes and matrix routing are **not included** in this
  correction. Their omission from opening-state REVERT is a separate remaining
  limitation. This is not a claim of complete sound/session restoration.
- Tuning data, MIDI maps, UI state and A/B slots are untouched. No full APVTS
  tree is copied back, so unrelated session data is not overwritten.
- Reads/writes are per parameter, not atomic against concurrent host automation.
  Live DSP phase, envelopes, held notes and effect tails are not snapshotted.

## Regression and qualification

`tests/PresetRevertTests.h` uses the real browser and processor with disposable
preset storage and no native peer. It covers every source on both ranks, every
valid MEMORY control in every bank, hidden MEMORY edits, custom name/index and
unsaved values, refresh/reopen, host notifications, no-op behavior, undo/redo,
malformed snapshot admission, and both recall setup policies.

Fresh-note rendered controls compare independent processors for a factory tone,
MEMORY A and MEMORY B, including both ranks. Non-silent positive controls and
unreverted negative controls establish that the snapshots affect audio. These
are deterministic offline processor renders, not native plugin/DAW validation.

The original production-code baseline fails **263 of 426** checks (exit 1).
The final focused test passes **736 checks with zero failures**, and all
**13 focused/adjacent CTests** pass sequentially (exit 0). Independent final
review verified the executable and all eight source hashes, then reproduced
**736/736 checks** and **13/13 CTests**, both exit 0, with no blocking finding.
The baseline test source, execution log and executable hash are retained in
[evidence](evidence/preset-revert-2026-11-11/).

```sh
cmake --build build-native --target CS80Snapshots
ctest --test-dir build-native -R '^cs80_preset_revert_tests$' --output-on-failure
```

The focused regression is also included in the existing full UI smoke entry
point. Qualification here is limited to the explicit tests recorded in the
manifest; it does not imply a full suite, native peer, sanitizer, pluginval,
macOS/Windows, host automation race or remote CI pass.

The isolated Linux Release build uses GCC 14 and the clean, repository-pinned
JUCE 8.0.11 checkout. The final snapshot executable SHA-256 is
`9e02363504427c0755929386209814ff4cb90a1964279c2a7dd78563a2512341`.

[Exact source/binary hashes and recorded results](evidence/preset-revert-2026-11-11/manifest.json).
