# Preset browser opening-state REVERT

## Reproduced fault and bounded correction

At `ce5896c6`, the browser saved only core parameters, name and program index.
REVERT loaded a factory program, then applied those core values. Both operations
selected PANEL. A hardware factory tone or MEMORY selection could therefore
come back with the opening name but different audio. Changes only to hardware
sources or any of the four MEMORY banks did not enable REVERT.

The browser captures a typed, bounded snapshot: core parameters, two hardware
source selectors, all 104 valid MEMORY parameters, the three MSEG shapes and
16 matrix routes, name and program index. Detection compares these same
recall-eligible fields. Hidden MEMORY edits and zero-amount routes count too.
The existing core relative comparison tolerance (1e-4 times max(1, opening
magnitude)) also applies to MEMORY values; modern configuration compares exactly.
Opening or reopening captures a new baseline; catalogue refresh does not.

A narrow processor recall validates the complete recalled domain before writing
anything, applies core values without an intermediate PANEL reset, restores
MEMORY and source values directly, restores modern configuration, then publishes
the captured name/index.
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
- Tuning data, MIDI maps, UI state and A/B slots are untouched. No full APVTS
  tree is copied back, so unrelated session data is not overwritten.
- Reads/writes are per parameter, not atomic against concurrent host automation
  or concurrent session replacement.
  Live DSP phase, envelopes, held notes and effect tails are not snapshotted.

## MSEG/matrix extension

At `3a22dc45`, the preceding source/MEMORY correction still omitted modern sound
side-state. The real browser regression reproduced **219 failures in 958 checks**:
MSEG-only and matrix-only edits did not enable REVERT, and an actual saved
user-file audition left its shapes/routes behind after REVERT.

The opening snapshot now owns a `modern::ModernConfig` value, never a shared
ValueTree handle. All active points (time, level, curve, count), loop bounds and
mode, trigger, sync and bipolar flags, plus every source/via/destination/target
are in its recall domain. Inactive point padding has no recall authority and is
zeroed. Admission rejects any required sanitization of active fields before any
parameter, metadata, tree, notification or undo change. The existing modern
sanitizers and ordinary preset/session import policies are unchanged.

Modern recall uses the existing `ModernLayerState::setConfig` under the APVTS
tree lock, matching A/B recall's lock order. This preserves the live root and
existing MODERN_LAYER child identity, creates fresh shape/matrix descendants,
and leaves unrelated siblings and session state alone. No opening tree can
alias edits or a later `replaceState`. Missing and explicit-default modern trees
compare as the same sound. As with existing modern edits, unknown fields inside
MODERN_LAYER are not part of the typed sound contract.

The existing mailbox publishes one coherent modern configuration and the audio
thread takes it with a try-lock on a later block. This is not an atomic recall
with the separately published core/source/MEMORY parameters. UI notification is
asynchronous; no new audio-thread allocation, blocking lock or DSP-history
snapshot is introduced. The browser boundary is message-thread work. Modern
state is applied before the existing sync/undo-clear/final display boundary;
a no-op leaves history and notifications untouched.

The extension tests cover every shape option and route field in every slot,
all 32 points per captured shape, actual user-file audition, value-copy and tree
identity, refresh/reopen/session replacement, default absence, undo/redo,
191 malformed modern configurations with complete rejection checks, valid edge
values and ignored padding. Six independent fresh-note audio controls cover
shape and routing changes for all three MSEGs and all rank targets, with finite
non-silent references and audibly different unreverted controls.

The focused existing modern state, ordinary preset/session, A/B and malformed
import suites also have a separate CTest entry (`cs80_modern_recall_host_tests`),
without requiring the full UI smoke test.

The final extension regression passes **1,387 checks**, the existing modern and
recall suites pass **2,354 checks**, and all **16 focused/adjacent CTests** pass
sequentially, all with zero failures and exit 0. Independent final review
verified the binary, all seven changed source hashes and five unchanged modern
dependencies, then reproduced **1,387/1,387 checks** and **16/16 CTests**, exit 0,
with no blocking finding.

[Extension baseline, final results and exact hashes](evidence/preset-revert-modern-2026-11-11/manifest.json).

## Earlier source/MEMORY regression and qualification

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
For the earlier source/MEMORY correction, the focused test passed **736 checks
with zero failures**, and all **13 focused/adjacent CTests** passed sequentially
(exit 0). Independent final
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
JUCE 8.0.11 checkout. The earlier source/MEMORY snapshot executable SHA-256 was
`9e02363504427c0755929386209814ff4cb90a1964279c2a7dd78563a2512341`.

[Exact source/binary hashes and recorded results](evidence/preset-revert-2026-11-11/manifest.json).
