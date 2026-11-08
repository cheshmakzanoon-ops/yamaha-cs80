# Voice release ownership

The modern unison modes allocate two, four or eight circuits for each note-on.
A live voice-mode edit applies to subsequent allocation; it must not change
which already allocated circuits belong to a note-off.

## Software contract

- Every direct note-on has a release group. All circuits actually allocated by
  a unison note-on share that group, including a partial group when fewer than
  the requested circuits are available.
- A matching note-off releases the newest held onset for that MIDI note and
  channel. It releases the entire group regardless of the current voice mode.
- Overlapping equal-pitch strikes retain separate groups. This extends the
  engine's existing newest-onset convention for single-voice notes to unison.
  Previously a note-off in unison mode released *all* matching held onsets.
- Normal sustain-pedal handling applies to each released group member. A mode
  edit cannot leave one member physically held after the key is released.
- Circuit reuse and reset establish fresh ownership. Per-circuit age remains
  separate and retains its existing modulation/retrigger meaning.
- The eight/sixteen selector is covered for one engine's ownership. The plugin's
  existing second-engine routing and teardown are unchanged.

These are MIDI/software lifecycle rules, not claims about Yamaha hardware.
Ordinary eight-voice allocation, DSP laws, envelope release timing, arpeggiator
sequencing and MPE negotiation are unchanged.

## Regression evidence

`cs80_voice_release_tests` covers every pair of the five voice-mode settings
under both Legacy and Circuit policies, with and without CC64 hold. It also
covers overlapping same-pitch onsets, wrong-channel note-off, partial unison,
circuit reuse, reset and completion of rendered release tails.

The previous implementation based note-off fan-out on the *current* mode:
leaving x2/x4/x8 unison could strand one/three/seven held circuits; entering
unison could release older overlapping strikes. The fix stores onset ownership
rather than guessing it from current controls. The change has no per-sample
work, allocation, lock or serialized-state-format change.
