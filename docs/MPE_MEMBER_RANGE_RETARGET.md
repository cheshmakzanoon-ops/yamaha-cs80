# Held member bend after sensitivity changes

## Bounded behavior

An MPE member's retained normalized bend now rescales held voices when that zone's effective member sensitivity changes. This covers zone-wide RPN 0 (including fractional Data Entry), same-role MCM defaults and manual fallback edits before negotiation. The existing per-voice smoother follows the target. No new polling or per-sample work is added.

This is the member counterpart to [manager retargeting](MPE_MANAGER_RANGE_RETARGET.md), not a claim that the MPE specification prescribes an exact immediate-RPN recomputation instant. It makes held notes consistent with the already implemented future-note sensitivity behavior.

The existing role-tagged normalized cache is read without initializing it. Retargeting requires an active, held voice, a current member role, matching cached role, and a changed effective range. Changed-role caches are discarded first. An old role cannot be reconstituted by this operation. The current member role chooses the sensitivity; the voice's captured historical manager-zone routing remains unchanged.

Accepted-release targets, including pedal-held-only tails, remain fixed under the existing [released-target policy](MPE_RELEASED_TARGETS.md). Manager targets, pressure, timbre, independent matrix sources, note queues and smoother history are untouched. Full MCM note/controller reset remains separate work.

## Reproduction and qualification

The initial real-processor regression on `e70ca70a` failed 144 of 240 checks. The fixed initial fixture passed 240/240. Its finite-audio comparison uses an explicit identical member-wheel replay across Legacy/Circuit, lower/upper zones and eight/sixteen-voice modes. It covers zero-range recovery, fractional sensitivity, held/released/future notes and same-role MCM.

Review strengthened the final fixture to vary the member input sign itself and send it before extension activation, directly exercising inherited raw state. Additional target witnesses cover distinct same-zone peers, opposite-zone isolation, manual fallback versus negotiated sensitivity, CC121 zeroing, role invalidation, explicit pedal-held release state and accepted NoteOff ordering through direct core calls, not a host same-offset MIDI dispatch test. The final focused fixture passed 282/282 checks. The old onset test deliberately expecting a held note to retain its previous range was updated; released-target assertions were not relaxed.

The affected Release build completed 45 steps with two existing enum-conditional warnings in the snapshot tool. All 26 selected core/native integration tests passed in 102.39 seconds, including six VST3 rate/mode configurations, member/manager sensitivity, onset/release, conventional bend, tuning, controller, activation, allocation, mirror and undo cases. Independent source review approved the bounded correction, and an independent repeat passed 26/26 in 105.20 seconds: member retarget 282/0, manager retarget 258/0, MCM 199/0, activation handoff 376/0 and allocation 204/0. Both runs retain the original test criteria. This is not full MPE, external controller/DAW, CPU/deadline or universal offline-render qualification.

The later [MCM membership reset](MPE_MEMBERSHIP_RESET.md) stops notes when channels enter or leave MPE control. The historical target-retention guarantees at this checkpoint do not exempt those later membership crossings; unchanged membership and still-MPE role-transfer boundaries remain distinguished.
