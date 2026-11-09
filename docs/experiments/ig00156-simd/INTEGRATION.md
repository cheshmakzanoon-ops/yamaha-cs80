# Default-off main integration, 2026-11-09

This report is local integration evidence, not a new fidelity, timing, DAW or
release qualification. The optional path remains **OFF by default**.

## Exact source and merge scope

The fresh worktree starts from actual remote main
`68fcb884d49afb2a938dd703328529fca06f331d`, including its screenshot-only update.
The independently approved commits were cherry-picked without conflicts:

- `bf3bb816eecb9bfcd14922ea9cff462004289a1a` becomes
  `2f434ec56591e8d50aaf503746f4c5c748f9d7ed`.
- `b0b04abe1dbab19abc86a309787dad4a567f3137` becomes
  `f7292e3c8fa0d8d84894ab9ed25085c13d740e49`.

The tested implementation source is `f7292e3`. All affected production DSP,
focused-test and filter-workflow files are byte-identical to the reviewed
`b0b04abe` source. Only CMake needed automatic context merging; its pre-existing
main additions are retained. The subsequent changes in this worktree are
limited to this report/evidence, a report link, and correction of the historical
diagnostic source location. README/STATUS provenance updates are outside this
change set.

The original main build workflow is unchanged. All 68 committed screenshot PNGs,
their inventory/hash manifest and provenance are unchanged from `68fcb88`.
Independent hash verification maps the manifest's `artifacts/ui/<file>` names
to their committed `docs/screenshots/<file>` locations. Provenance still names
rendering source `c7595a4ea5171b4bf710e53c635938aa7d363e27`, run `42574049530`.
No screenshot was re-rendered or relabeled as evidence for this DSP integration.

## Fresh merged-source checks

- A fresh GCC Release configuration **omits** the batching option and records
  `CS80_ENABLE_AVX2_FILTER_BATCH:BOOL=OFF`; the core receives
  `CS80_DISABLE_AVX2_FILTER_BATCH=1`. The complete core-only default build succeeds.
- The complete default-OFF core-only CTest run passes **139/139 tests** in
  302.17 seconds. This includes the new focused tests plus the pre-existing
  source-factory, library-audit, controller, filter, memory and packaging-fixture
  tests; no plugin/GUI target is built by this configuration.
- An independent check archives actual main `68fcb88`, then builds GCC original,
  GCC omitted/default, GCC ON, Clang original and Clang ON. Each of the five
  Engine runs passes **4,135,998 checks / 285,328 stereo timeline frames**.
  Every 22,400,288-byte audio/public-continuation corpus in the tested matrix
  has SHA-256 `30dd6762c840465bfce5a0286cf2bf17b04ec38133424519a388f3c875e66557`.
  The required comparisons are original/current within each compiler; the
  observed cross-compiler agreement is not a general portability promise.
- GCC ON and Clang ON each pass their three additional focused CTests: stage,
  scheduling, and the compiler-admission matrix. GCC stage: **1,126,360 checks,
  15,685 accelerated groups**. Clang stage: **1,142,908 checks, zero accelerated
  groups**, including when ON was requested. Each scheduling run passes
  **869,465 checks**. Admission covers the 11 profiles and Clang mixed-backend /
  removed-guard negative controls described in the main report.
- Independent before/after hashes confirm all 166 source files and all 70 repository image
  files (68 documentation PNGs and two golden-test PNGs) were untouched while those runs executed.

Exact commands, logs, corpus/source hashes and the default cache/compile
settings are retained in `evidence/main-integration/`. The independent script
contains this machine's original local paths; it is a record, not a request to
overwrite those paths. Portable core-only checks from the repository root are:

```sh
cmake -S . -B /tmp/new-default-core -DCS80_BUILD_PLUGIN=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/new-default-core --parallel 4
ctest --test-dir /tmp/new-default-core --output-on-failure --parallel 4
# Deliberate qualification configuration only, not the shipping default:
cmake -S . -B /tmp/new-gcc-on -DCS80_BUILD_PLUGIN=OFF -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++ -DCS80_ENABLE_AVX2_FILTER_BATCH=ON
cmake --build /tmp/new-gcc-on --parallel 2 --target cs80_ig00156_batch_tests cs80_filter_batch_engine_tests cs80_filter_batch_scheduling_tests
ctest --test-dir /tmp/new-gcc-on --output-on-failure -R '^cs80_(ig00156_batch_tests|filter_batch_engine_tests|filter_batch_scheduling_tests|filter_batch_admission_tests)$'
```

## Scope retained

All Clang/AppleClang builds and unsupported CPU/math environments stay scalar.
This does not override production scalar arithmetic flags or default-enable an
optimization whose throughput remains workload-dependent. The implementation
still computes the existing discrete model; top-octave/continuous-model misses
are not fixed or reclassified. Historical timing, sanitizer and platform
results in the main report retain their original source/scope; no new timing
measurement, native plugin/GUI validation, external DAW test, or remote CI result
is claimed by this local integration run. Publication is a separate action.
