# Nominal shared pressure-to-speed bus

## Source and bounded model

The original Yamaha CS-80 service manual page25 (TRG5) connects the a1-a8 pressure sources to individual diode anodes. The common cathode bus has 47k to E and feeds IC17 through330k. See [the source evidence and native crop hashes](INITIAL_TOUCH_CIRCUIT_EVIDENCE.md#shared-pressure-to-speed-bus-native-trg5-evidence).

For ideal diodes and stiff nonnegative sources, this is a maximum selector, not an average or latest-event latch. The engine now takes the normalized maximum pressure of active, held voices for shared SUB speed. A weaker pressure event cannot replace a stronger held pressure, and releasing the strongest proxy exposes the remaining maximum.

This is NOT an exact electrical diode model. Forward voltage, current sharing, source impedance, IC17 loading, the TWS a-path nominal100ms smoothing and pressure-to-voltage calibration remain unmodeled. The existing shared-speed frequency law and its existing smoothing are unchanged; this patch does not calibrate either.

## Explicit software policy

- Only active voices whose key/voice gate remains held contribute. MIDI pedal-held sound and release tails do not represent continued physical pressure.
- Existing per-voice pressure destinations and MIDI channel matching are unchanged. In particular, this patch does not redesign non-MPE poly pressure or master-channel pressure semantics.
- Messages that match no voice cannot create a global pressure value.
- Voice assignment already initializes pressure to zero; reusing a released circuit cannot inherit its previous bus contribution.
- The software arpeggiator contributes through its generated voices' held gates. This is an extension policy, NOT the original CS-80 physical keyboard scanner. Pressure on remembered source keys and generated octave notes is not newly remapped by this patch.
- No parameter identifiers, serialization fields or host control counts change.

## Regression coverage and limits

At44.1/48/96/192kHz, paired deterministic audio renders isolate shared speed -> SUB VCO while all other pressure destinations are zero. They cover event-order independence, nondominant changes, dominant pressure zero/release, unmatched note/channel messages, pedal tails, all-released neutrality, and circuit reuse. An audible pressure-sensitive positive control prevents a silent or disconnected fixture from passing the equality checks.

These tests establish software behavior only. Compilation and execution must be reported from CI separately; they do not certify a measured1:1 analog match.
