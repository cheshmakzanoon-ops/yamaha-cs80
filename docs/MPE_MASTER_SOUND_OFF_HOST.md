# Host MIDI MPE master All Sound Off

The core [MPE master CC120 correction](MPE_MASTER_SOUND_OFF.md) previously did
not reach normal host MIDI dispatch: the processor special-cased All Sound Off
by calling the intentionally channel-local public `allNotesOff` API. The
processor now routes CC120 through the core controller entry point for both
primary and extension engines.

Processor-owned host-key and UI-held masks are cleared over the same current
master/member zone using the primary engine's effective MPE state and negotiated
layout. This avoids leaving held-key display/ownership behind after the affected
voices are silenced. Member CC120 remains channel-local; disabled MPE and PURE
retain conventional scope. CC123, the public core API, controller/pedal state,
shared effect tails and the core's current-zone policy remain unchanged.

## Regression evidence

A real `processBlock` fixture covers legacy and circuit paths, 8/16 voices,
zero/32-frame blocks, lower and upper masters, member-only CC120, disabled MPE
and PURE. It fills available voices across both zones and a conventional channel,
then checks each affected/unaffected voice and corresponding host-held key mask.
The registered controller-host CTest reports 2304 checks: 160 failures before
the adapter correction, zero failures afterward.

One freshly linked Release native invocation passes six selected CTests, zero
failures, 13.42 seconds:

- `cs80_controller_reset_host_tests`
- `cs80_keyboard_handoff_host_tests`
- `cs80_voice_activity_host_tests`
- `cs80_sixteen_voice_release_host_tests`
- `cs80_mpe_released_target_host_tests`
- `cs80_mpe_member_onset_host_tests`

These are processor fixtures, not an external DAW, plugin wrapper or physical
MPE controller qualification. An initial manual command used an unsupported
CLI spelling and entered the default snapshot workflow, which crashed; it is
excluded from this evidence. Both baseline and final results above use the
exact registered CTest commands. No rendered-UI pass is claimed.
