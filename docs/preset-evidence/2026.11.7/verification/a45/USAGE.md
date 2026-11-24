# A45 — original Intel failure and bounded audit repair

This directory preserves observations of the original failed Intel job and the local evidence for the authorized offline audit repair. It does not qualify Intel, another platform, or a release.

## Source and original failure

The original source is `c5549f7d095a5c5f49a5ad22ec0ccc819245d201`; the comparison baseline is `18711f93a39aa49f3d6f9a41784be456224facbe`. The original Intel producer is job `138415960996`, run `45385716988`, attempt 1. Its authenticated API/log acquisition is retained by root under `../../ci/source-c5549f7d/macos-x86_64/`. The downloaded native archive is `/tmp/cs80-ci-1cee5a/c5549f7d/macos-x86_64-native.zip`.

`original-failure-observation.json` hashes these original inputs and all three native ZIP members. It compares the complete JUnit name set and full LastTest numbering to A29's pinned roster. `original-failure-lasttest.txt` is the exact failed LastTest block. These record 274 passes, two formal skips and one bus-error failure in the 277-test original suite. No original failure has been replaced with a local result.

## Static analysis

`sizeof-engine.cpp` only prints `sizeof` values. It does not construct an Engine. `sizeof-results.json` retains the successful ordinary compile/run commands and outputs against both frozen header trees.

`frozen-build-commands.json` records the complete Linux GCC 14.2 compilation of the frozen core and original audit. `repair-build-commands.json` records compilation of the repaired audit and the baseline audit stack report, with the repaired executable linked to the exact same frozen core object files. The Release flags are C++20, `-O3 -DNDEBUG -fPIC -g1 -fstack-usage`; the optional AVX2 batch backend is disabled, matching the scalar backend used on Mac. The held-WSC definition remains applied only to its two original translation units. These are local diagnostic builds, not claimed reproductions of AppleClang code generation.

The `*.stack-usage.txt` files are unmodified GCC stack-usage output. `static-stack-analysis.json` retains selected records, hashes of the source inputs, the source call chain, documented Darwin default-stack references and the limits of the diagnosis. The measured `measure()` frame falls from 504,944 bytes to 176 bytes. The current original frame plus a reachable interpolation-setup frame already totals 525,584 bytes, before other frames and call overhead, against the documented 524,288-byte Darwin worker default. Neither the native frame size nor the exact native crash site was observed.

No deliberately constrained-stack run, preload probe or induced-fault reproduction was executed. Static reports and ordinary tests are the complete local diagnostic basis.

## Normal behavior comparison

`compare-actual-presets.cpp` includes an audit source selected at compile time with `A45_AUDIT_SOURCE`, renames that source's original `main`, and invokes the actual `measure()` function on a bounded selection of factory programs. The original and repaired executables link the identical four frozen core object files. The helper does not modify audio operations inside `measure()`.

`actual-presets-comparison.json` records the exact compile/run commands, source context, core-object hashes, exit codes and runtimes. The two TSV files contain index, name, category, phrase peak, loudest-window RMS, stress peak, finite flag and circuit flag. Hexadecimal floating-point output retains all bits of each recorded double. All eight selected programs' 24 double results and 16 flags match exactly. This compares the actual audit metrics, not every PCM sample or every factory program.

`full-audit-normal.log` and `full-audit-normal.json` retain the complete existing repaired LibraryAudit execution with the original data, all 523 programs, two workers and all thresholds. It uses ordinary process/thread settings, with `CS80_LIBRARY_AUDIT_DATA_ONLY` absent. The existing 600-second test budget is not enlarged. The completed run returned exit 0 in 345.038464511 seconds and reported 301 curated plus 222 original programs. This local pass does not clear the original native failure.

## Repair and next acceptance

`library-audit-heap-repair.patch` is the reviewed diff from the failed frozen source. Only `tests/LibraryAuditTests.cpp` changes outside this evidence directory: an explicit `<memory>` include, a rationale comment, and one `std::make_unique<Engine>()` plus reference in each existing separate scope. Each phrase/stress Engine is still freshly constructed and destroyed at the same boundary. Test registration, worker scheduling, audio operations, rendered durations and thresholds remain unchanged.

Root owns source publication and fresh native qualification. A local pass cannot clear the original Intel failure, replace skipped validator/package steps, or reuse another source's accepted artifacts. The successor requires the complete native suite and all original validation, payload and delivery gates.
