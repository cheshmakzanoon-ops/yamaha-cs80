# A47 — Completed supporting filter CI qualification

**Supporting filter gates passed: all six original jobs and all 24 actual CTest executions.** This is acceptance of the filter workflow's focused scope. Whole-release qualification remains outside this review, and the machine-readable result retains `release_qualified: false`.

The [original run 45344888479, attempt 1](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/45344888479) executed `09af5358d34a7ea431de7e8a42e35dae66e52159`. Final authenticated API acquisition at **2026-11-24 08:15:10 UTC** confirms the run and all six jobs concluded successfully. The complete original logs for all jobs are retained. No rerun, local replacement execution or reconstructed passing log was used.

The final native-build candidate compared here is `c76ad288e02fbeca48f39652bf4fcdd99837f793`, tree `416c1f6bfb4cd9f7dd5dbd9f5cbc211edda2bf00`. The observations remain labeled with their actual `09af535` execution source; they are not represented as a fresh execution of `c76ad28`.

## Actual six-job matrix

| Original job | Observed compiler | Requested mode | Actual cache value | Configured core backend | Actual CTests |
|---|---|---|---|---|---|
| [138276946182](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/45344888479/job/138276946182) | GNU 13.3.0 | OFF | OFF | Scalar | 4 passed |
| [138276946520](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/45344888479/job/138276946520) | Clang 18.1.3 | OFF | OFF | Scalar | 4 passed |
| [138276946524](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/45344888479/job/138276946524) | Clang 18.1.3 | DEFAULT | ON | Explicit scalar fallback | 4 passed |
| [138276946543](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/45344888479/job/138276946543) | Clang 18.1.3 | ON | ON | Explicit scalar fallback | 4 passed |
| [138276946568](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/45344888479/job/138276946568) | GNU 13.3.0 | ON | ON | GCC exact-math runtime dispatch; CPU-dependent AVX2 or scalar | 4 passed |
| [138276946630](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/45344888479/job/138276946630) | GNU 13.3.0 | DEFAULT | ON | GCC exact-math runtime dispatch; CPU-dependent AVX2 or scalar | 4 passed |

Each original job contains exactly four real passing CTest result lines and one terminal `100% tests passed, 0 tests failed out of 4` summary. Across the six jobs this establishes **24 executions passed, 0 failed, and 0 formally skipped CTests**. Those executions cover **4 unique test definitions**, not 24 different tests:

| CTest definition | Source reviewed | Scope |
|---|---|---|
| `cs80_ig00156_batch_tests` | `tests/Ig00156BatchTests.cpp` | Scalar/batch arithmetic and state comparisons, admission/fallback edges, floating-point environment behavior and CPU-dispatch conditions |
| `cs80_filter_batch_engine_tests` | `tests/FilterBatchEngineTests.cpp` | Engine output/state comparisons under runtime batch toggles, voice/control changes, sample rates and filter bypass combinations |
| `cs80_filter_batch_scheduling_tests` | `tests/FilterBatchSchedulingTests.cpp` | Scalar/batch and block-partition comparisons through voice-count transitions, release/idle/wake behavior and modern routing |
| `cs80_filter_batch_admission_tests` | `tests/test_filter_batch_admission.py` and `tests/Ig00156BatchAdmissionProbe.cpp` | Independently compiled math-profile admission probes, including unsupported caller/backend combinations |

These results belong to this supporting filter matrix and must not be added to native plugin test totals as if they were executions in those other runs.

## Original command and configuration evidence

For each job, the unchanged inspector validates the original checkout SHA, actual compiler identification, matrix environment, printed cache value and explicit Clang fallback message where applicable. It also validates that the original API reports successful completion of all three gate steps and that the corresponding timestamped command groups occur within those steps:

1. **Configure core only:** a fresh-cache check followed by the actual Release CMake command with `CS80_BUILD_PLUGIN=OFF`. DEFAULT omits an option override, and the direct cache check confirms ON; explicit ON and OFF are verified against their requested values.
2. **Build focused qualification tests:** the direct build command targets `cs80_ig00156_batch_tests`, `cs80_filter_batch_engine_tests` and `cs80_filter_batch_scheduling_tests`.
3. **Exactness and scheduling gates:** the direct `ctest --test-dir build-filter-batch --output-on-failure` command uses the workflow's exact four-test selection.

All three original shell groups run under `bash -e`. The workflow has no retry or `continue-on-error` policy. Counts come from actual CTest result lines, not matrix size alone. Full commands and output remain available in the original job logs; a complete `CMakeCache.txt` was not separately uploaded.

## Exact input equivalence to the final candidate

[source-equivalence-c76ad28.json](../ci/source-09af535/filter/source-equivalence-c76ad28.json) establishes exact bytes, Git blob identity and file mode equality for **106 relevant repository inputs** at `09af535` and `c76ad28`. This covers all core files, the continuous-circuit header, complete CMake definition, filter workflow, test entrypoints, independent admission probe and their transitive quoted repository includes. It also covers **all 18 repository files hashed during core-only CMake configuration**, including provenance inputs for offline targets that this workflow does not build. There are no unresolved quoted repository includes.

The complete commit delta comprises these three paths:

- `docs/INSTALL_AND_QUICKSTART.md`
- `docs/PRESET_GUIDE.md`
- `tests/test_parity_capture.py`

None is consumed by the four selected filter tests or the recorded configure-time hash operations. The parity-capture test changed between the earlier `db6b5a7` candidate and `c76ad28`; it is outside this filter selection and the 106-input dependency set. Other test suites can consume these changed files, and their qualification remains separate. These paths also fall outside this filter workflow's push path selection.

This comparison supports applying the observed filter behavior to unchanged relevant source inputs. It does not claim that compiler binaries, runner images or external action implementations were rerun at the candidate commit.

## Precise backend and observation limits

OFF selects scalar processing for the configured core on both compilers. Clang ON and DEFAULT explicitly remain scalar. GCC ON and DEFAULT admit a qualified exact-math backend, but runtime AVX2 selection depends on CPU capability. The test source requires actual acceleration when a qualified build reports AVX2 support; the successful logs do not expose an accelerated-group total.

The backend labels above apply to `cs80_core` and its three linked C++ tests. The Python admission CTest compiles its own strict, unsafe and mixed compiler profiles without inheriting the CMake OFF macro. Successful `ctest --output-on-failure` output suppresses normal probe stdout. Therefore **actual accelerated-group counts and internal admission-profile skip counts remain unknown**, despite zero formally skipped CTests. The summary records these unknown values as `null`.

The workflow uploads no artifacts, and the final artifacts API reports `total_count: 0`. Original API bodies and full job logs supply the execution evidence. SHA256 digests protect the integrity of the retained copies; they are not GitHub signatures. This focused matrix does not establish native plugin packaging, artifact identity, DAW compatibility, performance gains or acoustic realism.

## Final records and checkpoint preservation

- [acceptance-summary.json](../ci/source-09af535/filter/acceptance-summary.json): derived final result with `supporting_filter_gates_passed: true`, all six actual jobs, 24 passing executions, original command observations and `release_qualified: false`.
- [acquisition-final.json](../ci/source-09af535/filter/acquisition-final.json): final retrieval provenance and distinction between previously retained and newly acquired logs.
- [run-final-api.json](../ci/source-09af535/filter/run-final-api.json), [jobs-final-api.json](../ci/source-09af535/filter/jobs-final-api.json) and [artifacts-final-api.json](../ci/source-09af535/filter/artifacts-final-api.json): unchanged final API bodies.
- Six `job-<id>.log` files in the same evidence folder: full original logs for the job IDs listed above.
- [a47-inspect-filter-ci.py](../ci/source-09af535/filter/a47-inspect-filter-ci.py): the unchanged checkpoint inspector, run against all final records and the final candidate source.
- [manifest-final.sha256](../ci/source-09af535/filter/manifest-final.sha256): retained-file integrity manifest including both checkpoint and final records.

All **17 files** named by the original checkpoint manifest, including `reviews/a47.md`, the pending acceptance record, the two earlier logs and the inspector, still verify against their original hashes. The earlier pending records accurately describe their earlier observation time and were preserved without alteration. This final review adds later completed evidence rather than rewriting that history.

No production source, workflow or shared build was modified by this worker. No CI rerun, commit or push was performed.
