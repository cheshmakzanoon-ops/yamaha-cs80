# A46 — supporting filter CI binding

**Accepted supporting result:** [Filter batch run 45244531626](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/45244531626), original attempt **1**, completed successfully. All six original jobs passed, with **24 passed CTest executions, zero failed and zero formal CTest skips** across four unique test definitions.

The actual executed source is **`bcc86aa39a33a0162254614e5502705f0113d701`**. The release source reviewed here is **`078c0981ffa1b5472342e0fb5ab8702bc3bd2889`**. These identities remain separate. The final API snapshot was retrieved on 23 November 2026 at 19:30 UTC.

## Original matrix results

| Job | Job ID | Recorded compiler | Cache mode | Passed CTests |
| --- | --- | --- | --- | ---: |
| clang++ / batch DEFAULT | `137937694490` | Clang 18.1.3 | ON | 4 / 4 |
| clang++ / batch OFF | `137937694796` | Clang 18.1.3 | OFF | 4 / 4 |
| clang++ / batch ON | `137937694856` | Clang 18.1.3 | ON | 4 / 4 |
| g++ / batch ON | `137937694947` | GNU 13.3.0 | ON | 4 / 4 |
| g++ / batch OFF | `137937694971` | GNU 13.3.0 | OFF | 4 / 4 |
| g++ / batch DEFAULT | `137937695222` | GNU 13.3.0 | ON | 4 / 4 |

For every job, the API binds the expected run and original attempt, confirms normal successful configure/build/test steps, and records a successful job conclusion. The authentic job log independently contains the exact checked-out source SHA, expected cache mode, each of the four named passing tests, and a single `100% tests passed, 0 tests failed out of 4` summary. The accepted run itself is also completed/success. No failed or canceled execution was substituted or reclassified; no rerun was requested.

## Source equivalence

All **91 relevant files** are byte-identical between the executed and release sources: every `src/core` file, `src/experimental/ContinuousVoiceCircuit.h`, `CMakeLists.txt`, the filter workflow, matching batch tests including the admission probe, and `tests/test_filter_batch_admission.py`. The complete per-file SHA-256 and Git-blob list is in [source-equivalence.json](a46/source-equivalence.json).

The two commits differ only in documentation/evidence and UI test fixtures. The corrected `src/tools/ui_snapshot.cpp` and `tests/PanelEditorTests.h` are outside this core-only build's executed targets. This is evidence for unchanged release inputs; it does not relabel the filter run as an execution of `078c0981`.

## Scope and interpretation

Each matrix job ran `cs80_ig00156_batch_tests`, `cs80_filter_batch_engine_tests`, `cs80_filter_batch_scheduling_tests`, and `cs80_filter_batch_admission_tests`. Counts are CTest executions across compiler/mode combinations, not internal assertions or six independent test definitions.

DEFAULT resolves to ON in the CMake cache. Clang ON/DEFAULT explicitly remains on the scalar path because only the supported GCC math contract admits the optional AVX2 backend. The matrix must not be called six AVX2 executions. Compiler-profile acceptance/rejection cases inside the admission harness are not counted as additional CTests.

This supporting workflow does not qualify native plug-in wrappers, installation archive bytes, a DAW workflow, or acoustic realism. The parent release review retains those independent requirements.

## Retained evidence

- [Compact qualification](a46/summary.json): source/run/attempt, original job and gate outcomes, actual test results, compiler/mode and every log's byte count/SHA-256.
- [Final run/job API snapshot](a46/final-api.json).
- [Source-input identity](a46/source-equivalence.json).
- Six original `a46/job-<job ID>.log` files.
- [Reproducible evidence summarizer](a46/summarize.py), which requires a successful exact-source run, six successful original jobs, all expected gate steps and all four named passing tests per log before writing acceptance.

No repository files, workflows, tests or Git state were changed by this reviewer. No new tests were run. Initial partial snapshots remain explicitly historical and are not used as final acceptance.
