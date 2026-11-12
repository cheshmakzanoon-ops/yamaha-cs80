# Native static-random LFO rate/depth qualification

Test-only qualification of the exact native module built from published rate89
`244016b22e290a450707947b221f777f18e22c01`, with new fixture source based on
published `e71b2d7b687674308b2cd6bdb9f3a5c3998a69b5`. No production changes;
roster remains 89. Historical source/tests/evidence remain unchanged. CMake adds
only the new fixture target and six bounded registrations; the qualification
script compiled the new fixture directly, not the synthesizer module.

## Verified results

- Static sample-and-hold and smooth-random, three isolated rate/depth paths and
  all six queues together, 44.1/48/96 kHz realtime/offline.
- 48 cases, 864 completed renders; 144 scalar/block comparisons, maximum error 0.
- 48 queue-order comparisons and 48 initial 1024-frame prefixes byte-identical.
- Positive held-window contributions in every case: rate 0.000553495–0.0208021,
  depth 0.00194452–0.0407704, above unchanged 1e-5 gates.
- All complete audio buffers finite. Legacy flattening, PURE/destination-off
  inertness, empty/null equality passed. Zero-depth uses separate rate-only
  automation versus constant-rate histories, not varying-depth queues.
- 192 captures, each 131,072 bytes, are not cross-build comparison pairs.
- Both native binaries match their pre-run hashes afterward; all six source
  freezes and 642 historical source/test identities remain unchanged.

The single compile/run completed with exit 0. No failed or interrupted attempt
was discarded or retried. `summary.json`, `binary-before.json`, `capture-hashes.json`
and `sha256.json` preserve counts and exact identities. The CMake registrations
were source-reviewed, not configured/executed in this direct-run qualification.

## Duration, matrix and limits

Each render has 16,384 stereo frames: about 0.3715193/0.3413333/0.1706667 seconds at
44.1/48/96 kHz, not a universal quarter-second duration. Ordinary channel 0 notes
start at 17/18 and release at 14336/14337. The minimum held duration 14319/96000 s
exceeds 2/16 s; rate histories stay 16–20 Hz with existing convex smoothing, so at
least two accumulated cycles occur before release. This does not claim ideal
phase, every-phase coverage or statistical randomness quality.

The reduced matrix is manual/manual 13/13, capacity 8 with two sounding notes,
same static random shape on all active LFOs. Pitch/filter/amplitude destinations
are fixed. The independent scalar trajectory evaluator is a protocol/history
reference, not a separate DSP/randomness oracle. The new local ProcessData
helper preserves true sample timestamps and does not alter the historical renderer.

No waveform queues or native switching, mixed waveforms, factory-source or
16-note coverage, full-range/low-rate accuracy, all destinations, DAW/GUI/AU,
Windows/macOS, CPU target, hardware fidelity, release or whole-project claim.
Historical suites were not rerun; native CI remains separate.
