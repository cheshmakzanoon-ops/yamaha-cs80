# Native MSEG routing prerequisite

This bounded check establishes three ordinary valid-session MSEG routes before
adding sample-accurate rate automation. Production sources and the 89-lane roster
are unchanged. The retained release module is from the published LFO-rate milestone
`244016b22e290a450707947b221f777f18e22c01`.

## Verified

- Six sessions generated through public ModernLayerState/processor APIs, with six
  canonical configuration roundtrips; original serializer bytes loaded natively.
- Three unsynced, poly-triggered, always-looping bipolar MSEGs, respectively routed
  to pitch, cutoff, and level on both ranks. Fixed modulation amounts are 0.15.
- Fifteen finite stereo captures of 16,384 frames at 48 kHz realtime, block128,
  manual/manual source mode, two notes, plus fifteen exact native config readbacks.
- Held-note peak effects: pitch 0.0311674, cutoff 0.00786983, level 0.010888;
  all exceed the predeclared 0.00001 minimum. Zero amount equals no route
  byte-for-byte; PURE mode makes the configured route inert byte-for-byte.
- Retained source, candidate source, archive/module and helper binary identities
  verified before/after execution. See source freezes, binary-before.json,
  build-provenance.json, capture/state hashes and sha256.json.

## Limits

This is routing prerequisite evidence, not MSEG rate-automation qualification.
No rate lanes were added, no sample-rate/block-size matrix or old/new audio
compatibility comparison was run, and no native DAW, cross-platform, full-range,
malformed-state, performance or hardware-fidelity claim follows. CMake target
integration was source-reviewed only; the exact standalone build commands and
retained linkage are recorded in the build logs. No editor/window or display
workaround was used. Historical tests and evidence were retained, not rerun.

The qualifier requires fresh output/build directories and intentionally stops
rather than overwriting any prior attempt. PCM and native readback outputs remain
in the local build directory; their identities are recorded here. Canonical
serializer session files are retained in `canonical/`.
