# Native static-random LFO qualification plan

Prepared against fetched published main `e71b2d7b687674308b2cd6bdb9f3a5c3998a69b5`.
The design below was frozen before the native run. Completed results are in
[the qualification report](experiments/lfo-native-random/README.md). No production or historical source/test file
changes. CMake adds only the new bounded target and six test registrations. The exact previously qualified module from published rate89
`244016b22e290a450707947b221f777f18e22c01` is pinned to SHA256
`59ed49c6b64d28847b7ebe83ec60811c819c06d827679a15a72992818c93de4f`.
Only the new fixture is compiled; the synthesizer module is not rebuilt.

## Ordinary-input matrix

Two static shapes: sample-and-hold and smooth random; three isolated LFO
rate/depth paths and all six queues together. Same static shape on all active
LFOs, pitch/filter/amplitude destinations, manual/manual 13/13 sources,
8-voice capacity with two sounding notes. 44.1/48/96 kHz realtime/offline gives
48 cases, 18 renders each = 864 renders. Four captures per case = 192 files,
not cross-build comparisons. 144 scalar/block and 48 reversed queue-order checks.

16,384 stereo frames per render: 0.3715193/0.3413333/0.1706667 seconds at
44.1/48/96 kHz respectively, not a quarter-second claim. Onsets at17/18;
releases at14336/14337. The shortest held duration is14319/96000 seconds,
more than2/16 seconds. Rates are bounded16–20 Hz; initial18/19/20 Hz,
with convex rate smoothing unchanged. Thus at least two accumulated cycles
occur before release, not an assertion of ideal-DSP phase or randomness quality.

Hold initial values through frame1023. Physical-rate knots at4095/8191/12287/
16383 are20/16/20/18 Hz, normalized through the existing parameter mapping.
Depth knots at3071/6143/10239/16383 are.3/.8/.25/.7 from initial.65.
Independent scalar history evaluation retains endpoint/midpoint self-checks;
block trajectories use the existing separate helper.

## Gates and retained boundaries

Finite full buffers, scalar versus64/127/256 blocks with existing audio
 tolerance, initial1024-frame byte-prefix, positive held-window[1024,14336)
rate and depth ablation effects at the unchanged1e-5 threshold, byte-identical
reverse queue ordering, Legacy flattening, PURE and destination-off inertness,
empty/null equality. A separate zero-depth comparison uses rate-only curves
versus constant-rate queues, never varying depth curves mislabeled zero-depth.

The new local ordinary ProcessData helper uses the existing Instance lifetime
and Instance.audio, scheduling only the two valid channel0 notes. It does not
modify historical Instance::process or remap process timestamps. Existing
supported-input compile guard excludes broad historical test bodies.

The script refuses existing build/evidence paths, checks642 historical source/
test identities and six reviewed source freezes, pins module and runner before
and after execution, and preserves complete logs and capture hashes. Any failed
or interrupted attempt must remain visible, not be silently retried or repaired
by weakening gates. Historical suites are not rerun.

No waveform queues or native switching, mixed waveforms, factory sources,
16-note behavior, low-rate/full-range or every-phase coverage, DAW/GUI/AU,
Windows/macOS, CPU target, hardware fidelity, release or whole-project claim.
This native protocol/history reference is not an independent DSP/randomness
oracle. Execute only after independent exact source/script review approval:
`python3 scripts/qualify-lfo-native-random.py`.
