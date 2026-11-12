# MSEG and matrix editor session-save notifications

## Reproduced issue

On source `9b94c7e8f09491f75d5fab823207473aca898c36`, ordinary MSEG **Sync** and matrix **Source** component actions changed the saved `MODERN_LAYER` configuration, but an attached `AudioProcessorListener` received no parameter or processor-change notification, even after the message-thread timer ran. These are non-parameter session fields, unlike matrix amounts and MSEG rates.

JUCE's `AudioProcessorListener::ChangeDetails::withNonParameterStateChanged` contract explicitly describes state changes that require re-saving and tells the host to offer to save before destroying the state. `ChangeBroadcaster` refreshed these components but did not notify that host boundary. This reproduction establishes the missing notification, not the behavior of a particular DAW's unsaved-project indicator.

The original three-action probe retained a graph-insertion attempt that did not change state because its displayed revision was stale. That attempt is not evidence of the defect. The two changed actions above are the baseline witnesses; final graph tests use current component state and require a real change.

## Bounded correction

- All seven MSEG/matrix editor publication sites request a save reminder only when a real configuration revision changes: graph edits, shape presets, typed point edits, shape copy, shape switches, matrix route copy/clear, and individual route fields.
- The existing parameter-state timer coalesces these requests. It exchanges pending flags before making one final `updateHostDisplay` call. Dirty and existing program-display flags can coexist. No host callback runs inside the component mutation/refresh stack, and a new edit requested during notification survives for the next timer.
- Processor Undo/Redo request the same reminder when successful replay changes the modulation configuration, not for parameter-only or unavailable replay.
- Generic modulation setters remain silent. Accepted session restoration cancels an earlier pending editor reminder, and later user edits can notify again. Session restoration itself does not become a dirtying user action.

No DSP, host parameter, XML schema, timer frequency, audio-thread publication, preset recall policy, or protected fader/Ribbon files change. Shape drags may generate reminders across multiple timer intervals; this is coalescing, not a promise of exactly one notification per complete drag. Delivery occurs on the existing timer, not synchronously with each edit.

## Regression evidence

The dedicated `cs80_modern_state_notification_tests` target uses real processor, MSEG page and matrix page instances without native peers. It checks three shape slots, all sixteen matrix rows and four route fields, real graph and numeric edits, shape copy and metadata-only route copy/clear, no-op callbacks, coalescing, combined program flags, reentrant save, Undo/Redo, parameter-only amount notifications, generic-setter silence, accepted-restore cancellation, subsequent edits, and component destruction before or during deferred delivery.

Final Linux Release build and sequential safe selection: **9/9 passed in 31.80 seconds**, explicit exit 0. The new target reports **949 checks, 0 failures**. Adjacent gates report tuning notifications 452/0, Undo gestures 317/0, hardware-source history 307/0, A/B/randomise history 106/0, matrix route actions 2574/0, matrix field routing 481/0, MSEG shape copy 215/0 and MSEG editor lifecycle 803/0.

The new test executable SHA-256 is `a65a7eb362482d74c9efedde47ff94c8713198e011311cbe8f56c35c7f149381`. Six source/build-definition hashes and all three exercised executable hashes are retained with the raw build/test logs and baseline probe. The final selection contains no parser, malformed-input, native-peer or allocation suite. Existing nonfatal compiler warnings are not a warning-free-build claim.

Independent final review verified the six source and three executable hashes before and after execution, then repeated the same selection: **9/9 passed in 31.76 seconds**, explicit exit 0, including **949/0** in the new target. Source, action boundaries, no-op/replay/restore semantics and deferred lifetime handling were approved without a remaining blocker.

## Limits

This is a Linux direct-component/processor notification qualification, not native window interaction, a built VST3/AU wrapper or DAW save-prompt qualification. It preserves the existing processor-outlives-editor contract. The revision/config comparisons and cancellation cover serialized editor/restore workflows; they do not make concurrent host restoration and editor mutation globally atomic. Parameter notifications retain their existing semantics. This does not assert that every other non-parameter session domain has been audited.
