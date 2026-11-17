# Strict Engine-only PGO: scoped standalone CPU gain

Frozen source/protocol commit: `58523b0982b3bdd5009a188f175db601eb886203`.
Single first-attempt GitHub study: [44061057792](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/44061057792).
Artifact: `14319700021`, SHA256 `aa75e27b8357d7ae1eb0ff1a637fdca345c88c1f854f1b9de1db19bfaa4bbd36` (GitHub artifact retention 14 days).

**SCOPED_COST_PASS**, not a production optimization release. GCC 13.3.0, Linux x86-64, Xeon 6975P-C, pinned CPU 0 on a free standard hosted runner. Engine.cpp alone received PGO flags; source, ordinary objects and safe math options remained unchanged. One frozen training pass preceded all held-out assessment. No retraining or retry.

## Correctness and controls

- Full held-out baseline/performance qualification: 151 rows, 32,436 render calls, 66,428,928 stereo float bytes per arm, exactly equal. Callback-boundary MXCSR/x87/errno transcripts exactly equal under the declared seeded environments.
- Five unchanged core regressions passed in both arms with identical logs. The existing FilterBatchEngine corpus was exactly equal: 22,400,288 bytes, SHA256 `ed310ea3b9dcef3c8e70b17d1530b6b55880ac6b7f0a55b11787493d3c233330`.
- All three fresh A/A pairs passed all aggregate and per-row calibration controls. Largest primary A/A deviation: 0.4006504%; declared gain threshold: strictly above 0.8013007%.
- Six calibration runs plus six paired-cost runs retained all 1,200 rows. Identical non-timing fields, no nonfinite output, callback CPU availability/count/denominator controls passed. No profiling runtime in assessment binaries; profile and binary hash guards passed.

## Measured result, this corpus only

Primary: the 24 source-performance warmed-held rows with initial selectors 2 through 9, at 44.1 kHz / 256 samples, 128 held callbacks. Candidate/baseline pair ratios were `0.9285637078`, `0.9280444175`, `0.9282642718`; geometric mean `0.9282907746`, or **7.1709% lower callback CPU time**. All three pairs improved.

Secondary geometric ratios: source-performance `0.9280716950`, all rows `0.9223263544`, isolation `0.9064899725`, legacy `0.9923396242`. All declared aggregate/per-row regression gates passed. This is measured callback CPU time, not an instruction-count proxy; the preregistered screening gate is not a statistical-significance claim.

## Independent audit

A separate reviewer recomputed all 1,200 raw calibration/cost rows and the frozen decision, verified PCM/FP/corpus hashes, training provenance/order, compiler/source receipts and affinity/symbol logs. No blocking discrepancy was found. Executable/object bytes were not retained in the artifact, so their recorded hashes cannot be independently recalculated; the runner enforced unchanged hashes during execution. See `independent-audit.json`.

## Limits and next decision

The training and held-out configurations/selectors differ, but share implementation and some scenarios. This is not universal generalization or a claim about older laptops, user hardware or DAWs. FP checks cover the stated boundaries/environments, not all possible inputs or every intermediate exception.

Production CMake builds position-independent core code; this standalone study does not. No production DSP/CMake/profile distribution was changed. A separate prospective PIC/plugin build-compatibility qualification is necessary before integration. Windows/MSVC, macOS/AppleClang and ARM remain unqualified. Do not reuse GNU profile data across incompatible compiler/source/target configurations or silently ignore a missing profile.
