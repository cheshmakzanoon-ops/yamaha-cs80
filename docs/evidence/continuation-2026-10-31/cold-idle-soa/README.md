# Cold idle clock qualification

2026-10-31. Numerical software evidence; no hardware measurements.

The original isolated qualification used audio commit
`bc41f23ea22ad96b58225749aa8048fa5099dd50`. The published integrated repair is
`46a7a5a2ce077e95b9025de81da85b3480134176`, separately pinned by
`6ab46dc8e1f75f4b559b279c54a254ace0480fbd`. A git comparison of `src/core`,
`src/experimental`, and the cold clock, cold filter, note-off and original
free-running test sources between those two audio commits has no differences.
Thus the original mutation and full-rate causal evidence is retained explicitly
as that qualification, not relabelled as a new run.

The integration was independently evaluated before the repair: 1,928 of
2,121 cold-clock checks failed (`integration-before.log`). The fresh integrated
CTest run then passed all 2,121 checks, with maximum elapsed error
8.00071120466e-12 seconds and phase error 6.82538964014e-7 cycles. The original
static and varying-control phase, wake-residual and phrase limits are unchanged.

`mutation-summary.json` and the three exact patches/logs retain the deliberately
restored elapsed stagger, unseeded first frequency, and stale reset-priming flag.
They fail 1,848, 1,056 and 1,056 checks respectively.

The phase probe, full-rate diagnostic patch and CSV/JSON files preserve the
original physical-PANEL Bass counterfactual. They are diagnostic variants only;
they are not production changes. Original retrigger and note-off scores above
one remain visible. Compatibility logs and the new three-round timing series
are added at their completed integration checkpoints.
