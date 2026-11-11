# Filter production comparison evidence, 2026-11-11

Base checkout: `34a8edc9ff2dd6458725f2ee46bb57c30da044ff` plus this change.
Core-only Release build with GCC 14.2.0; plugin disabled. No runtime DSP or
standalone reference source changed. No remote publication is part of this
record.

- `comparison-tests.txt`: all 24 new tests passed.
- `focused-ctest.txt`: all six selected CTest targets passed, 24.09 seconds.
- `independent-ctest.txt` and `.exit`: independent rerun of the same six targets,
  all passed, 25.28 seconds, explicit process exit 0.
- `summary.json`: exact event fixture, requested and actual parameters, metrics,
  source identities, and manifests/artifact hash inventories for four frozen
  comparison bundles. The independent review recomputed every metric from all
  801 rows and verified all 20 artifact hashes in each bundle.

The six targets are the filter and amplitude production comparisons, their
standalone references, and the reference chain and chain-clock tests. This is
not the full repository suite, a native-plugin qualification, or hardware
accuracy evidence. The direct native API oracle validates export plumbing;
independent signed power/log controls validate the stated analytic segments and
boundary behavior separately.

All four example runs use 8 kHz, 801 endpoints, A=.0137, D=.02, R=.03, and the
same four events recorded in `summary.json`. Run commands and reproduction
requirements are in [the comparison guide](../../FILTER_PRODUCTION_COMPARISON.md).
The IL/AL pairs are:

- `signed_up`: -.7 / 1.3. Maximum absolute difference .02225741741624221,
  RMS .003867153014671933, 1 stage mismatch.
- `signed_down`: 1.3 / -.7. Maximum absolute difference .01198477697904854,
  RMS .0020822582714229046, 0 stage mismatches.
- `zero_span`: 0 / 0. Zero level error and 6 stage mismatches; relative RMS is
  null because reference RMS is zero.
- `equal_nonzero`: -2 / -2. Maximum absolute difference .08450952976551762,
  RMS .011201745712168483, 3 stage mismatches.

These differences are descriptive, not pass/fail tolerances. In particular,
continuous zero-span completion and discrete production stage transitions do
not agree even when levels are identical. Native parameter rounding and
per-tick phase-boundary handling are retained rather than compensated.

The complete local frozen bundles are retained outside the source checkout at
`filter-comparison-evidence-20261111/{signed_up,signed_down,zero_span,equal_nonzero}`
beside it. Their manifests and hashes are summarized here; binaries and full
CSV snapshots are not duplicated into Git. The hashes identify observed bytes,
not authenticated builds or bundled compiler/Python/libm runtimes.
