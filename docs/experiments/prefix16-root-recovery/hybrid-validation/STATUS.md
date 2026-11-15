# Experimental safeguarded interpolation checkpoint

Base: c51120121490854bf088c88ebfd64fca83a6d289. Worktree audio-root-hybrid.
Production delta is only Ig00156RootFallback.h. No remote writes.

The Illinois-weighted secant proposal falls back to midpoint when invalid, and every third iteration bisects. Acceptance still requires actual finite residual <1e-7 and writes the result only on success. No bracket-width-only acceptance.

Initial g++ -O3 -ffp-contract=off checks:
- 768 captured equation fixtures: 0 failures.
- Production batch regression: 648358 checks, 2048 eligible cap-rejection positions, 0 failures.
- 512 warped captures: 3–27 additional equation evaluations, median15, counted tuples bit-identical to uninstrumented helper.
- Prior bisection implementation:27–37, median35.

No timing or whole-engine benefit established. Independent review underway. The192 iteration cap retains64 scheduled bisections but increases worst-case evaluation ceiling from82 to210 including bracket expansion. This tradeoff is unresolved; candidate is NOT accepted for publication.
