# Studio preference host-save notifications

QWERTY enabled, controller-strip visibility, circuit-note tooltips and tour dismissal are persisted in `UI_STATE`. Editing them previously bypassed the host's non-parameter dirty notification.

Explicit processor preference setters now compare the previous atomic value and queue the existing deferred host notification only on change. The real StudioMount edit paths and AdvancedPanel computer-keyboard toggle use these setters. Existing immediate QWERTY release, controller visibility and tooltip-cache handling remain in their UI callers.

Repeated values are silent. Multiple edits coalesce on the existing message-thread timer, where a host can save the completed state reentrantly. Accepted session restore retains direct atomic assignments and existing pending-update discard behavior. Periodic studio synchronization does not use these setters and stays silent.

No schema, DSP, host parameter IDs, or protected fader implementation changes. Circuit-note tooltips retain their existing process-global behavior. Headless component/processor qualification does not establish a native DAW's save-prompt behavior.
