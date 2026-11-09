# Core release regression attempt history

Only tests/SixteenVoiceReleaseTests.cpp and its two requested CMake registrations changed. No production files, native headers, CoreTests source, or standalone behavior were patched.

Toolchain: official CMake 3.31.6 and Ninja 1.11.1.git.kitware.jobserver-1 under pitch-power-recovery/tools; GCC 14.2.0; Release; plugin OFF; AVX2 filter batching OFF.

Preserved failed attempts:
- configure-01.log / build-01.log: selected tools/bin/cmake was a Python entry point without its module on the active Python path; ModuleNotFoundError. Used the installed CMake binary at tools/cmake/data/bin/cmake instead.
- configure-02.log / build-02.log: Ninja argument accidentally named tools/ninja (a package directory), producing permission denied. Corrected to the existing ELF tools/bin/ninja. This was not an access-policy denial.
- build-03.log: test incorrectly named private Engine::kNumVoices in three loops. Corrected test to the existing eight-voice API convention, as neighboring tests use; no production access change.
- focused-01.log: 1326 checks, one failing test expectation that reset permits late binding. Existing Engine reset deliberately preserves voiceCounter, so prior-use binding must remain denied. Corrected the expectation and added separately constructed distinct-source audio/broadcast fixtures. No production defect was found.

Passing attempts:
- configure-03.log: completed correct Release/core-only configuration.
- build-04.log: first successful focused compile.
- build-neighbors-01.log: unchanged CoreTests and nine neighboring targets built.
- build-05.log, focused-neighbors-02.log: all 11 focused/neighbor CTests passed in 11.37 seconds. This log is retained without replacement.
- focused-03-verbose.log: intermediate focused successful run.
- build-06.log, focused-04-verbose.log: final focused test source, 1418 checks, zero failures, 3.34 seconds. Adds two-matching-waiter order and filtered unknown host-waiter UI coverage.

Full-suite follow-up commands and statuses are written separately by run-complete-core.sh to full-core-commands.log, full-core-build.log, full-core-ctest.log and full-core-statuses.txt.
