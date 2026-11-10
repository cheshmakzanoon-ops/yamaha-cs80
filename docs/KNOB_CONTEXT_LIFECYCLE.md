# Knob and fader context-menu lifecycle

Parameter knobs/faders can be rebound while their asynchronous context menu is
open. In particular, the LFO controls switch between original and modern
parameters. A menu built for the old binding must not reset the new parameter
or arm MIDI learn using a stale binding.

Each menu action now retains the binding generation, parameter identity and
host index, together with the existing safe component pointer. It checks that
identity and the control's current enabled/gesture state before executing.
Successful A-to-B-to-A rebinding still invalidates the old action. A no-op
binding to the same parameter does not invalidate it.

MIDI learn additionally rechecks PURE at action time; cancel rechecks the
pending parameter, and forget rechecks the CC mapping seen by the menu. These
are sequential stale-action guards around existing atomics, not atomic
conditional transactions or a MIDI-learn concurrency redesign.

Fresh reset, learn, cancel and forget actions retain their existing behavior.
The common identity guard also covers menu-launched typing, copy, paste and
help. The original menu-only change did not cover an inline value editor already
open before rebinding; that is addressed separately by the
[inline editor lifecycle correction](INLINE_VALUE_EDITOR_LIFECYCLE.md). It does
not add context menus to uncovered controls.
DSP, parameter IDs, state formats, drawing and hardware modeling are unchanged.

Tests invoke actual JUCE menu callbacks on real parameter controls and the
processor without opening a native popup. Native OS focus/popup behavior and
real DAW automation qualification remain separate.

## Verification

The baseline context-menu matrix on the preceding unchanged UI implementation
completed **37 checks with 7 behavioral failures**. Two additional inherited/inner
slider disablement checks were added to the final matrix; the fixed native target
completed **39 checks with zero failures**. The baseline count is not presented
as a run of that expanded final matrix. Four native JUCE CTests passed
(context-menu keyboard/lifecycle, illuminated-switch context, typed values and
undo replay; 4/4, 2.66 seconds). Tests cover rebound reset, A-to-B-to-A learn,
PURE entry, stale cancel/forget, disabled controls, active edit rejection,
fresh actions and deleted-component safety.

The local Release build used the retained official JUCE 8.0.11 source and staged
Linux build dependencies. Fontconfig emitted cache-directory warnings; these
callback tests do not validate rendered fonts or screenshots. No native popup
was opened, and no whole-plugin, cross-platform, DAW, performance or complete
accessibility qualification is claimed. As with parameter attachments, the
processor must outlive its editor controls.
