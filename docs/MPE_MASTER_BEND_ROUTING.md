# MPE master pitch-bend routing

## Defect

With MPE enabled and partial zones configured, a lower-zone master pitch bend
changed the pitch of sounding notes on channels outside both zones. The render
path selected the lower master bend for every voice not captured as an upper-zone
voice. Those gap channels are conventional channels, not implicit lower members.

## Correction and compatibility

The render path now suppresses the master-bend contribution for an unassigned
channel while MPE is enabled. The own-channel member-bend contribution is unchanged.
The following existing behavior is deliberately retained:

- Channel 1's own-channel global-bend fallback when no lower zone is configured.
- Master/member addition inside an assigned lower or upper zone.
- Captured-upper routing while a voice remains assigned to either zone. This is
  not a change to live zone retagging.
- Ordinary non-MPE global bend, PURE CS-80 isolation, control smoothing, ranges,
  event handling and initial-note controller behavior.

The eligibility check runs once per rendered voice, outside the two-rank loop.
It adds no allocation, locks, host parameters or controller state. This is a MIDI
routing correction in the optional MPE layer, not a hardware-fidelity claim.

## Regression evidence

`MpeMasterBendRoutingTests.cpp` tests post-onset wheel gestures across all 16
channels, seven zone layouts, Legacy and Circuit engines, both oscillator ranks,
own-channel bend addition, held notes and release tails, MPE off/on, and a member
removed from its zone. It also preserves the existing assigned-voice captured-upper
policy, ordinary global bend and PURE isolation. A full upper zone correctly
includes channel 1 as a member, not a lower master.

The 1,152-check suite has 424 failures against unchanged `f757909` and zero with
the routing correction. Every pitch-bend event in the tests follows note
allocation. Pre-note expression retention is outside this change.
