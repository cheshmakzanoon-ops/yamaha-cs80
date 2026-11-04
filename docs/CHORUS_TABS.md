# Independent chorus and tremolo tabs

2026-10-31. `chorus_tabs` is appended after every shipped parameter, with AU
version hint 16. Its default is 0, **Historical mode**. That preserves the
existing `chorus_on` section master and `chorus_mode` choice exactly. The other
states are 1 **Both off**, 2 **Chorus**, 3 **Tremolo**, and 4 **Both on**.
The section master gates both effects in every state.

The circuit engine uses the new physical tab states. Legacy continues to use
its shipped two-parameter law, irrespective of the new parameter. These are
physical panel controls, so PURE CS-80 retains them; they are not a modern FX
overlay. The default is neutral for every existing preset and saved session.

## Evidence and inference

OE1/OE2 routing, the two equal 100 kOhm BA617 modulation feeds, and the
adjustment procedure's printed p21 DO1/DO2 periods and O1..O4 amplitudes are
recorded in `BBD_CHORUS_MODEL.md`, `OE2_CIRCUIT_EVIDENCE.md`,
`BA617_SOURCE_REVIEW.md`, and `reference/Oe1Modulation.h`.

The existing CHORUS law moves the BBD clock; the existing TREMOLO law moves
the four wet-return VCAs. Applying both existing laws simultaneously is a
**declared inference**. It is not a newly recovered switching circuit or a
hardware measurement. The alternatives are mutual exclusion or a switching
network that alters depth/loading when both tabs are down. Neither alternative
is silently ruled out by the new software state.

The delay history, noise generators, filters, and DO1/DO2 oscillators continue
to run while off. Changing a tab does not clear a capacitor, reset a phase,
restart an envelope, or introduce a fade.

## Qualification

Limits were committed before evaluation in
`tests/parity/tolerances-v2/chorus-tabs.json`. The regression covers all four
tab combinations and the historical mapping at 88.2, 96, and 192 kHz internal
rates. An independent closed-time OE1 oracle applies the four VCA gains to
the chorus-only moving-clock wet signal, so a mutually exclusive clock/VCA
implementation cannot pass the both-on check.

Initial qualification: 144,037 checks, zero failures. Maximum both-on oracle
error `4.79992412e-8` (limit `2e-6`); the both-on/chorus-only difference reaches
`0.0725257993` (required positive control `>1e-6`). Historical modes are exactly
equal in all three engine policies; every new state is inert in Legacy.
Five mutations were rejected: ignore the new parameter; suppress clock
modulation during tremolo; suppress VCA modulation during chorus; reverse the
historical default; and apply the new state to Legacy.

Full compatibility, host parameter/state validation, and UI interaction
qualification are recorded in the continuation status when integrated.
