# Agent 31: retained full-bank and affected-test review

## Outcome

The retained final bank run passed **all 523 programs: 301 curated and 222 original circuit programs**. The separate affected-suite run passed **7/7 test executables**. Across these two local runs, **8/8 CTest executions passed**, with zero reported failures, skipped tests, or disabled tests. This review read existing evidence only; it did not rerun tests or modify production files.

## Full-bank measurements and unchanged limits

| Measurement | Recorded aggregate | Acceptance limit | Result |
|---|---:|---:|---|
| Maximum phrase peak | −4.6 dBFS | ≤ −1.0 dBFS | Pass |
| Loudest 50 ms window RMS, minimum | −29.8 dBFS | ≥ −30.0 dBFS | Pass |
| Loudest 50 ms window RMS, maximum | −18.0 dBFS | ≤ −12.0 dBFS | Pass |
| Maximum eight-voice stress peak | −1.5 dBFS | < 0.0 dBFS | Pass |
| Audibility RMS floor | Minimum −29.8 dBFS | ≥ −42.0 dBFS | Pass |
| Closest curated parameter-vector distance | 0.429 normalized L1 | ≥ 0.08 | Pass |

The closest curated pair is **Flute Organ 8+4 / Organ Vibrato**. Printed sound-level values are rounded to 0.1 dB. The test uses 44.1 kHz rendering; phrase checks use the category phrase or four-note chord at velocity 0.9/full pressure, while the stress render uses eight notes at full velocity/full pressure. The evidence contains aggregate extrema and the 523-program completion count; it does **not** retain 523 individual measurement rows.

`tests/LibraryAuditTests.cpp` is byte-identical between acceptance baseline `59d489d1a1f0065ae2a0f42a08c74edc43e94159` and reviewed source `e32728b31d97fc356cac291cb98935fdcd38f803`. Both have Git blob `04dd1abe2b1ede8271a26bdd858cccf53b0b7300` and SHA256 `41160b16166f47e960c5129792287e772bf39fa6f7b6bfcac54c7973a3080407`. The acceptance thresholds were not weakened.

## Affected test results

| Executable | CTest result | Duration, seconds | Retained stdout |
|---|---|---:|---|
| `cs80_instrument_body_tests` | Pass | 4.57339 | Truncated |
| `cs80_tine_strike_dynamics_tests` | Pass | 0.36924 | Truncated |
| `cs80_bowed_body_formant_tests` | Pass | 3.05499 | Truncated |
| `cs80_instrument_body_engine_tests` | Pass | 10.41320 | Complete |
| `cs80_preset_voicing_tests` | Pass | 15.43050 | Truncated |
| `cs80_preset_character_tests` | Pass | 5.91132 | Truncated |
| `cs80_preset_precision_tests` | Pass | 43.54240 | Truncated |

`cs80_instrument_body_engine_tests` explicitly records **18,156 internal checks, zero failures**. Six other JUnit stdout blocks contain CTest's output-removal notice at the 1024-byte threshold. Their pass results are recorded, but their final internal assertion totals cannot be recovered from these JUnit blocks.

The revised preset-voicing negative control keeps the **4 dB** acceptance gate. It sets both ranks' initial velocity levels to zero so that changing spectrum alone cannot satisfy the amplitude-dynamics criterion, retains a nonzero static level, and adds an audibility assertion in each of three preset cases. The source therefore adds three checks. **284** is an expected count derived from the task's earlier count of 281 plus these three additions; it is **not a directly observed final stdout count**.

## Source and run provenance

The parent identifies the tested audio source as `c4fe30987a5242305be60bd6cfddb144facd6643`. Its `src/core/InstrumentBody.h` SHA256 is `d33f34f5cff6341abc9bc2c85056ce381b492d141a0fad856e0c7cbd4f06422e`. The reviewed source `e32728b31d97fc356cac291cb98935fdcd38f803` has only one production source difference from that commit: the Wind description string in `src/core/curated/Fx.inc`. No numeric preset or DSP source changed in that interval. JUnit does not itself embed a source commit or binary hash, so this review distinguishes the parent-supplied build provenance from independently verified file and diff evidence.

Bank JUnit timestamp: `2026-11-23T11:48:48`; affected JUnit timestamp: `2026-11-23T11:45:57`. These raw timestamps contain no timezone indication. Bank test duration is 340.341 seconds.

## Evidence SHA256

All four files were read from `/workspace/scratch/dde4ad85ed5b/evidence/final/`.

| Evidence file | SHA256 |
|---|---|
| `bank-junit.xml` | `70d1b43758986fe80175919bea5c83a624c4d721649cd2961f910fad49416733` |
| `bank-tests.log` | `8ba1a196480ae9cea60239bcb8f78ab075b6efa4d9ce503a39f5776171e3b635` |
| `affected-junit.xml` | `9efaf69e6f04fbe2bc9536eb52d9ca2394972a3811d04f4fc703bd8d20a31c5f` |
| `affected-tests.log` | `4b85dbfe20a42a3fca3817ddf4bf4e4d474866fd0dd47bc33604b907a6bac1bd` |

These automated checks establish measured level, signal, and regression behavior within their tested scope. They do not alone prove acoustic identity with real instruments, subjective realism, native-host compatibility, or that downloadable release binaries contain the same source.
