# Focused core instrumented checkpoint: ccb812e

Source: `ccb812e0e3466c1bdeb0ac59e205a4eaec5aa461`, November 10, 2026.
An isolated core-only RelWithDebInfo build completed all 12 build steps for the
four selected executables. Both core and test translation units use
`-fsanitize=address,undefined -fno-omit-frame-pointer`; linking uses
`-fsanitize=address,undefined`. Plugin build is OFF, tests ON. The existing
`CS80_TEST_TIMEOUT_SCALE=4` option extends deadlines without changing assertions.

## Leak-enabled attempt: failed

With the CI-style options `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1` and
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`, all four tests failed
(116.48 seconds total). Each ended with LeakSanitizer's fatal runtime-support
message and its hint that it does not work under ptrace. These were not leak
reports, but neither are they passing leak checks. The failed result is retained;
the CI sanitizer gate was not changed or qualified by this attempt.

## Separate narrower ASan/UBSan run: passed

Using the same binaries and assertions, a separate serial CTest invocation used
`ASAN_OPTIONS=detect_leaks=0:halt_on_error=1` and the same UBSan options. All four
selected suites passed, zero failures, **119.03 seconds**:

- `cs80_core_tests` (46.07 s)
- `cs80_controller_reset_tests` (29.43 s)
- `cs80_scanner_ordering_tests` (3.63 s)
- `cs80_tuning_midi_tests` (39.91 s)

This is bounded address/undefined-behavior instrumentation coverage with leak
detection explicitly excluded. It is not a replacement for the failed
leak-enabled attempt, a full core-suite run, or proof of memory safety outside
these executions. No native UI, wrapper, DAW, hardware-fidelity, release or
performance qualification follows. No product code, test assertions, CI
workflow, or global security settings changed for these checks.
