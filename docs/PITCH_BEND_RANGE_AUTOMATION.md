# Live conventional pitch-bend range

With MPE off, the held MIDI pitch wheel is a normalized position independent of
the Pitch Bend Range parameter. Editing that range must immediately select the
new semitone target, just as receiving the same wheel event after the edit does.
Previously the engine retained the old semitone target until the next wheel event.

The range edit now retargets from the saved normalized wheel position. Legacy
applies the target immediately; the circuit engine retains its existing 5 ms
controller smoother, including when an edit occurs during an unfinished glide.
The modulation matrix's normalized Pitch bend source does not change with range.
Policy/quality resets continue to clear the wheel before any retargeting. MPE
negotiation, ranges, and mode-transition behavior are unchanged. This is a modern
MIDI control contract, not a claim about the original CS-80's circuitry.

`cs80_pitch_bend_range_tests` covers positive/negative/centered wheels, repeated
range edits including fractional values and both endpoints, edits during a glide,
startup/reset behavior, simultaneous policy/quality changes, and render partition
independence. It compares sample-for-sample audio with an explicit-wheel-event
reference and oscillator pitch with an independent time-domain smoothing oracle
at 44.1, 48, and 96 kHz, including the circuit engine's HQ oversampling mode.

Configure with `CS80_BUILD_PLUGIN=OFF`, build `cs80_pitch_bend_range_tests`, then
run `ctest --test-dir build-core -R '^cs80_pitch_bend_range_tests$' --output-on-failure`.
