# Avoiding repeated source-route descriptor copies

The control-frame path stages a two-rank descriptor transaction when either
hardware selector or the source-model class of the render policy changes.
An already accepted route does not need to reconstruct that transaction.
Every frame still validates its model version, both selectors and all physical
parameters. An accepted frame still clears the previous interval readouts and
applies its parameters, including smoothing, policy resets and envelope
timing. Rejected frames still leave the published routing and readouts intact.
Changing one selector still compiles that rank when needed; PANEL/memory
remain separate paths, and Circuit to PURE still applies its policy reset.

With GCC 14.2 x86-64 Release/PIC, inspection of the unchanged and changed Engine
objects confirmed two 390-qword descriptor copies on the old steady path and a
branch around both copies on the new steady path. This eliminates 6,240 bytes
of logical descriptor copies per unchanged-route invocation in that build;
actual memory-system traffic is not asserted. Both copies remain available for
a route change. No public data layout, sound setting, note lifecycle, timing
or DSP fidelity knob was changed.

## Local check

- The focused Source Performance suite passed **496 checks**, including
  invalid and valid repeats, active readouts, non-route controls, PURE,
  one-rank changes, reprepare and PANEL re-entry.
- A separate source compatibility fixture matched **9,332,176 bytes** across
  four sample rates and seven factory selections (selected Engine readouts
  and PCM). Common SHA-256:
  `08a381f4ac9de1709a9c9197a78baefb12ffac93cd79f76d4b1e92d4f329e195`.
- The stock and candidate Linux VST3 used the same Release build commands.
  Rebuilding the unchanged Engine object and re-linking reproduced both the
  stock object and full stock plug-in byte-for-byte. The candidate changed
  only the Engine object. **227 native artifacts / 8,358,877 bytes** match.
  These cover held eight-note factory audio and state plus ring automation
  across 44.1, 48 and 96 kHz in realtime and offline modes, including
  eight/16-voice and legacy/PURE/source cases. This is focused Linux
  validation, not a claim that the entire cross-platform suite ran locally.

Setter-only timing on the cloud had favorable median ratios, but wide
variation, including a worse ratio of totals for an alternating-volume case.
The [native stock calibration](../../../tools/vst3-target-qualification/cloud-diagnostic/README.md)
also failed the predeclared small-effect gates. **This change removes confirmed
redundant copying; no whole-plug-in or DAW CPU saving has been measured.**
