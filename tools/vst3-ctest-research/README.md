# Full-suite CTest scheduling trial

Separate diagnostic only. Production `build.yml`, source, CMakeLists and all test
assertions remain unchanged. This workflow freshly builds all test targets, then
runs the entire frozen Linux inventory through CTest itself: exactly twelve
previously qualified VST3 tests at concurrency two, followed by the other 557
serially. The remainder runs even if the first group fails or its reporting
fails. The final result aggregates both groups, never hides a failure.

The prior diagnostic at commit672cfe45/run43857489182 demonstrated only isolated
12-test feasibility. This trial closes different gaps: native CTest execution,
full-suite interaction, exhaustive membership, reporting and failure continuation.
A successful trial is evidence for review, not automatic production rollout.

## Invariants

- Frozen source/CMake baseline f9dc1586; pinned clean JUCE8.0.11 and immutable
  previously reviewed helper. All 569 names must be unique and match the prior audited roster digest; the exact12/557
  partition must match CTest's own filtered inventories and union to the full set.
- Generated diagnostic-build `CTestTestfile.cmake` is saved byte-for-byte. Only
  twelve per-case HOME/XDG environment properties are appended. Structural
  inventories must prove every command, timeout and other property unchanged.
  The generated file is restored in `finally`; unexpected concurrent edits are
  not overwritten, and failed restoration fails acceptance. Repository CMake
  files are never edited.
- Original 180s limits remain on selected tests. Other tests keep every original
  property, including teardown PASS_REGULAR_EXPRESSION and packaging labels.
  No global parallelism, stop-on-failure, retries or timeout relaxation.
- CTest runs with --no-tests=error. Each group has separate JUnit, console/output
  logs, LastTest.log and failure/cost receipts. Exact JUnit name multisets and
  CTest exit codes must agree. Only the existing cs80_filter_batch_cap_tests return77 with explicit
  SKIP_RETURN_CODE=77 JUnit reason may produce a reported skip; a skip is never described as a pass.
- Original PWM output paths remain intact, with exact file roster/size/hash
  receipts. Selected isolated homes must remain empty. Module/fixture/generated
  test-file hashes must be unchanged at completion.

Real-CTest tiny-fixture tests exercise exhaustive scheduling, selected failure
with remainder execution, reporting exceptions/loss, duplicate/missing inventory
and XML, property drift, unexpected skip, symlink refusal and failed restoration.
The diagnostic uses read-only GitHub permissions, build concurrency2, selected
concurrency2/rest1, a240minute job limit and first attempts only. A canceled or
terminated job cannot promise remaining-test execution and is incomplete.

Relevant main pushes/manual dispatch can create a run. Do not repeat or widen
this protocol to select a favorable outcome. Review any change first. This
research does not publish synth binaries or claim a synth CPU improvement.

```
python3 -m unittest discover -s tools/vst3-ctest-research -p 'test_*.py' -v
python3 tools/vst3-ctest-research/run.py --repo "$PWD" \
  --build "$PWD/build-ctest-diagnostic" --out "$PWD/vst3-ctest-results"
```

Inspect `results.json`, `scheduler-result.json`, both JUnit files and raw logs.
Normal production CI remains a separate run and must be reported separately.
