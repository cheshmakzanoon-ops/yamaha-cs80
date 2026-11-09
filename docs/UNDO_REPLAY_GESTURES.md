# Host gestures for plugin undo and redo

## Scope

The plugin's Undo/Redo commands now bracket effective parameter replays with
host `beginChangeGesture` / `endChangeGesture` notifications. They no longer
rely on the unbracketed value notification produced by a plain JUCE ValueTree
property action. This is a message-thread history/UI change, not a DSP change
or a real-DAW automation qualification.

The normal control edit still produces its original gesture only. Parameter
IDs, ranges, defaults, saved-state schema and recorded values are unchanged.
Side-state-only history, such as a stored A/B snapshot or metadata, does not
invent parameter gestures. Initial programmatic edits keep their existing
notification behavior; this increment covers replay, not every editing command.

## Source contract

`UndoableParameterState::write` records an internal parameter-value action
instead of JUCE's generic property action when the write belongs to user
history. Non-user `write(..., nullptr)` remains a direct, non-undoable tree
write. The action retains:

- The exact child tree, original property presence and original `juce::var`.
- The new value. Adjacent actions on the same owner/child coalesce into the
  first old value and last new value, as JUCE's property actions did. Actions
  that originally added a missing property do not coalesce.
- The existing transaction order. An interleaved transaction may contain more
  than one action for a parameter; each effective replay action has a balanced
  gesture, not necessarily one gesture per parameter for the whole command.

The initial `perform()` only records the already-delivered control edit. Replay
is identified by `UndoManager::isPerformingUndoRedo()`, not a `firstPerform`
flag. This distinction matters because JUCE constructs a coalesced replacement
action without performing that replacement before its first undo/redo.

On replay, eligibility follows JUCE APVTS dispatch: a `PARAM` child must still
belong to this live state and its current ID must resolve to a parameter. The
actual ID is used, not a core-index/host-index assumption. Duplicate children
are not silently replaced by a last-child rule. A detached or unmapped child
can retain its existing tree replay semantics without inventing a host edit.
Removing an originally absent value uses APVTS's parameter-default fallback.
An already-identical tree write, or an approximately equal physical value under
APVTS's own comparison, needs no parameter notification and emits no gesture.

Replay's gesture callbacks are excluded from history classification. They must
not reopen a user transaction, increment its gesture count, synchronize other
changes mid-replay, or flush another history action. Parameter-value replay
callbacks retain their existing exclusion.

`Cs80AudioProcessor::undo()` / `redo()` delegate to guarded state entry points.
They reject an off-message-thread call, an active original gesture or
`ScopedUserEdit`, a recording-write callback, or reentrant replay. Rejection
performs no undo/redo; the original edit can finish and the caller can retry.
After replay, the next edit opens a newly named transaction even if no message
loop turn has occurred yet. The raw UndoManager accessor remains available for
queries and side-state recording, not as an alternative replay entry point.

Parameter actions carry the existing weak state-lifetime token. An expired
action refuses execution or coalescing before accessing its former owner;
destroying retained actions is also safe. JUCE's UndoManager returns `true`
when it finds a transaction even if an action fails, then clears that failed
history. The isolated lifetime tests check this refusal/clearing behavior, not
a new promise that every generic metadata action can outlive its owner. Raw
UndoManager replay is still not the supported plugin entry point. State
replacement continues to clear the history.

## Concurrency and coverage limits

Gesture eligibility and the physical-value comparison are action-time checks,
not a transaction against concurrent host automation or state mutation, nor
a guarantee for concurrent owner destruction. A
concurrent write can occur after a check. The existing global user-gesture
classification, atomic parameter storage, and tree lock are retained, not
redesigned into per-parameter or cross-thread ownership. This increment does
not establish a stronger simultaneous-automation guarantee.

The supported recording path is this plugin's `UndoableParameterState::write`.
Arbitrary third-party raw tree actions, direct UndoManager replay, or a caller
using JUCE's base `copyState()` to create generic property actions are outside
this change. No repository production call uses those as the plugin's normal
parameter replay path.

## Local verification

`tests/UndoReplayGestureTests.h` runs real JUCE parameter/state objects and
processor instances without opening an editor, native peer or popup. It checks:

- Initial edit versus replay order, repeated undo/redo and no recursive history.
- Mid-gesture flush/coalescing, first redo of a coalesced action, and interleaved
  multi-parameter order.
- Active-edit, off-thread, recording-write and replay-callback rejection,
  original-gesture completion, and the next transaction's name.
- External automation exclusion; exact/approximately-equal no-op replay;
  missing properties and original variant types; duplicate and detached children.
- Side-state-only/mixed transactions, state replacement, and action destruction
  after the state owner, including expired replay/coalescing refusal.
- Actual processor entry points, core controls, selectors and all eight memory
  SAW/PULSE host IDs, with all-parameter notification observation.
- The unchanged session-save, A/B-history and randomise-history regressions.

The registered gate is `cs80_undo_replay_gesture_tests`, or:

```sh
CS80Snapshots --undo-replay-gesture-tests
```

Eight private-object negative controls remove replay notifications, the replay
listener guard, active-edit protection, original coalesced value, original
property absence, external-automation exclusion, recording-write protection,
or the message-thread guard. A fault counts only if it compiles and links,
exits normally with the expected failing status, and reports actual behavioral
failures. The source and normal build objects are not modified.

Frozen source `5740dd4` passes **317 checks**, with zero failures. Eight focused
and neighboring CTests also pass, including the switch and controller-reset
host gates. All eight faults complete 317 checks normally and fail the intended
behavioral gate: **49 / 1 / 19 / 2 / 1 / 21 / 2 / 1** failures in the order
listed above. See the [commands, raw results and hashes](evidence/undo-replay-gestures-2026-11-09/manifest.json).

A separate ASan/UBSan run also passes 317 checks. Its instrumentation is
explicitly partial: the changed `UndoHistory.cpp`, `PluginProcessor.cpp`, and
focused fixture are instrumented, while unchanged native/JUCE objects are
reused. Leak detection is disabled. It is not a whole-plugin/framework
sanitizer qualification.

A focused parameter-listener gate is not a VST3/AU wrapper or DAW recording
session. No new full UI smoke, pluginval, remote CI, native-input, audio
bit-compatibility, screenshot-refresh or release qualification is claimed here.
