# Bend-range defaults after an MPE Configuration Message

## Contract and source

MPE v1.1 §2.2.5 (page 14) requires a receiver of an MPE Configuration Message (MCM) to set the zone manager's pitch-bend sensitivity to 2 semitones and its members' sensitivity to 48 semitones. Later RPN 0 messages may override those values. Section 2.2.1 and Appendix B describe each MCM as configuring one addressed zone.

The [official MPE page](https://midi.org/mpe-midi-polyphonic-expression) links the specification. The retained M1-100-UM, 14 April 2022 PDF has SHA-256 `c5f6727af065e454fe9bc4ff80f66414e8b25ffb6c1019ce7795ab4a89da1f1e`; it is not redistributed here.

Previously, MCM handling stored the negative “use parameter fallback” sentinel for both ranges. A non-default user parameter therefore silently overrode the required received-MCM defaults. For example, with manager/member parameters 5/12, half-range bends produced 8.5 semitones rather than 25. The independent core witness reproduced this in Legacy and Circuit for both lower and upper zones.

## Bounded correction

A valid MCM now stores explicit manager/member ranges 2/48 in its addressed zone. A repeated same-size MCM also reinstates those defaults. Count zero stores the defaults while disabling the zone; they are inert while it is disabled. Negotiated ranges of the opposite zone remain untouched even if its membership is reduced or disabled by overlap.

Manual/startup MPE without a received MCM retains its existing parameter-fallback behavior. Later parameter edits cannot override ranges already installed by an MCM or RPN 0. Later RPN 0, including the existing fractional Data Entry LSB support, still works. Member-channel RPN 6, NRPN/null selection and non-MCM CC38 traffic do not install defaults. MPE-off and PURE admission policies are unchanged.

This intentionally changes pitch for workflows that receive an MCM while depending on non-default fallback parameters. It is not a compatibility-only optimization. Already-scaled manager bend targets are not recomputed by the MCM itself; the pitch witness sends a new bend after configuration. Existing target/active-note retargeting and full MCM note/controller reset semantics are separate work; this correction does not claim full MPE conformance.

## Evidence

The initial 198-check layout/native processor fixture failed 112 checks against production source `6014f88d` before the correction. It covers all 0–15 zone sizes, both addressed zones, overlap and opposite-range preservation, repeated MCMs, ignored-message controls, later fractional RPN 0, manual fallback and later parameter edits. Native cases use Legacy/Circuit, eight/sixteen voices, zero-frame/rendered callbacks and actual member pitch plus settled manager oscillator pitch. Sixteen-voice cases configure the primary before activation, exercising the controller-state handoff.

Review strengthened same-size renegotiation and the live-MCM-MSB CC38 control; the expanded final fixture has 199 checks. The first broader run passed 21/23 tests in 101.63 seconds and exposed six old assertions that deliberately expected parameter fallback after MCM (two member-onset and four released-target checks). Their expected ranges are updated to the new explicit 48-semitone contract; manual pre-MCM fallback remains independently tested. The affected build completed 62 steps, followed by a final six-step rebuild of the strengthened and updated fixtures. All 23 selected tests then passed in 101.00 seconds, including the expanded MCM fixture at 199/0, related MPE/controller/tuning tests, allocation and state/undo tests, and all six actual VST3 rate/mode registrations. An independent repeat of the same 23-test selection on `17f68d15` (runtime source `d61e83f2`) also passed in 98.24 seconds, including MCM 199/0, tuning 39,219/0 and allocation 204/0. No external DAW/controller, GUI, complete MPE compliance or CPU/deadline guarantee is implied. Historical intermittent offline-render failures remain documented and are not declared fully explained by this change.
