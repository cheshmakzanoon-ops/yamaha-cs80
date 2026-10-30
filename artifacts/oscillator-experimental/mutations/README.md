# Mutation observations

Each temporary mutated header was compiled separately against the unchanged charge/timing, nominal pin and independent periodic complex-harmonic tests. Every listed mutation returned exit status 1 (expected failure):

- `restore_polarity`: replace reset charge derivative `+1/tau-f` with `-1/tau-f`.
- `shaper_knot`: replace the selected middle ordinate `.7071067811865475` with `.7371067811865475`.
- `omit_slope_event`: omit the `db[branch]*te` exponential slope-event correction.
- `independent_sine`: replace the shared fold/shaper voltage with `G*sin(2*pi*getPhase())`.

These are mutation observations for the experimental standalone core, not coverage of the not-yet-integrated Engine note/idle/legacy selection behaviors. Original temporary harness: `/tmp/recon_mutations/tests.cpp`; it selects `timingAndControls`, `profileAndPins`, and `independentNumerics` from the test source. No failed variant is selected by the product build.
