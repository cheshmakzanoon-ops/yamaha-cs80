# Pitch conversion: no material CPU benefit established

Frozen GitHub Actions research run [43804904995](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/43804904995), attempt 1. **Do not promote this DSP candidate, retry this experiment, or lower its thresholds.** Production DSP remains unchanged. The user's below 2% CPU target is not established.

Both identical-binary calibrations passed this run's preregistered usability guards. All six active and four low-load baseline/candidate pairs then completed. The apparent differences were below the fixed material-benefit requirements; no frozen hard-loss or caution gate fired. Near-neutral results do not prove equivalence.

| Scope | Median candidate/baseline callback CPU | Per-pair range |
|---|---:|---:|
| Active total, all 36 rows |0.99828677|0.99652696–1.00055781|
| Active source-held aggregate |0.99676270|0.99330738–0.99970107|
| Low-load total, all 19 rows |0.99869663|0.99649097–1.00034228|
| Low-load isolation-held aggregate |0.99621954|0.99593245–0.99871364|

Active source-held means the median of nine source_performance warmed-held row ratios within each pair. Low-load isolation-held means the median of four nonidle isolation warmed-held row ratios. Total means the ratio of summed callback-thread CPU seconds over all phase rows, not a median of row ratios. Raw absolute costs, every case/phase, AB/BA strata and all tails/Legacy/isolation outcomes are retained in the analysis JSONs and CSVs.

Active calibration maximum aggregate deviation E=0.006338957644054233; low-load E=0.0034489245301458915. Required active improvement was at least 2% on both aggregates and strictly greater than 2E, plus repeated/order/no-loss requirements. Neither measured aggregate met the material floor. These are practical screening rules, not statistical precision estimates. No noise subtraction or favorable-case selection was used.

## Provenance and interpretation

- Workflow publication: c753c0c438c90a6c6e0151f9925edbb5d1a62d00, reviewed tree 7deefa39fe27a44229adfdf2f61af58677404fd3.
- Immutable production baseline: 5836041138bc1708c3fd2d4240cf63a3e1def4b3.
- Candidate: 18e1c1da25842b7bbacfc871067835db735d1230, reconstructed only in disposable build source using patch SHA 7d9c73919d31330f35c9aad10b21a86d56b86526044a1f75b6398c694a1fe1ec. It is not a main-branch DSP change.
- Hosted Ubuntu 24.04 x86-64, GCC 13.3.0; selected CPU 0 from allowed 0–3. Exact environment/compiler/flags/executable hashes are retained.
- Fresh native correctness, immutable-history comparison, source-path admission and matched build flags precede timing. This run measures the fixed core benchmark at 48 kHz / 128 frames, not a complete DAW, audio driver, GUI or the user's computer.
- Metric: callback-thread CPU seconds divided by rendered audio duration. It is not whole-machine CPU percentage. The candidate's prior ~0.5–0.6% whole-callback guest-instruction reduction did not establish a material native CPU benefit here.

## Reproduction lesson

A fresh hosted runner passed these calibration guards where the previous shared Dot environment did not. That is evidence about **this run**, not a guarantee that future GitHub-hosted runners are stable or a reason to reuse its calibration. Keep calibration fail-closed for separately authorized future experiments. Do not repeat this request to obtain a favorable result. No automatic source promotion exists.

The original downloaded artifact ZIP remains unchanged: SHA256 e56d1b971fa2c3182e7c8bfeca570032e49686f5e4ccd04154691f27d22d8379. `raw-text-evidence.zip` contains all 315 nonbinary receipts byte-for-byte, including every timing CSV. Extract it to a fresh directory to inspect the original raw layout. Six large qualification corpora are omitted from this docs copy; their sizes/hashes are in RAW-MANIFEST.json and the original Actions artifact. No executables are included. Independent raw audit passed; see INDEPENDENT-AUDIT.md. Executable hashes are trusted-runner receipts, not independently rehashed from the downloaded artifact because executable binaries were not uploaded.
