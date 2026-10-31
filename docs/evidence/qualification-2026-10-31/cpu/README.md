# Fresh serial CPU qualification

These are software timings on the shared cloud executor, not measurements of a
CS-80 or a real-DAW qualification. All checkpoints use the unchanged
`cs80_source_factory_benchmark` sweep: 48 kHz, 256-frame callbacks, 1,024 callbacks
per measured phase, three complete rounds, 27 cases and 51 result rows per round.
No `--quick` option is used: that option changes the callback size.

`main-before` is the starting main revision
`db24a47170610f81236e8829ef5536af69dfc94d`. `inherited-before` is the reviewed
completion work at `2ee33dcc8113aaca4141dc7d77fb374384d8560c`, before the new
qualification optimizations. The latter includes the independently qualified
filter phase correction and electrical programmer. Those changes have a real
cost, so it would be misleading to treat the inherited tree as the old main
baseline or to combine timings from different revisions.

The six initial rounds were run serially after the main core suite completed.
Project builds, tests and agent compilation were paused for the entire timing
window. External contention on this shared executor remains uncontrolled.

The reporting convention follows the existing completion evidence: select the
minimum callback median across the three rounds **for each fixture**. Report
the p99 and deadline misses from that same selected round, rather than mixing
independently favourable statistics. Every raw round remains available.
`scripts/summarize-cpu-benchmarks.py` checks fixture completeness and produces
the per-row summary and raw-file hashes; it does not enforce or alter a DSP
tolerance. Held ranges cover all eleven paired-tone rows in the Pure CS-80
(`isolation`) policy. The moving-controls and quiet-idle rows are reported
separately.

| Checkpoint | Held median range, realtime factor | Held p99 range | Quiet idle | Moving controls |
| --- | ---: | ---: | ---: | ---: |
| Starting main, three rounds | 0.612398–0.720204 | 1.105197–1.638138 | 0.132888 | 0.732507 |
| Reviewed completion tree, three rounds | 0.664192–0.952562 | 1.137207–1.767049 | 0.098337 | 0.956260 |
| Exact lazy filter histories `476c2f1`, three rounds | 0.647884–0.926200 | 1.136551–1.751327 | 0.100304 | 0.927262 |

The original targets remain 0.15–0.20 held realtime factor and at most 0.02
quiet-idle realtime factor. Neither initial checkpoint meets them. Subsequent
checkpoint directories and their summaries record each isolated optimization;
an exact-output test is not a claim of a measured whole-engine speedup.

The lazy-history candidate improves the held and moving medians modestly, while
quiet idle is slightly higher in this shared-host sample. It does not meet either
target. Its selected held rounds contain 1,133 deadline exceedances in total;
the moving row contains 322 of 1,024 callbacks. All 51 fixtures in all three rounds
have zero nonfinite outputs. The complete distributions are retained.
