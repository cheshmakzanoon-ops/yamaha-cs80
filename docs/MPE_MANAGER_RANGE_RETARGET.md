# Retained manager bend when sensitivity changes

## Chosen behavior and evidence

For an effective MPE zone manager with a retained normalized wheel value, changing its effective sensitivity now rescales that zone's bend target without requiring another wheel message. This applies to RPN 0 (including fractional Data Entry), a same-role MCM default reset, and manual fallback parameter changes before negotiation. The existing controller smoother handles the new target.

MPE v1.1 §§2.2.5–2.2.6 permits sensitivity changes at any time and requires retention of manager bend for new notes and influence over sounding notes. It does not explicitly prescribe the exact instant of recomputation after RPN 0. This is our consistent product interpretation, not an invented “shall immediately retarget” quotation or complete MPE-conformance claim.

On `96fb7db3`, an independent Legacy/Circuit and lower/upper witness showed that .5 at range 12 produced 6 semitones; changing range to 4 left both current and future notes at 6 until the same bend was sent again. A zero-to-nonzero sensitivity change also lost the bend until another message. The initial actual-processor regression failed 122 of 224 checks.

## State and lifecycle boundaries

Two fixed-size records retain the last valid normalized **manager** inputs and their validity. Raw values are never recovered by division from semitone targets, so zero sensitivity does not destroy them. Actual lower/upper manager events populate the records; conventional or gap-channel input cannot masquerade as a manager input.

Reset, an effective MPE mode edge, or a manager endpoint role change invalidates rescaling eligibility. Mode/role invalidation preserves the previous target under existing behavior; it is not the still-open full MCM controller reset. After any applicable invalidation, only a changed effective range with a valid same-role manager record changes the target. Invalidated input cannot return through a later range edit. CC121 supplies a valid zero bend through the existing reset path.

The second instrument inherits these records, including validity, during the [controller activation handoff](CONTROLLER_ACTIVATION_HANDOFF.md). No per-voice member bend, release target, pitch-wheel matrix source, dezipper history, note queue or DSP history is rewritten. Manager changes continue to affect sounding release tails through the existing manager route. Member sensitivity retargeting and full MCM note/controller-reset semantics remain separate work.

## Disabled lower-zone boundary correction

The preceding MCM-default correction stored 2 for a disabled lower zone. Channel 1's intentional conventional fallback accidentally read that inactive manager range instead of the ordinary parameter. An additional eight native oscillator checks reproduced the error in both policies, with and without an upper zone. That fallback now reads negotiated/defaulted manager sensitivity only when channel 1 actually has the lower-manager role; otherwise it uses its existing conventional parameter range. The disabled-zone range is therefore inert on that path as intended.

## Qualification

The initial manager implementation passed 224/224 before the disabled-zone controls were added. The expanded 232-check intermediate run failed exactly the eight disabled-zone controls. The final fixture additionally checks a nonzero released-member bend target and CC121 zeroing. It compares finite audio bits with an explicit identical-wheel replay reference for held, released and future notes, including already-active and newly activated sixteen-voice modes, both zones, signed bends, fractional ranges and zero-range recovery. Reset/mode/role controls verify that stale manager input is not reused.

The final fixture passed 258 checks with no failures. The affected Release native build completed 45 steps, with two existing enum-conditional warnings in the snapshot tool. The selected 25-test core/native integration run passed 25/25 in 104.88 seconds, including all six configured native VST3 rate/mode cases, conventional pitch range, MPE, controller, activation, allocation, state-mirror and undo suites. An independent repeat of the same selection passed 25/25 in 102.14 seconds, including manager retarget 258/0, conventional bend range 129/0, MCM 199/0, activation handoff 376/0 and allocation 204/0. Independent source and lifecycle review found no blocking issue. No full CPU target, worst-case deadline, external DAW/controller, complete MPE or universal offline-render qualification is claimed; the historical intermittent offline failures remain recorded.

The subsequent [held member range retargeting](MPE_MEMBER_RANGE_RETARGET.md) implements live sensitivity rescaling for eligible held member voices while preserving accepted-release targets. The earlier remaining-work statements above describe their original checkpoint.

The later [MCM membership reset](MPE_MEMBERSHIP_RESET.md) stops notes when channels enter or leave MPE control. The historical target-retention guarantees at this checkpoint do not exempt those later membership crossings; unchanged membership and still-MPE role-transfer boundaries remain distinguished.
