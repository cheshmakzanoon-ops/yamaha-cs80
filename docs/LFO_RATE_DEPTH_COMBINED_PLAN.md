# Simultaneous modern LFO rate/depth qualification design

This reviewed design has completed its bounded run. See the
[verified results and limits](experiments/lfo-rate-depth-combined/README.md).

Test-only checkpoint on verified published89 commit
244016b22e290a450707947b221f777f18e22c01. No new lanes, production code, DSP,
UI, parameter metadata/defaults or historical evidence changes.

Three isolated rate/depth pairs plus all six queues together use fixed sine
waveforms and pitch/filter/amplitude destinations. Retain source pairs13/13 and
0/13, capacities8/16(actual2/16notes),44.1/48/96kHz and realtime/offline:96cases.
4096 stereo frames each. Hold initial rate8/11/14Hz and active depth0.65 through
frame1023. Rates use16/4/18/7Hz at1535/2047/3071/4095; depths use0.3/0.8/0.25/0.7
at1279/1791/2815/4095, making knots interleave. Normalize physical Hz with the
existing helper; inactive depth/destination remains zero.

A separate piecewise-linear normalized evaluator drives the scalar reference;
validate known endpoints/midpoints. The block path keeps the historical bounded
renderer and absoluteValue helper. This is an independent protocol/history
reference, not a DSP/analog oracle; preserve the historical float sine phase.

Require finite audio, scalar vs64/127/256 tolerance, exact initial1024frames,
positive held differences from rate-only AND depth-only ablations, destination-off
and PURE inertness, Legacy final-point block-start flattening, exact block127
comparison with reversed queue insertion order, and empty/null queue equality.
Do not label varying-depth queues zero-depth controls: they re-enable modulation.

Use the exact published module with before/after binary/source hashes. No
same-source cross-build compatibility claim. Capture neutral,Legacy,PURE,scalar
combined with hashes:16renders/case=1536renders,384captures,288scalar/block
comparisons and96queue-order checks. Captures are not cross-build PCM pairs.

Independent design review recommended this scope. Exact fixture/contract/script
must pass independent source review before execution, then final evidence audit.
No malformed inputs,broad host/scanner suites,GUI,benchmark replay,sanitizer
workarounds. Exclude other waveforms/all destinations/all-phase/full-cycle
accuracy,DAW,cross-platform,CPU,hardware fidelity and release qualification.
