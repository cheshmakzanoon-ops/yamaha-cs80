# Prefix16 bounded root recovery experiment

Based on experimental c51120121490854bf088c88ebfd64fca83a6d289, not a publication recommendation.
Only production change is Ig00156RootFallback.h: up to16 Illinois-weighted interpolation steps followed by64 bisections, strict actual residual <1e-7. Worst-case helper evaluations98 rather than82; retained initial192-step trial had ceiling210.

Independent generic/native/sanitizer768fixtures, generic/sanitizer648358batch checks,100000equation/guard probes pass. Fresh AVX2-enabled integration15/15 passes70.22s. Native/sanitizer trajectory432cases/1769472samples each show zero pre-first-baseline-cap mismatches and nonfinite states/outputs. Postrepair equivalence not claimed. Trajectory CAP rows after divergence are not replayable fixtures; see reviewer limitation.

Three alternating failure-heavy timing pairs independently audited: singlefailure scalar ratios0.648–0.853, wrapper0.698–0.884; fourfailure scalar0.847–0.915, wrapper0.880–0.972 relative prior bisection. Medians of512-group chunk averages, not whole-engine speedup, WCET or realtime guarantee. Whole-engine comparison complete: all six runs passed and independently audited; three SourcePerformance held ratios0.989510/1.002836/1.006596, release0.980855/0.992068/1.000455. No consistent whole-engine speedup.

Binaries retained outside repository and hashed; source, logs, raw measurements and review preserved here. Initial variant evidence retained as historical, not current header qualification. No remote writes.

Relative recovery: zero old-success/new-failure across100000deterministic cases each generic/native. Three/two new-only acceptances are outside physical nonnegative-coefficient domain; not a production-benefit claim. This sampled result is not a universal guarantee.
