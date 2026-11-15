# Frozen convergence captures, schema CS80_CAP_V1

Fresh baseline: `28999229df3662d8cf032e6dc38a0b12403aca06`.
Original `Ig00156Filter.h` SHA256:
`dd6ae57c9b3fed7817eeabc4e6f1128c41330a1ae7c183d73297a2fe5466e218`.
The companion frozen test header changes only namespace, dependency include paths and provenance comment.

The probe compared 432 reset-origin cases / 1,769,472 samples with zero output or named-memory bit differences between baseline and instrumented copies. These are the first 256 capped failures per clock, not an unbiased distribution. Clock 0 is unwarped 3 kHz; clocks 1 and 2 are warped 88.2 and 96 kHz. The audio recovery regression now requires all 768 retained states to repair, including both warped clocks; the frozen baseline still reproduces each original capped residual.

First line is `CS80_CAP_V1,768`. Each remaining line contains 50 fields:
- `CAP`, case ID, clock index, LP flag, pattern, sample index;
- rate, requested cutoff, Q, exact raw float input;
- previous Memory: z.input, z.feedback, z.bp, z.lp, v.input, v.feedback, v.bp, v.lp, slope1, slope2, halfIntegrators, halfCouplings;
- equation: zBp, c0, c1, g, ff, level, scaled input, inputScale;
- five successive (bp, hp, residual, Newton denominator) tuples.

Floating fields are hexadecimal. Patterns are deterministic noise in [-8,8], alternating +/-8, and tone followed by silence. These stress amplitudes follow the existing nodal tests; they are not hardware fidelity evidence. The truncated file intentionally contains only one capture and must be rejected with exit code 2. The missing-input negative test requires no file named `ig00156-cap-states-missing.csv`.

`FilterBatchCapTests.cpp` uses the unmodified production backend and exact named state/output comparisons with contraction disabled. It validates all 512 warped states in each lane position, multi-failure groups, duplicate-pointer fallback, unsupported admission atomicity and continuation. Backend unavailable/disabled returns 77, registered as an explicit CTest skip, not a passing SIMD qualification. Missing/truncated input is rejected before dispatch. Fifth-evaluation attribution is measured only by a separate diagnostic observer, not inferred from production success counts.
