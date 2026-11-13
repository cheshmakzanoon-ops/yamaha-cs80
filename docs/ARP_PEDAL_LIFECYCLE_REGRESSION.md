# ARP source-key release with MIDI sustain

Status: independently source-reviewed, then built and run once on Linux with
scoped Release/Ninja CMake and CTest, 2026-11-13: **1/1 test, 24/24 assertions
passed**, exit 0. See [complete output](experiments/arp-pedal-lifecycle/test.log),
[identities and commands](experiments/arp-pedal-lifecycle/result.json), and the
[hash-frozen pre-run plan](experiments/arp-pedal-lifecycle/pre-run-plan.md).
Base: `a7ead295` (the preceding bounded ARP rate regression). No production changes.

## Playing contract

**CC64 does not latch ARP input keys.** Releasing an input key removes it from
the pattern even with the MIDI sustain pedal down. If that key supplied the
currently generated note, its held gate ends immediately, including when the
generated note is octave-transposed. Releasing another input key leaves the
current gate running. Remaining keys continue the pattern; releasing the final
key stops further generated notes. Raising the pedal does not revive removed keys.

This is the existing software ARP policy, not a hardware CS-80 behavior claim.
An ended held gate can leave an ordinary audible envelope release tail. That is
not the same as a `pedalHeld` voice or a latched pattern. The test does not demand
instant silence or zero active release voices. Ordinary non-ARP sustain behavior
is unchanged and is not qualified by this fixture.

Source grounding: ARP `applyNoteOff` calls `forgetHeldKey`, which uses the
source-key order to stop the current generated gate and rebuilds remaining input
keys. `stopArpVoice` calls `releaseVoice`, not the normal `releaseKey` pedal path.
Final source removal resets the ARP clock. No private state is accessed here.

## Predeclared bounded fixture

`tests/ArpPedalLifecycleTests.cpp` uses fresh Engine instances, explicit Legacy
policy, 48 kHz host / fixed Eco 2x (96 kHz internal), scan timing off, ascending
16 Hz ARP with two octaves. Two held source keys, 60 and 64, use channel 1 and
velocity 0.7. CC64 is set down before note-on. Dispersion/drift/noise are disabled.
Three cases each have eight assertions (24 total):

1. After 1,000 host frames, generated root 60 is positively verified; release
   source 60 and require its held gate to end immediately.
2. After 6,500 host frames, generated octave 72 is positively verified; release
   source 60 and require its held gate to end immediately.
3. After 1,000 host frames, verify generated 60; release the other source 64 and
   require current held 60 to remain immediately unchanged.

After the first release, inspect every host frame for 12,000 frames: only the
remaining source and its octave may be held, and both pitches must actually
occur. Release that final source, then inspect 12,000 frames with CC64 still
down, requiring no held generated notes. Finally raise CC64 and inspect another
12,000 frames, requiring no revival. All audio must remain finite; there may be
at most one generated held voice and no `pedalHeld` voice throughout the history.
Public getters also confirm the fixed internal rate.

The edit times are deliberately inside known open gates, not on timing edges.
No clock-frame tolerance or numerical audio threshold is needed: checks concern
exact public note identities and Boolean ownership, with finite audio only.
No alignment, threshold adjustment or failure retry is permitted after the run.

This does not test Source policy, MPE, arbitrary channel layouts, scan timing,
parser/state/queue admission, all ARP modes, MIDI clock, hardware fidelity,
GUI, CPU performance or general sustain semantics. It does not add an ARP latch.

## Review-gated reproduction

The exact CMake/CTest target is `cs80_arp_pedal_lifecycle_tests`, timeout 60 s.
Configure a fresh build directory with plugin OFF/tests ON, build only this
target (which compiles the three `cs80_core` translation units), verify the JSON
dry list contains exactly this test with anchored regex
`^cs80_arp_pedal_lifecycle_tests$`, then run that selection once. No broad core,
TuningMidiTests or host suite is requested. Freeze source/plan identities before
execution and retain all logs. This run verified exactly one dry-listed test,
zero CTest prerequisites, four zero-exit stages and unchanged source hashes.
Only the fixture and linked three-TU core library were built. No historical
evidence was changed and no broader test suite was run.
