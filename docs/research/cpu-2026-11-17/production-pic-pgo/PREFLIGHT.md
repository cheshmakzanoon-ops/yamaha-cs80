# Production PIC PGO configuration preflight

Commit `9467d903c68d3015f0bd92c78cd8dca3dfd63048`, [run 44067129187](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/44067129187), artifact `14321042143`.
ZIP SHA256: `76e87e8aeb3480adf33de127007fa3e04c5ac706424b3c101272697a783b48ef` (GitHub artifact retention 14 days).

**CONFIGURATION_PREFLIGHT_PASS**, independently audited. This is not a synth CPU measurement, plugin build validation or production enablement.

Four configurations at one canonical build path (normal, research baseline, generation, use) retain all 587 compile entries, 4,377-file patched JUCE content manifests, 569-test rosters and raw CTest definitions. Only Engine receives the declared PGO options. Engine uses production `-std=gnu++20 -fPIC`; the stock benchmark driver uses `-fPIE`. Both C and C++ driver/frontend identity are recorded, GNU 13.3.0; 21 ordinary C translation units remain unchanged. The source objects remain clean and no Engine object or synth profile is produced.

One unrelated integer-only compiler plumbing execution verifies actual GNU 13.3 `Engine.cpp.gcda` naming and same-object-path profile use, followed by expected missing-profile and coverage-mismatch compiler failures. This is not synth training, testing or timing.

## Remaining production-trial gates

509 executable commands are unavailable before build and were explicitly recorded as absent, not qualified. The subsequent trial must build and validate actual commands, preserve all existing tests/timeouts/assertions, prove the fresh profiled archive member reaches the exact tested ELF, and hash-bind host invocations. The generated plugin link has no generation runtime and executes a normal module-information helper after linking; generation must build only the core and use a separately linked training driver, while final plugin linking must preserve the helper.

The bounded production-PIC trial and full Linux release qualification have not run. The earlier 7.17% standalone core result is not a measured plugin/DAW gain. Windows, macOS and other target hardware remain unqualified. No production DSP or normal build defaults were changed.
