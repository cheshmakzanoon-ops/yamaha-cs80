# Post-review compiler admission evidence

This directory records the correction after `bf3bb816`; evidence in the parent
directory predates it unless explicitly noted. Current code is GCC-only for
SIMD. Strict Clang/AppleClang is deliberately unsupported and remains scalar.
Production Engine and kernel arithmetic did not change in this correction.

- `gcc-profiles.json`, `clang-profiles.json`: final independent caller/backend
  probes for 11 math profiles each; Clang also links each profile to a strict
  GCC backend. Macro summaries retain the compiler admission evidence.
- `negative-control.txt`: removing the Clang exclusion fails 5 checks. It is an
  intentionally failing control, not a production test failure.
- `gcc-ctest.txt`, `clang-ctest.txt`: final Release core-only CMake runs, option
  ON, 12/12 GCC and 4/4 Clang tests. The dedicated admission matrix is included.
- `independent-*` and six `*-engine.log` files: independently built original,
  current ON and current OFF Clang Engine corpora, strict and unsafe profiles.
  Each profile is internally bit-identical. Tool paths in the retained command
  script are this machine's local evidence locations, not portable defaults.
  Only core compile commands are retained. The independent source hashes were
  frozen before the later test-only fixture correction and CTest cross-build
  guard; production header/Engine/backend hashes are unchanged.
- `fixture-*`: independent diagnosis of the strict-Clang setup-oracle defect,
  including coefficients differing before any process call, and noinline-only
  correction. No production header is exposed or changed by the diagnostic.
- `*-stage.txt` and `*-rounding.txt`: final ordinary and test-only
  `-frounding-math` stage runs. Dynamic-fenv qualification requires the latter
  contract; ordinary compilation is retained as a separate gate.
- `source-sha256.txt`: final production/test/build files for this correction.

Portable reproduction from the repository root:

```sh
python3 tests/test_filter_batch_admission.py --compiler g++ --output /tmp/new-gcc-admission
python3 tests/test_filter_batch_admission.py --compiler clang++ --output /tmp/new-clang-admission
# Optional --strict-backend-compiler /path/to/g++ explicitly selects the mixed backend.
cmake -S . -B /tmp/new-clang-core -DCMAKE_CXX_COMPILER=clang++ -DCS80_BUILD_PLUGIN=OFF -DCMAKE_BUILD_TYPE=Release -DCS80_ENABLE_AVX2_FILTER_BATCH=ON
cmake --build /tmp/new-clang-core --target cs80_ig00156_batch_tests cs80_filter_batch_engine_tests cs80_filter_batch_scheduling_tests -j2
ctest --test-dir /tmp/new-clang-core --output-on-failure -R '^cs80_(ig00156_batch_tests|filter_batch_engine_tests|filter_batch_scheduling_tests|filter_batch_admission_tests)$'
clang++ -std=c++20 -O3 -DNDEBUG -frounding-math -Isrc tests/Ig00156BatchTests.cpp src/core/Ig00156Batch.cpp -o /tmp/clang-rounding-stage
/tmp/clang-rounding-stage
g++ -std=c++20 -O3 -DNDEBUG -frounding-math -ffp-contract=off -Isrc tests/Ig00156BatchTests.cpp src/core/Ig00156Batch.cpp -o /tmp/gcc-rounding-stage
/tmp/gcc-rounding-stage
```

No scalar-only unsafe-flag bitwise promise is inferred from admission probes.
The historical unsafe stage mismatch counts are retained with their enabled /
disabled controls in the main report. No new performance, hardware-fidelity,
AppleClang execution or non-AVX timing claims are made by this correction.
