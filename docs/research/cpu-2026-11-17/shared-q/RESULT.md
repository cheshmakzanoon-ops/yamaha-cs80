# Shared-Q hoist: terminal measured rejection

One-shot GitHub Actions study 44044345534, attempt 1, commit 3028aa220b9c67d5565469b91b1c3fbb40114f9d, completed 2026-11-17 08:14:49 UTC. The green workflow means the experiment completed, not that the optimization passed. Production DSP is unchanged. No retry or promotion.

Fresh standard ubuntu-24.04 runner: Intel Xeon 6975P-C, GCC 13.3.0, child affinity CPU 0. Scoped callback-thread CPU workload: unchanged quick performance suite, 48 kHz/128 frames, 512 callbacks, 15 cases/36 rows. Not a DAW, whole-machine CPU, target-hardware or general stability certification.

Correctness: 131072 paired component readouts and 28672 invalid transactions pass full bits/MXCSR/x87/errno comparison. Paired full-engine qualification matches 19 baseline rows (3916800 PCM bytes) and 36 performance rows (11563008 PCM bytes). PCM SHA256 respectively 09180fb39546c6ff5d2d379b10fa546cdf7ff0fbb041d23506f0eca42f72d78a and 331fe05797c6a99925e09b54bfd1459d4b1653fde7710830f9afc6bdfd2e1c40. These compare arms on this compiler, not cross-compiler output identity.

All three predefined calibration pairs passed. Primary baseline/baseline ratios were 0.9950813923, 1.0196975747 and 1.0006949149. The second approaches the fixed 2% boundary; no threshold adjustment was made. All 36 row controls retained.

Six cost executions followed A/C, C/A, A/C, all completed. Geometric mean candidate/baseline primary ratio 1.0006723398: effectively no demonstrated gain (point estimate 0.067% more CPU), below the required 3% improvement. All-row ratio 0.9977755065; isolation held ratio 0.9964750552. Neither secondary point estimate substitutes for the failed primary gate. Decision REJECT_MATERIAL_GAIN_OR_REGRESSION_GATE. Static division reduction was not predictive of material engine CPU savings.

Artifact 14310853023 SHA256 297e4ed6f183dc1d33a9457bd9955e2b574c15714cce38030dae97959a95be6e verified against GitHub digest. Raw artifact, decision, execution ledger, command logs, calibration/cost CSVs and qualification PCM retained locally. Independent raw-receipt audit confirmed all hashes, 55 qualification rows, all 432 cost/calibration rows, affinity witnesses and exact execution order. Recomputed metrics agree. Promotion rejected; tiny differences do not establish meaningful gain or slowdown.
