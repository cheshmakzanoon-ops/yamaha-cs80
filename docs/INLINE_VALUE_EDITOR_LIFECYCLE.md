# Inline typed-value editor lifecycle

An inline value editor belongs to the exact knob binding and editor instance
that opened it. Previously an open editor survived parameter rebinding, so
Return could apply text intended for one control to another. A deferred close
from an older editor could also delete a newly opened replacement.

Successful binding to a different parameter now cancels its old inline editor.
No-op rebinding retains the editor. Return uses safe pointers to the knob and
text editor, verifies the current editor identity and binding generation, and
checks enabled state and absence of an active slider gesture before editing.
Deferred close callbacks only close their own editor instance. Deleted or
replaced editors cannot redirect those callbacks to a new one.

Fresh valid Return still uses the existing parser and one slider edit gesture;
invalid text remains in the editor for correction. Escape and focus loss retain
their cancel/close role. Showing an editor on a disabled control or during an
active slider gesture is ignored. No parsing units, parameter IDs, DSP, drawing,
or state-format changes are included.

The native callback tests reproduce rebind cancellation, stale Return, stale
queued close, and disabled commit, and verify a fresh current edit and its
closure. They use actual JUCE components and processor parameters without a
native peer. Native OS text focus, physical keyboard routing, popup rendering,
and real-DAW automation qualification are separate.

## Verification

The unchanged preceding UI with the regression matrix completed **46 checks
with four behavioral failures**. The fixed implementation completed **46 checks
with zero failures**. Context-menu lifecycle, switch context, typed-value units,
and undo replay native CTests passed together (4/4, 2.77 seconds).

The retained official JUCE 8.0.11/local Linux Release setup is the same as the
[knob context-menu checkpoint](KNOB_CONTEXT_LIFECYCLE.md). Nonfatal Fontconfig
cache warnings remain; the callback result is not screenshot, cross-platform,
DAW, or complete accessibility qualification.
