# Forward A/B copy host-save notifications

The inactive A/B comparison slot is serialized as `AB_COMPARE` in the session. Copying the current sound into that slot previously changed saved session data without notifying the host that non-parameter state was dirty.

The normal `abCopyCurrentTo` path now compares its library-state serialization before and after the operation, under its existing message-thread state lock. A changed serialized result queues the existing coalesced non-parameter notification. The host is not called inside the copy operation.

Comparing the persisted view intentionally excludes active-slot-only copies, identical snapshots and in-process preset-origin data, which are not changes to the saved inactive slot. Invalid destinations remain no-ops. This adds no serialization or allocation to the audio callback and makes no DSP or state-schema changes.

Scope is forward copy only. A/B toggle and history replay paths are unchanged and not qualified by this change; CopyAction undo/redo notification behavior remains a separate follow-up. Headless processor/listener checks do not establish native DAW save-prompt behavior.
