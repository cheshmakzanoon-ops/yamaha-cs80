# Repeating an already-held ARP input key

Status: source prepared for independent review, not compiled or run.
Base `f01cbe84257a700a3690b4fac027fab002f46b56`. Production is unchanged.

## Existing playing contract

A repeated note-on with the same pitch and MIDI channel updates that ARP source's
velocity without creating a second held-key obligation or moving its played
order. The current generated note keeps its gate, voice and original velocity.
Rebuilding the sequence resets its next-step index to the beginning, but does
not reset elapsed clock time. Later generated occurrences use the new velocity.
One matching note-off removes the source. This describes existing software
policy, not a new phase-preserving/retrigger policy or physical CS-80 behavior.

`rememberHeldKey` updates the matched entry, preserves its source order and calls
`rebuildArpSequence`. The latter resets the sequence index without touching the
current gate/clock; mode 4 retains played order. `triggerArpNote` subsequently
uses each source entry's velocity/channel. Normal source-key release removes the
one matching entry. No private state is accessed by this fixture.

## Bounded fixture and frozen criteria

48 kHz host, Legacy policy explicitly selected, Eco 2x/96 kHz internal rate,
scanner disabled, 16 Hz ARP, one octave, **played-order mode 4**. Using played
order makes source-order retention observable rather than hidden by pitch sorting.
Both inputs use channel 1: first 60 at velocity .25, then 64 at .5. Drift,
dispersion and analog noise are disabled. Velocities are exact binary fractions.

Two fresh Engines share the initial chord. Only the candidate receives repeated
60 at velocity .75 before host frame 1000. Its current generated voice index,
pitch, channel and velocity must immediately remain unchanged. Both engines run
to frame 7000, with public held-note transitions inspected after each frame.
The candidate's complete transition timing must match the untouched reference
within **two host frames**, with the same count and held/gap pattern. The
candidate's three onsets must be 60,60,64: the repeated input resets sequence
index, not clock. The second 60 must use .75 and 64 must retain .5.

At frame 7000 one note-off for 60 must leave the current 64 intact. During 9000
more frames, only channel-1 note 64 at .5 may be held and at least two new 64
onsets must occur. Final release of 64 must leave no generated held notes
immediately or in the following 6000 frames. Ordinary release tails may remain
active; the test does not demand immediate audio silence.

**Ten assertions** cover fixed clock, positive pre-repeat witness, unchanged
current voice, preserved timing, exact three-onset order, source velocities,
noncurrent removal, no duplicate obligation, final stop, and finite/single-held
output. Timings, counts, exact values and two-frame tolerance are fixed before
execution. No adaptive alignment or tolerance changes are permitted.

This is a focused contract check using an untouched engine as timing control,
not independent clock validation, all ARP modes/channels, Source policy, MIDI
clock sync, parser/state/queue/scanner behavior, GUI or CPU qualification.

## Review-gated execution

CMake/CTest target `cs80_arp_repeat_lifecycle_tests` links only `cs80_core` and
has a 60-second timeout. After review, configure a fresh plugin-OFF/tests-ON
build, build only this target and its three core translation units, verify an
exact one-test JSON dry list using `^cs80_arp_repeat_lifecycle_tests$`, then run
that selection once. Freeze sources and this plan, retain logs, and do not run
broad core/host suites or retry failures silently. No execution is claimed here.
