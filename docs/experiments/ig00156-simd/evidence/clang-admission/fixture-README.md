# Strict Clang stage-test mismatch isolation

Clang19.1.7, -std=c++20 -O3 -DNDEBUG, current scalar-only Clang admission. Production untouched. The diagnostic source variants, binaries and copied header exposing private coefficients were retained locally at `/tmp/cs80-clang-stage-analysis/` on the qualification machine. This committed directory contains the selected logs and this summary, not those diagnostic sources. They were never changes to production headers.

- Original: 90 finite Memory bit mismatches, no accelerated groups.
- [[gnu::noinline]] compare() only: same 90 failures.
- [[gnu::noinline]] prepare() only: 1,142,908 checks, zero failures.
- Global #pragma STDC FENV_ACCESS ON: 1,142,908 checks, zero failures.
- Extra public Memory/half-step/Q prechecks: 2,069,652 checks, still only90 failures.
- Copied header exposing private fields identifies different starting coefficient values before process: first malformed-slope fallback case (compare16399), lane0 xFromFb differs1ULP, xFromIn/xFromU2ULP; lane3 xFromFb/xFromIn/xFromU/xFromBp also differ. See coefficients.log. These starting differences are invisible in Memory (which is process history, not full setup).

This is a setup-oracle problem: two independently optimized constant prepare paths do not begin process with bit-identical coefficients. It is not demonstrated to be a process wrapper arithmetic defect. An opaque common test setup helper preserves the ordinary compiler contract while making starting coefficients identical. Dynamic fesetround cases separately require a rounding-aware compile contract; existing lack of FENV_ACCESS must not be inferred safe merely because tests happened to pass. Keep finite-bit equality assertions unchanged. No production flags or sources edited.

Compile pattern: LD_LIBRARY_PATH=/tmp/cs80-portability/apt/root/usr/lib/x86_64-linux-gnu /tmp/cs80-portability/apt/root/usr/bin/clang++-19 -std=c++20 -O3 -DNDEBUG -I<source>/src <variant>.cpp <source>/src/core/Ig00156Batch.cpp -o <variant>. Coefficient diagnostic uses copied/src, others actual worktree src.
