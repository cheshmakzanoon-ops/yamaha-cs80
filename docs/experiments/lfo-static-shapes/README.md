# Static deterministic non-sine LFO qualification

Test-only qualification of the exact native module from published source
`244016b22e290a450707947b221f777f18e22c01`. No production/DSP/UI or historical
source/test/evidence changes; the roster remains 89.

## Verified results

- Static triangle, saw up, saw down and square, each with three isolated
  rate/depth pairs and all six queues together.
- 44.1/48/96 kHz, realtime/offline: 96 cases, 1,536 completed renders.
- 288 scalar/block comparisons, maximum absolute error 0.
- 96 reversed queue-order comparisons and 96 initial 1,024-frame prefixes byte-exact.
- Both held-window contribution witnesses positive in every case:
  rate 0.000237634–0.0152108; depth 0.00142631–0.036264.
- Finite complete audio buffers; Legacy flattening, PURE/destination-off inertness,
  empty/null queue controls passed. No positive-effect or audio tolerance changed.
- Both binaries match pre-run hashes afterward; all 5 source freezes and 630 retained
  source/test identities match. Capture hashes identify 384 files of 32,768 bytes.

One run completed with exit 0. No failed/interrupted test attempts were removed
or retried. The 384 captures are not cross-build comparisons. Summary, binary,
capture and evidence identity files are adjacent; only the fixture was compiled.

## Scope and edge coverage

The reduced matrix uses manual/manual 13/13 sources, 8-voice capacity with two
sounding notes. It does NOT extend the sine factory-source/16-voice coverage to
these shapes. All active LFOs use the same static shape; mixed shapes, random
waveforms and waveform switching remain excluded.

Before execution, design review identified that inherited slow rates could leave
square output constant in the held window at 96 kHz. Rates were therefore designed
within 16..20 Hz, with initial 18/19/20 Hz and 20/16/20/18 Hz endpoints. Confirmed
phase reset, target snap and convex smoothing place the first double-phase
half-cycle within 25..31.25 ms (allow sample rounding), inside held frames
[1024,3072) at every sample rate. This exercises square edges and saw wraps without
weakening gates. It is not all-phase/full-cycle coverage. Measured ablations
separately demonstrate both controls affect audio in the selected histories.

The scalar trajectory evaluator is independent of the block helper, but not an
independent DSP or analog oracle. No new synthesis law, DAW/GUI/AU, Windows/macOS,
CPU target, hardware fidelity, release or whole-project qualification is claimed.
Historical suites were not rerun; native CI remains separate.
