# Independent first-measurement review

**PASS for the preregistered median-cost gates, not hard-real-time qualification.**
No measurement was rerun. The separate standard-library
[recomputation script](../independent_measurement_audit.py) does not import the
primary analyzer. Its machine-readable findings are
[independent-audit.json](independent-audit.json).

Verified directly from compressed raw observations:

- All **73,728** positive-duration observations, exact chronological pair/
  callback/alternating-arm order; **144** unique matrix pairs and **288** rows.
- Every row's median (mean of positions127/128), nearest-rank p99 (position253),
  maximum and strict deadline-overrun count. Every primary-summary gate ratio,
  idle fraction and retained-overrun record agrees with independent arithmetic.
- All36 eight-active candidate/baseline median ratios:
  **1.028370794694061–1.1259106823647014**, each below the frozen **1.35** limit.
- All24 48kHz zero-audible candidate median real-time fractions:
  **0.2376645–0.2987049375**, each below the frozen **0.35** limit.
- **143 overruns remain:**46 baseline,97 candidate. Maximum real-time fraction
  **17.44651575 baseline /9.0533685 candidate**. The p99 exceeds the callback
  budget in5 baseline rows and8 candidate rows. None was excluded from any
  statistic. These observations do not establish worst-case execution time or
  certify a DAW deadline.

The published checkpoint `c2db96a317657ac214d7a1bd8d69c1af27d909bc` predates the
recorded measurement start. Its tree, frozen criteria SHA256
`6fe9119d43d3cbfa7ae0f414bad4bff382966e43625ffb8ddbc22259b7cbfb65`, and manifest
SHA256 `b3ff0c60495c18c68e12bfa026396fdb4d6a94d2a0315561824580529c866a94`
match. All40 manifest entries were independently checked: published/current
experiment sources, pristine428 files, actual preserved baseline/candidate
headers and source, and executable. The executable hash is
`1df35ab3f935065704cba17c62ae847e2e86b3bc433a29904b539ebe5528bf28`.
Compressed/uncompressed raw hashes and byte count match metadata; gzip time is0.
The review verifies retained provenance records, not an independent observation
of runtime CPU affinity or the host's scheduling state.

Cold/held candidate sample totals equal2 times warmup+measurement host frames.
Retired totals have the expected256-frame preparation granularity and the same
preparation prefix across block/control variants for each rate/scene. The frozen
probe checks the exact post-warmup delta at runtime; its before-counter is not
stored separately, so this review does not independently derive the retired
preparation duration. Effective model, active count, zero rejections and finite
checksums match all rows. Checksums demonstrate observable execution, not audio
parity: the enabled model intentionally differs from baseline.

Reproduce this **analysis only**, without executing the benchmark:

```sh
python3 docs/voice-continuity/engine-experiment/independent_measurement_audit.py \
  /tmp/cs80-engine-continuity-replay-first
```

The optional workspace argument verifies the preserved executable and candidate
tree; without it the script explicitly reports those checks as unavailable.
The analysis supports a separately reviewed opt-in design, not automatic
activation, analog accuracy, universal CPU behavior or production approval.
