# Nominal shared pressure-to-speed bus

## Source and bounded model

The original Yamaha CS-80 service manual page25 (TRG5) connects the a1-a8 pressure sources to individual diode anodes. The common cathode bus has 47k to E and feeds IC17 through330k. See [the source evidence and native crop hashes](INITIAL_TOUCH_CIRCUIT_EVIDENCE.md#shared-pressure-to-speed-bus-native-trg5-evidence).

For ideal diodes and stiff nonnegative sources, this is a maximum selector, not an average or latest-event latch. The Legacy engine takes the normalized maximum pressure of active, held voices for shared SUB speed. A weaker pressure event cannot replace a stronger held pressure, and releasing the strongest proxy exposes the remaining maximum.

That Legacy ideal-maximum policy is NOT an exact electrical diode model. Forward voltage, current sharing, source impedance, IC17 loading, the TWS a-path nominal100ms smoothing and pressure-to-voltage calibration are not modeled by that Legacy selector. The later Circuit path has its own TWS/diode model, described below. The existing shared-speed frequency law and its existing smoothing are unchanged; this patch does not calibrate either.

## Explicit software policy

- Only active, held voices supply ongoing force. Legacy selects their pressure maximum directly. MIDI pedal-held sound and release tails do not represent continued physical pressure; Circuit may retain a decaying TWS a-node response after the force input becomes zero.
- Existing per-voice pressure destinations and MIDI channel matching are unchanged. In particular, this patch does not redesign non-MPE poly pressure or master-channel pressure semantics.
- Messages that match no voice do not directly inject force into this shared-speed bus. With effective MPE enabled, the later member-onset cache can retain pressure for a future accepted allocation; that cache alone is not a sounding voice or bus source.
- Voice assignment starts from a zero pressure default, or from fresh retained member-channel pressure under the later [bounded MPE onset contract](MPE_MEMBER_ONSET.md). This is channel-role state applied at accepted allocation, not inheritance of the previous occupant's pressure target. It does not imply instantaneous clearing of every Circuit touch node or bus transient.
- The software arpeggiator contributes through its generated voices' held gates. This is an extension policy, NOT the original CS-80 physical keyboard scanner. Pressure on remembered source keys and generated octave notes is not newly remapped by this patch.
- No parameter identifiers, serialization fields or host control counts change.

## Current Circuit path and later onset retention

In the current source, every Circuit TWS a node advances each sample. Its force
input is `voice.active && voice.held ? voice.pressure : 0`; the existing diode
bus combines the resulting a nodes. Released keys therefore supply zero new
force, but their residual a-node decay can still affect the bus. This differs
from Legacy's direct maximum over active, held pressure targets and must not be
described as instantaneous Circuit bus neutrality on release.

The [MPE member-onset repair](MPE_MEMBER_ONSET.md) seeds a voice's pressure only
on accepted allocation. Retaining a pre-note member message does not itself
inject bus force. These statements reconcile the current source paths. A separate
[supplemental onset witness](experiments/mpe-member-onset/README.md) passes 32
scenarios and 1,853,296 checks under each GCC generic/native profile, covering
both engines, SUB modes, four sample rates and pedal/no-pedal release. It
compares SUB diagnostics after each host frame while normal internal ticks
advance, not at every internal tick directly. The witness checks no cache-only
bus force before allocation, a positive held-onset response and no fresh force
from stronger pressure after release. It preserves existing Circuit residual
decay instead of asserting instantaneous zero. This does not extend the
historical evidence below or certify analog calibration.

The later [MPE released-target repair](MPE_RELEASED_TARGETS.md) also stops ordinary
member pressure from retargeting a released voice. It does not change this
bus's gate or TWS decay: retained cache values alone still supply no force,
accepted held onsets do, and released/pedal-held gates supply no new force.
CC121 and manager/global exceptions remain separately documented. Neither
increment makes Circuit residual node decay vanish at Note Off.

## Regression coverage and limits (original increment)

At 44.1/48/96/192 kHz, the original ideal-maximum increment's fixtures use paired deterministic audio renders to isolate shared speed -> SUB VCO while all other pressure destinations are zero. They cover event-order independence, nondominant changes, dominant pressure zero/release, unmatched note/channel messages, pedal tails, all-released neutrality, and circuit reuse. An audible pressure-sensitive positive control prevents a silent or disconnected fixture from passing the equality checks.

The original evidence scope is unchanged. In particular, its all-released neutrality and circuit-reuse assertions do not establish instantaneous neutrality of the later Circuit TWS/diode path. These tests establish software behavior only. Compilation and execution must be reported from CI separately; they do not certify a measured1:1 analog match.
