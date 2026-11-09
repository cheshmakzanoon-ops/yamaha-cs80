# Legacy SUB oracle evidence

[Scope and result](../../LEGACY_SUB_LFO_ORACLE.md). Production source is unchanged
from `4ef0666`; test implementation is `1782677`, checker hardening `c95e1fa`.

The preserved logs are local Linux GCC 14.2 / Clang 19.1.7 results. Configure
separate build directories for each compiler and each math profile. Example:

```sh
cmake -S . -B /tmp/cs80-oracle-gcc -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCS80_BUILD_PLUGIN=OFF -DCS80_BUILD_TESTS=ON \
  -DCS80_ENABLE_AVX2_FILTER_BATCH=OFF -DCMAKE_CXX_COMPILER=g++ \
  '-DCMAKE_CXX_FLAGS_RELEASE=-O3 -DNDEBUG'
cmake --build /tmp/cs80-oracle-gcc --parallel 2 --target \
  cs80_tests cs80_legacy_sub_lfo_oracle_tests
ctest --test-dir /tmp/cs80-oracle-gcc --output-on-failure \
  -R '^cs80_(core|legacy_sub_lfo_oracle)_tests$'
```

Repeat with `clang++` and in a **different** build directory add `-march=native`
to the release flags. Build core and tests with the same profile. Optional AVX2
filter batching remains disabled. The four recorded compile-command and cache
files identify the actual local commands/compilers, not a universal binary pin.

For complete-core sanitizers, use another fresh build directory with release
flags `-O1 -g1 -fsanitize=address,undefined,float-cast-overflow
-fno-omit-frame-pointer -D_GLIBCXX_ASSERTIONS`, build both targets, then run with
`ASAN_OPTIONS=detect_leaks=0`. To run all configured core-only tests, build the
complete default configuration and run `ctest --output-on-failure --timeout 600`.
The exact completed run, any earlier interrupted attempt, and limitations are
separately retained rather than silently replacing evidence.

Fault controls (GCC generic, existing matching core archive; output must not
already exist):

```sh
PYTHONDONTWRITEBYTECODE=1 python3 \
  docs/experiments/legacy-lfo-oracle/check_mutations.py \
  /tmp/cs80-oracle-faults /tmp/cs80-oracle-gcc/libcs80_core.a
```

The script copies source into that new scratch directory, changes only each
named fault, links its private Engine object before the unchanged archive and
requires completed numerical rejection. The seventh fault changes only the test
validator. It never edits production. The original first six core checks can be
reproduced by compiling `4ef0666`'s `tests/CoreTests.cpp` against its byte-identical
native core; keep original headers paired with that source and library.

`manifest.json` pins every file in this experiment directory (including the
verifier and preregistration) except itself, plus the report. Its own identity is
the containing Git commit and the separately reported SHA-256. Source pins cover
all 166 production files and the exact test/build inputs. The excluded-artifact
index records local binary and full mutant-tree identities. Large executables and copied mutant trees are not repository
payloads. `verify_evidence.py` verifies committed content hashes and reported
counts; it does not rerun binaries or claim external excluded files exist on a
new checkout. Independent review artifacts are explicitly labeled.


## Inspect the archive without running tests

```sh
PYTHONDONTWRITEBYTECODE=1 python3 \
  docs/experiments/legacy-lfo-oracle/verify_evidence.py
PYTHONDONTWRITEBYTECODE=1 python3 -O \
  docs/experiments/legacy-lfo-oracle/verify_evidence.py
```

The verifier uses explicit exceptions, so optimized Python does not disable its
checks. It rejects omitted/extra artifacts, altered hashes, wrong exact result
counts, incomplete scenario grids, wrong surviving arithmetic tracks, insufficient
wraps/draw counts, and misrepresented interrupted attempts. It does not rebuild or
execute any binary. `--external` additionally verifies the deliberately excluded
binaries and mutant trees at their recorded original paths; that option is useful
only in the original workspace, not a requirement on a fresh checkout.

Archive layout:

- `evidence/runs/`: byte-preserved build/test/driver logs, mutation commands and
  summaries, original placeholder command, completion statuses and attempt notes.
- `evidence/builds/`: selected actual CMake cache entries, all three core and both
  test compile commands, and generated Ninja compilation/link commands for each
  of the four optimized profiles and the complete-core sanitizer profile. These
  are read-only extractions from the completed builds, not fresh executions.
- `evidence/diagnosis/`: historical baseline test/probe source copies, first
  frame-398 discrepancy logs, fused/split speed probes and the diagnostic report.
  The report predates this repair; its proposed next step is historical. The
  outlined `noinline` pass is diagnostic only, not a portable solution. These
  copies are not registered test targets. Large disassembly/binaries are excluded.
- `evidence/source-pins.json`: comparison base, implementation/checker commits,
  production source tree and per-file SHA-256 pins.
- `evidence/excluded-artifacts.json`: sizes and SHA-256 identities of binaries,
  objects and complete copied mutant directories omitted from this repository.
- `evidence/review/`: independent source-review report, focused native pass, and
  independent coefficient/revived-eligibility binary reruns, and final checker
  review (two valid parser inputs accepted, 11 invalid forms rejected). Read the recorded
  scope before attributing compiler-matrix, sanitizer or full-suite execution.
- `evidence/original-native-reconstructed-command.json`: exact local paths for
  the original failing test compile, explicitly reconstructed rather than a
  contemporaneous transcript. Its input and output identities are pinned.

Absolute paths inside preserved files refer to the **original** workspace. They
are provenance, not paths expected to exist after cloning. The scripts remain at
`docs/experiments/legacy-lfo-oracle/`; do not run a copied or relocated checker.
For a fresh run replace build/output paths and supply the matching compiler and
core archive. The fault driver requires Linux `taskset` and CPUs 0–3 to be allowed,
and invokes `c++`; ensure that command selects the intended GCC before running.
No compiler/tool versions or native CPU ISA are promised on other machines.

For the original four-failure control, extract base `4ef0666` into a separate
scratch checkout, configure its native GCC core with the same flags as above,
and compile that checkout's `tests/CoreTests.cpp` with its `src` include directory
and native `libcs80_core.a`. Use the exact argument order in the reconstructed
command file, substituting only workspace locations. Its expected result is
`4 OF 11508 CHECKS FAILED`, not success. No new original-test execution was
performed while packaging this archive.

The first completed mutations used `af79529`; the final completed restart used
`c95e1fa` with unique terminal count enforcement. The interrupted final attempt
contains only its first compile command and has **no** numerical result. The
interrupted default CTest attempt stopped after result 35/142 and has **no** final
verdict. Only the restarted full-suite log/status supports the final suite result.
The fault driver succeeds only if each mutated program returns normal status 1
with 54 scenarios, exactly 1,958,109 checks and a nonzero bounded failure count.
Build failures, signals, timeouts and partial/duplicate summaries are not passes.


## Frozen result

The completed restarted default core-only suite passed **142/142** tests, zero
failures, exit 0, in **907.75 seconds**. All four optimized compiler/profile runs
passed 54 scenarios / 1,958,109 checks, 11,508 core checks, and both focused CTests.
The separate complete-core sanitizer run passed the same scenario/core counts.
All seven final fault controls reproduced the exact failure counts in the report;
the driver exited 0 only after all seven mutated programs completed with exit 1.
This archive makes no plugin, GUI, native-installation, DAW, hardware or speedup
qualification claim. Run integrity verification from this archived revision;
intentional later edits to pinned source/tests will correctly fail its snapshot
checks.
