# Variable-rate seeded-random LFO oracle results

Standalone core software-law qualification only. No production source changed;
this is not native-host automation, Engine smoothing, depth/routing or a randomness
quality test. Sample-and-hold and smooth-random use the existing Engine's three
nonzero seeds at internal sample rates 88.2/96/192/384 kHz.

## Verified results

- 24 cases, each 0.25 seconds with a fixed piecewise-linear 16–20 Hz input history.
- 1,140,300 samples compared to the independent reference, plus the same number
  replayed after reset and required byte-identical to the original sequence.
- All sample-and-hold values exactly match the independent seeded sequence,
  including wrap samples. Worst smooth-random absolute error: 1.49011611938477e-07, below
  the unchanged existing shape tolerance 1e-6.
- All cases completed four wraps; entire actual/reference sequences are finite
  and bounded. Different Engine seeds produce different sequences.
- Minimum reference distance from an integer-cycle boundary: 2.35023412031767e-07, above
  the predeclared 1e-10 clearance. No samples were skipped or edge gates loosened.
- The independent xorshift32 recurrence passed fixed seed-1 known-answer vectors.
- Runner binary hash is unchanged before/after; five source freezes and all
  634 retained source/test identities remain unchanged.

One compile/run completed with exit 0, with no failed or interrupted attempts.
Evidence retains hashes for 48 actual/reference sequence files and four exact
float rate-input arrays. These 52 files are not cross-build compatibility pairs.
`summary.json`, `run.log`, `binary-before.json`, `capture-hashes.json` and
`sha256.json` record exact counts and identities.

## Reference and limits

Reference phase integrates the actual float rate inputs cumulatively in long
double precision, without consulting production phase. A separately implemented
integer recurrence and upper-24-bit rational mapping generate the random stream.
Startup consumes one value for hold and two for smooth random; later wraps consume
one. The smooth reference evaluates the raised-cosine law independently before
float projection. This tests the declared software law, not analog authenticity.

Only these static shapes, seeds, input trajectory and rates are qualified. No
waveform switching, seed-zero fallback, statistical distribution, independence
quality, host behavior, Engine smoothing/depth/routing, GUI/DAW, cross-platform,
CPU target, hardware fidelity, release or whole-project claim is made. Historical
suites were not rerun. This was not a benchmark; elapsed time is not an outcome.

## Independent saved-data audit

`independent-capture-audit.py` reconstructs all 1,140,300 compared samples from
saved float rates with exact integer phase accumulation (rate quantum 2^-19 Hz)
and separate rational RNG arithmetic. Its retained JSON report confirms exact
sample-and-hold, the same maximum smooth error, zero saved-reference error, and
four wraps per case. It does not invoke the native executable; reset replay is
verified by the reviewed native fixture rather than repeated by this audit.
