# Independent kernel audit checkpoint

2026-10-30. Scope: `ContinuousVoiceCircuit`, not an Engine adapter, voice
allocator, host lifecycle, panic implementation, measured hardware model or CPU
qualification. See [circuit evidence and limitations](CIRCUIT_REVIEW.md).

## Frozen sources and result

| File | SHA-256 |
|---|---|
| `src/experimental/ContinuousVoiceCircuit.h` | `e55baa85f381ffef857c7521b9ef5e8b11b43ac01ebaa19a67c1210590e6d560` |
| `tests/ContinuousVoiceCircuitAuditTests.cpp` | `934aa91d871be9f3d1109da8b3fe1afa1aafebd009317ee88cf4bce106df2397` |

GCC `g++ (Debian 14.2.0-19) 14.2.0`:

- Strict optimized native: **33,962 checks, 0 failures**.
- AddressSanitizer + UndefinedBehaviorSanitizer: **33,962 checks, 0 failures**,
  with no sanitizer diagnostic. Leak detection was disabled; this is **not** a
  leak-check result. No ThreadSanitizer/concurrent-use claim is made: the kernel
  is caller-owned single-threaded state, not an atomic mailbox.

Reproduce from the repository root:

```sh
mkdir -p /tmp/continuous-voice-audit
g++ -std=c++20 -O2 -Wall -Wextra -Wpedantic -Werror -Isrc \
  tests/ContinuousVoiceCircuitAuditTests.cpp \
  -o /tmp/continuous-voice-audit/audit
/tmp/continuous-voice-audit/audit

g++ -std=c++20 -O1 -g -fsanitize=address,undefined \
  -fno-omit-frame-pointer -Wall -Wextra -Wpedantic -Werror -Isrc \
  tests/ContinuousVoiceCircuitAuditTests.cpp \
  -o /tmp/continuous-voice-audit/audit-san
ASAN_OPTIONS=detect_leaks=0 /tmp/continuous-voice-audit/audit-san
```

## Coverage and boundaries

- All four electrical NAND combinations, explicit independent cold phases and
  key voltage, zero key voltage, no note-on/off allocator API, and no gate-edge
  phase reset.
- Every floating input's declared endpoints and rejection of NaN, infinity and
  adjacent out-of-domain values, in either rank. Invalid preparation and sample
  transactions preserve the same object's **entire byte representation**,
  including coefficients and the unused RNG, plus its future accepted trajectory.
  This is not a cross-platform serialization/padding-equality assertion.
- An independently assembled routing oracle uses the unchanged production
  oscillator/SVF primitives to check two oscillator and four filter clocks,
  gain, externally supplied noise, both bypass routes and gate ordering.
  It is an independent wiring/clock oracle, **not an independent physical or
  numerical-accuracy oracle for those shared primitives**.
- Muted and fully bypassed histories evolve in both ranks; gain changes leave
  their internal state identical. An external ADSR is clocked by the caller and
  only its scalar result enters the kernel. No complete Engine EG integration is
  tested by this boundary check.
- 65,536 adversarial samples at each internal rate 16,000 / 44,100 / 48,000 /
  96,000 / 192,000 / 1,536,000 Hz; includes declared extremes, high Q and changing
  controls. A separate **60-second / 2,880,000-sample** run at 48 kHz checks a
  muted, bypassed, high-Q circuit without periodic resets. Outputs and numerical
  memories remain finite. These are numerical-domain checks, not analog limits,
  antialias qualification, infinite-duration proof or full-system stability.

## Negative controls

Seven isolated temporary-header mutants were compiled with the same final test
file and strict native flags. All compiled successfully and exited1. Only the
copied header under `/tmp/continuous-voice-audit/<name>/experimental/` changed;
it was selected using an include path before `-Isrc`. Production files were not
modified. Each row retained the same 33,962-check test program.

| Mutation | Failed checks |
|---|---:|
| NAND `!(TR && SUS)` changed to `!(TR || SUS)` | 16,316 |
| Early return when both output gains are zero | 20,483 |
| Skip HP processing while HP is bypassed | 16,339 |
| Skip LP processing while LP is bypassed | 14,318 |
| Tick each oscillator twice per accepted sample | 16,392 |
| Commit incoming key voltage before validating both ranks | 17,539 |
| Set HP0 coefficients to `(55 Hz, Q=.9, sampleRate)` before rank validation | 110 |

The last mutation is important: ordinary public-state and future-audio checks
could miss a coefficient-only write that the next valid sample overwrites. The
same-object byte snapshot detects it. The unmodified source then passed native
and sanitizer runs at the hashes above. No production blocker was found within
this kernel boundary; integration and CPU acceptance remain separate work.
