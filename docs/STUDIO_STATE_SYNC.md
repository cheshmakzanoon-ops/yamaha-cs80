# Restoring studio preferences with an editor open

The processor restores `UI_STATE` when the host loads a valid saved session, but an existing `StudioMount` previously consumed QWERTY, controller-strip and circuit-note preferences only at construction. The saved values and the live editor could disagree until the editor was reopened.

The existing message-thread `sync()` now consumes those three preferences. A QWERTY enabled-state transition uses the existing keyboard setter, so disabling releases keys owned by that keyboard. The controller preference is retained while PURE mode hides the strip, then reapplied when PURE is left. Circuit notes are updated only on a local preference transition.

Circuit-note tooltips retain their existing process-global storage. Each mount caches its last-applied preference so unchanged timer ticks do not repeatedly overwrite another instance's choice. This is not per-instance tooltip isolation.

There are no state-schema, parameter, DSP or fader changes. Restore does not create a new host dirty notification. The existing one-time tour-opening policy is unchanged. Headless component tests do not constitute native DAW/window qualification.
