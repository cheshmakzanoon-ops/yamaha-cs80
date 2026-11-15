# Relative recovery probe

Same deterministic 100,000 coefficient tuples used by the edge probe, now evaluated against both frozen committed bisection helper and prefix16 helper. Original helper copied with namespace/include adaptation only. Full differing records retained as exact hex floats in logs. Generic uses -O3 -ffp-contract=off; native uses -O3 -march=native. Actual commands and source/binary hashes preserved. Both runs exited 0.

Generic: old-only 0, new-only 3, both 81,242, neither 18,755.
Native: old-only 0, new-only 2, both 81,238, neither 18,760.

No observed old-success/new-failure in this probe. This is sampled evidence, not a universal recoverability guarantee. All differing records have negative c1, outside the nonnegative physical-coefficient monotonicity argument; one generic-only recovery also has maximum finite g. Do not market these as meaningful production recovery improvements. Every new accepted tuple independently satisfies finite consistency and strict residual <1e-7, and every new rejection preserves the caller sentinel. Native/generic arithmetic results are intentionally reported separately.

No production edits, timing or hardware claims.
