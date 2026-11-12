# Simultaneous modern LFO rate/depth interaction qualification

Test-only qualification of the exact native module built from published source
`244016b22e290a450707947b221f777f18e22c01`. The roster stays at 89 lanes.
No production/DSP/UI source, metadata/defaults or old evidence changed.

## Completed results

- Six Linux VST3 configurations: 44.1/48/96 kHz, realtime and offline.
- 96 cases, 1,536 completed renders, 384 retained PCM captures.
- 288 scalar/block comparisons (blocks 64/127/256), maximum absolute error 0.
- 96 reversed queue-order comparisons were byte-identical at block 127.
- 96 initial 1,024-frame prefixes were byte-exact.
- Both independent contribution witnesses positive in all 96 held windows:
  rate contribution 0.000243351–0.152166; depth contribution 0.00171288–0.10888.
- Full render finiteness, destination-off/PURE inertness, Legacy block-start
  flattening and empty/null queue equality passed.
- Module and fixture binary hashes match before and after the run. Five source
  freeze identities and all 626 retained source/test identities remain unchanged.

This single run completed with exit 0; there were no failed or interrupted test
attempts to discard or retry. `capture-hashes.json` identifies all 384 captures,
each 32,768 bytes. They are NOT cross-build compatibility pairs. `summary.json`,
`binary-before.json` and `sha256.json` record counts and exact evidence identities.
The script compiled only the new fixture, not a new synthesizer module.

## Scope and interpretation

Three isolated rate/depth pairs plus all six queues together, fixed sine
waveforms and pitch/filter/amplitude destinations; two source layouts and
8/16 capacities (actual 2/16 notes). Rate and depth knots interleave after a
shared initial hold. Rates use the existing physical-Hz normalization.

The scalar reference independently evaluates piecewise-linear normalized curves
with endpoint/midpoint checks, while block queues retain the historical helper.
It is an independent protocol/history reference, NOT an independent DSP or
analog oracle. Rate-only and depth-only ablations separately witness both
contributions. Varying depth queues were not mislabeled as zero-depth controls.

Historical float sine phase remains unchanged. Returning to a rate does not
restore phase, and inactive modulation still advances phase. The 4,096-frame
histories do not establish full-cycle/all-phase accuracy, other waveforms, all
destinations, DAW/GUI, AU, Windows/macOS, CPU targets, hardware fidelity, release
or whole-project qualification. Historical suites were not rerun. Native CI is
separate. No cross-build compatibility result is inferred from unchanged code.
