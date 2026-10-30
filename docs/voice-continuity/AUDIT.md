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

## Native/CI checkpoint — `00624e2` (2026-10-30)

Exact source: `00624e2d5389b5ea30b0bb3dea2a81077d32b109`. This adds the
experimental kernel and its tests, **not production activation**. The entire
`src/core/` and `src/plugin/` trees are byte-unchanged from `428e4b2`, including
Engine, Dsp, Params and PluginProcessor. The local audit and hashes above remain
a separate result, not rewritten by the later CI run.

[Build 41007847302](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41007847302)
completed **FAILED** because of the stable Linux validator; all four native
build/test/package jobs and the Windows validator succeeded:

| Native job | CTest suites passed | Install artifact |
|---|---:|---:|
| [Linux 123906692801](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41007847302/job/123906692801) | 45/45 | 12957080379 |
| [macOS arm64 123906693025](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41007847302/job/123906693025) | 40/40 | 12956352402 |
| [macOS x86_64 123906693699](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41007847302/job/123906693699) | 40/40 | 12956434256 |
| [Windows 123906693051](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41007847302/job/123906693051) | 38/38 | 12957001700 |

All four logs verify clean official JUCE 8.0.11 at
`c352e2489000ffeb9e397051377d75eef2efe641`; package metadata matches this source
and build, and all four artifacts were unexpired when checked. Native archive
checks passed (34 synthetic package checks on Unix, 22 on Windows).

- [DSP 41007847313](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41007847313): all five jobs passed. [Sanitizer job 123906692947](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41007847313/job/123906692947) passed **40/40 suites in 951.06 s**, with ASan/UBSan halt-on-error and **leak detection enabled**. This does not retroactively make the earlier local leak-disabled audit a leak check.
- [Screenshots 41007847315](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41007847315) passed; source-matched [panel artifact 12956606577](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41007847315/artifacts/12956606577). The experimental kernel is not selected by those editor renders.
- [Stable Linux validator job 123909705477](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41007847302/job/123909705477): headless seed `0x4b613e9` and fixed GUI seed `0x2da72db` passed. **Primary GUI seed `0x322bd8b` printed SUCCESS, then exited139.** This is distinct from `428e4b2`'s fixed-seed failure. No local ELF core was available. Existing same-seed GDB and handler diagnostics exited0, but nonreproduction proves neither cause nor repair and does not substitute for the failed original invocation. [Reports artifact 12956981178](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41007847302/artifacts/12956981178).
- [Official beta comparison 41009257077](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41009257077) passed **31 GUI-inclusive / 25 headless tests**. Both job logs explicitly select Linux VST3 artifact **12955684456**, originating run **41007847302**, source **00624e2d5389b5ea30b0bb3dea2a81077d32b109**, original conclusion **failure**. Workflow head `2b523cd` is not the tested plugin source. Different validator/JUCE versions, inventories and seeds cannot establish that the stable failure is fixed.

The stable gate remains **FAILED**. These results establish software-test scope,
not active continuous-voice Engine behavior, hardware equivalence, CPU/DAW-session
qualification or a new fully green release.
