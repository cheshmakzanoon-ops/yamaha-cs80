# Simultaneous MSEG rate histories

Test-only extension of the isolated 92-lane rate milestone. The retained native
module is byte-identical to that milestone; all production sources, prior test
contracts and historical evidence are unchanged.

## Verified scope

Two ordinary canonical sessions were generated and roundtripped through public
processor/ModernLayerState APIs: three routes enabled together, and no routes.
Unsynced poly-triggered, always-looping bipolar MSEGs target both-rank pitch,
cutoff and level at fixed 0.15 amounts. Each rate has independent interior
breakpoint offsets and values within the existing octave range. Histories use
manual/manual source mode, two held notes and 16,384 stereo frames.

Six configurations (44.1/48/96 kHz, realtime/offline) completed 114 finite renders:

- 24 scalar/block comparisons (blocks 7/64/256/1024), maximum error
  9.31323e-10, within the unchanged host contract.
- 24 forward/reversed parameter-queue comparisons, byte-identical.
- 18 independent per-lane motion witnesses, effects 0.0133371–0.0468734,
  above the predeclared 0.00001 minimum.
- Exact initial 1,024-frame holds and byte-inert unrouted, zero-amount and PURE
  controls in all six configurations.
- 12 native configuration readbacks and two canonical-session roundtrips.

All four runtime binary identities (archive, module, generator, runner), seven
candidate source hashes and 4,910 retained identities matched before/after.
Capture and state identities, build linkage provenance and logs are retained here.

## Limits

The 114 captures are identities, not 114 baseline comparisons. No old/new module
compatibility comparison was performed in this test-only extension. No source-mask,
sync, shape-switching, full-range, native DAW, cross-platform, CPU-target or
hardware-fidelity claim follows. Legacy behavior is covered only by the preceding
isolated-rate milestone, not rerun here. Historical tests were not rerun. New CMake
targets were source-reviewed only; recorded standalone helper builds were executed.
No production rebuild, broad/malformed-state suite, editor/window or display
workaround was used. PCM and native state outputs remain locally in the build
directory, with their hashes recorded here.
