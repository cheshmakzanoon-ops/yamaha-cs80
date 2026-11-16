# Diagnostic only: native macOS ARM thread CPU clock

Frozen base: `a1e51ee0483ec70ce6e07061936a3777ee52bb1d`.
No shipping source, core, denormal policy or existing test is changed. No retry,
performance gain, DAW CPU target, audio correctness or ARM GUI fix is claimed.

The isolated workflow runs once per own-path push and only on attempt 1, with
read-only contents permission and no persisted checkout credentials. macos-15
must pass a native arm64 gate. The job limit is 25 minutes; configure 120s,
build 600s, native probe 15s, and each of three benchmark executions 180s.
Command timeouts kill the process group. Quick still renders release tails
(up to 40 seconds of audio per applicable case); it does not mean 16 callbacks
for every phase. There is no automatic retry or reduced workload on timeout.
A timeout is an inconclusive qualification failure, not a negative clock result.
Raw logs, receipts, binaries/core hashes, compiler commands and output survive
failures via always-upload. Early checkout failure may produce no artifact.

## Exact experiment

1. Archive the frozen source; generate candidate by replacing exactly two
   Linux-only preprocessor guards with Linux-or-Apple guards. Count/implementation
   guards fail closed. No metadata strings or arithmetic are changed. In particular
   the benchmark's historical text saying Linux-only is intentionally retained;
   this README and receipt describe the diagnostic exception.
2. Build both wrappers with identical Release target settings, linking the SAME
   `cs80_core` static archive. The original benchmark body is untouched. Both
   receive the same wrapper (rename main, read FPCR/FPSR, call main, record exit).
   This wrapper is diagnostic instrumentation, not a byte-identical shipping
   executable. Original/candidate body difference is only the two clock guards.
3. Probe public `clock_gettime(CLOCK_THREAD_CPUTIME_ID)` and `clock_getres`.
   Apple's public SDK `time.h` supplies these identifiers (macOS >=10.12).
   Record nominal resolution, 100,000 finite monotonic reads and zero-delta count and aggregate
   read-loop wall/thread CPU cost; this is not isolated per-call overhead.
   Record 200ms current-thread busy, sleep, background-worker plus sleep
   thread/process/monotonic deltas. Broad isolation gates reject unavailable,
   backwards, charged-sleep or no-worker-accounting readings. Contention can
   cause an honest inconclusive failure; never loosen the gate after a failure.
4. Exactly three executions, fixed order: original clock-off, candidate clock-off,
   candidate `--callback-cpu`. All `--quick --rate 48000 --block 128 --callbacks 16`.
   Source/body, core and binary SHA256 hashes and each invocation's log survive.
   A failed execution stops the qualification, rather than retrying it.
5. Compare all deterministic metadata and all CSV columns except an explicit
   timing/accounting allowlist. This includes checksum, peak, RMS, workload,
   event counts and voice/tail outcomes. Require the fixed 11-case/19-row inventory, 48k/128, zero nonfinite samples, matching completion/rows,
   candidate availability, callback call count equal to each row's callbacks,
   finite positive CPU values and CPU/audio ratio consistency.

FPCR/FPSR are recorded around the probe and actual benchmark calls in the same
process/thread. FPCR must remain unchanged; benchmark entry FPCR must also
match across all three runs. FPSR is recorded, not required unchanged: ordinary
floating point arithmetic may legitimately set cumulative status flags. No FP
register is written. FPSR equality is not proof of identical arithmetic.

Deployment target is explicitly macOS15.0/arm64; compile-time API availability is gated at10.12 and SDK version/path are recorded. The core compile command must explicitly disable the optional AVX2 backend. Clang/ARM uses the scalar compiler backend, unlike the separately qualified
GCC AVX2 path. ARM's unchanged denormal state is not x86 MXCSR FTZ/DAZ. Therefore
this qualifies a diagnostic clock at most, not cross-platform benchmark timing
comparability. Fixed order, cache state, runner load and extra clock reads all
confound timing. Checksums/statistics are NOT PCM captures: no PCM-exact claim.
The broad native probe is separate from actual callback timing, whose checks
validate finite accounting, counts and ratios, not an external accuracy oracle.

Local Linux validation: `python3 -m unittest discover -s tools/mac-thread-clock-qualification`.
Native C++ ARM instructions and macOS API launch cannot be validated here;
that is the bounded CI experiment, subject to independent review before publish.

Public API reference: [Apple Libc clock_gettime(3)](https://github.com/apple-oss-distributions/Libc/blob/main/gen/clock_gettime.3)
documents the calling-thread CPU clock, process CPU clock, resolution query,
and macOS10.12 introduction. Nominal resolution need not equal actual precision.
