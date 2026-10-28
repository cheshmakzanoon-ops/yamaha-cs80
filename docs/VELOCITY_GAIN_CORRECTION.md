# Remove unconditional velocity attenuation

The original owner manual describes velocity response through selected initial-touch
destinations, rather than a compulsory permanent gain multiplier.
[Original manual, TOUCH RESPONSE and initial-touch explanation](https://device.report/m/08eee3fd014ab14d5f15fe50a150900b9c3ed22ad75895b5736f46b2e186f9b0).

The engine previously multiplied every voice by 0.38 + 0.62 * velocity, even with
Initial Level, Initial Brilliance and Initial Pitch Bend all zero. That made
disabling touch response impossible. This multiplier is removed. Velocity remains
available to the explicit initial-touch destinations and to MIDI/arpeggiator
metadata.

## Audible compatibility impact
Previously saved sessions and factory programs can play louder for velocities
below maximum because this unrequested attenuation is gone. Maximum-velocity gain
is unchanged. There is deliberately no hidden automatic gain compensation:
adding one would restore another undocumented response. Existing parameter IDs,
values and state remain unchanged.

## Remaining limitation
Initial Level and Initial Brilliance use sustained velocity values. Direct tracing
of TWS17 and TRG19/23 shows a sampled/held b path for these destinations and a
separate AC-coupled C path for initial pitch bend. The earlier shared-decay
interpretation is withdrawn; see INITIAL_TOUCH_CIRCUIT_EVIDENCE.md. The actual
sample/hold transfer, force/contact mapping, loading and separate pitch-transient
response still require circuit modeling and measurements. This correction does
not invent a shared decay time or certify the remaining model.

## Regression evidence
The explicit Initial Level test uses nonzero depth and isolates level from
brightness. New four-rate tests compare rendered attacks, held notes and releases
at different velocities with every initial-touch depth zero; they require
sample-identical output. Compilation and Actions results remain required.
