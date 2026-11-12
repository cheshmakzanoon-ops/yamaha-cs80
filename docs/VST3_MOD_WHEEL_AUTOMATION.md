# MOD WHEEL depth automation: bounded Linux VST3 evidence

This candidate adds existing `P_modWheelSub` at lane 81 and `P_modWheelVcf` at
lane 82, total 83, preserving the complete literal 81-lane prefix. The sole
production edit appends these two entries. Parameter metadata/defaults, DSP,
continuous smoothing, UI/faders and historical native renderer are unchanged.
All 53 historical numerical contracts are preserved.

## What was tested

The reviewed script `scripts/qualify-mod-wheel-automation.py` completed once,
exit 0. Six full candidate processes and six separately compiled frozen81
compatibility processes covered 44.1/48/96 kHz in realtime/offline modes.

Each process covers SUB alone, VCF alone and both together, 8/16-note capacities,
and manual/manual 13/13 versus factory/manual 0/13 source layouts. Each render
contains 4096 stereo frames. A connected controller maps CC1 on bus 0/channel 1
and remains connected throughout rendering. Explicit CC1=1 at frame 0 is held
stationary; only depth-amount trajectories vary. No MIDI-controller timing
capability is inferred from this excitation.

The SUB oscillator is deterministic sine waveform 0 at 8 Hz with originalMode 0,
other SUB destinations/touch amounts and external modulation neutral. Initial
amount endpoints at frames 0/1023 preserve a 1024-frame hold before trajectories
at 1535/2047/3071/4095. The existing SUB pitch multiplier and VCF brilliance
multiplier provide separate destination witnesses, without new DSP laws.

## Observed results

- 72 full cases, 72 frozen81 compatibility cases; zero reported failures.
- 1080 candidate +576 baseline =1656 renders across 12 native processes.
- Scalar block 1 reference versus blocks 64/127/256: maximum absolute error 0.
- All 72 initial 1024-frame prefixes were byte-exact.
- All 72 held-window effects were positive, ranging 0.00551682–0.15359 in reported
 maximum absolute PCM difference over frames [1024,3072).
- Legacy sample-offset trajectories matched final-point-at-block-start flattening
 byte-for-byte. PURE amount trajectories were independently PCM-inert.
- Zero-CC1 amount automation versus fixed amounts was independently byte-exact.
- 432 actual PCM comparisons passed, from 864 capture files. These are pairs,
 not a claim that 864 captures are 864 comparisons.

Each scenario contributes six cross-build pairs: fixed no-controller, fixed
connected-zero, fixed connected-held, representative preceding 81 automation,
Legacy protected-policy trajectories, and PURE protected-policy trajectories.
The preceding 81 representative run drives volume/masterTune/tuningRef/rank1pan;
it is not a runtime rerun of every previous lane qualification.

## Identities and evidence

Frozen base: `edeaaf5f272380e8decfa997cac71d71cf876584`.
Candidate module SHA256:
`04adbf33704b1b053847d4e6f5532b9d75b0b960584f087f1a9864338b0f4182`.
Frozen81 module SHA256:
`f582e27e6fcbe360c55fe93b940ce10f699e619d91a458166bd5ba386b9a440f`.
Reviewed source-freeze SHA256:
`d77ac7e5be64b22e8062f1d232c8f2598a42e14e44e27d185bb8c147349777c2`.

See `experiments/mod-wheel-automation/summary.json`, `compatibility-pairs.json`,
`sha256.json`, retained-source inventory and twelve native logs. The baseline
runner was compiled inside isolated frozen81 source/header copies, not merely
a candidate 83 executable loading an older module. Logs attest compiled 81 versus
compiled 83; source and frozen-module identities were rechecked after execution.
Pair records accompany comparisons of actual bytes, not hash-only inference.

## Limits

This is bounded Linux VST3 software-protocol qualification, not hardware-fidelity,
CC1 event sample-accuracy, DAW/GUI, AU, Windows/macOS or CPU-target qualification.
The CC1 and TUNING REF fixtures received source-reviewed forward guards requiring
at least 81 lanes plus the exact literal 81 prefix; their old runtime suites were
not rerun. The historical tuning contract still declares 81, with lane 80 fixed.
No broad host/scanner/GUI/benchmark or malformed-input suite was invoked.
STATUS 81 remains its separate historical checkpoint. Independent evidence review verified the actual pairs and retained contracts.
Publication remains separate from these completed local results.
