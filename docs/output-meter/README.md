# Independent output-meter audit

Frozen 2026-10-30. **6,348 checks, zero failures** in native, AddressSanitizer/UndefinedBehaviorSanitizer, and ThreadSanitizer executions. Compiler: `g++ (Debian 14.2.0-19) 14.2.0`. No production files were edited by this reviewer.

| Input | SHA256 |
|---|---|
| `src/plugin/OutputMeter.h` | `c808318ef5055d58cb9ef214a964de007dfce328c83db1f856d299ea8ffaca08` |
| `tests/OutputMeterAuditTests.cpp` | `3bce82f358288d494ae74cbc49fb3c7807bcd87a78b8c1410133158339f0769d` |

## Evidence and contract

- Enumerated **3,136 reset schedules**: seven initial peaks × seven incoming peaks × 64 choices of consumer exchanges before/after load, strong CAS and fallback store. Every observed history matches an exact-max publication linearized among the exchanges. Positive publication uses one load, one validating strong CAS and at most one fallback store; zero skips publication.
- **16 injected stale-load cases** separately validate the historical-value corner. Actual mailbox `0`, historical load `3`, incoming peak `2` must leave `2`. A temporary mutant reinstating the old `peak > previous` skip leaves `0` and was rejected. This is a deterministic robustness model, **not a native stale-read failure reproduction or exhaustive weak-memory proof**.
- A real producer thread publishes **200,000 blocks** while the consumer polls. The global finite maximum and both event kinds survive; the quiescent final consume drains both mailboxes. TSan executed successfully and reported no race.
- Exact finite magnitude, signed-zero handling, threshold strictly greater than `1`, and separate NaN/Inf detection are covered. NaN payloads and every input sample bit remain unchanged. Native x86 tests enable **FTZ/DAZ** and verify minimum subnormal, largest subnormal and normal peaks bit-for-bit; integer magnitude ordering avoids float-comparison flushing.
- Editor-open-style consume/discard preserves later publications. Stopped lifecycle reset clears telemetry. Peak and flags intentionally are **not a coherent snapshot** and can reach adjacent UI polls.

The proof relies on one nonzero audio producer; concurrent UI writes only clear to zero. The validating CAS avoids trusting a potentially stale larger relaxed load. A failed strong CAS therefore leaves only a clear to handle, and one fallback store cannot overwrite a later value from another producer. No source-level retry loop exists.

**Boundedness means a bounded count of C++ atomic operations, not a machine-level wait-free or worst-case execution-time proof.** The lock-free static assertion does not establish bounded hardware/LL-SC latency. Scanning cost is linear in the actual output sample count. The independent flags add at most one event `fetch_or` per published block.

The audit tests helper nonmutation; genuine host audio/MIDI/parameter comparisons live separately in `tests/OutputMeterHostTests.h`. Their host execution status must be reported separately. No limiter, DSP safety guarantee, true-peak, loudness, native AU, or hardware-fidelity result follows from this audit. Panic retains FX/noise and does not implicitly clear telemetry or promise immediate silence.

## Reproduce

From the repository root:

```sh
g++ -std=c++20 -O2 -Wall -Wextra -Wpedantic -Werror -pthread -Isrc \
    tests/OutputMeterAuditTests.cpp -o /tmp/output-meter-audit-native
/tmp/output-meter-audit-native

g++ -std=c++20 -O1 -g1 -fno-omit-frame-pointer -fsanitize=address,undefined \
    -pthread -Isrc tests/OutputMeterAuditTests.cpp -o /tmp/output-meter-audit-asan
ASAN_OPTIONS=detect_leaks=0 /tmp/output-meter-audit-asan

g++ -std=c++20 -O1 -g1 -fsanitize=thread -pthread -Isrc \
    tests/OutputMeterAuditTests.cpp -o /tmp/output-meter-audit-tsan
/tmp/output-meter-audit-tsan
```

Each final execution printed:

```text
Independent output-meter audit: 6348 checks, 0 failures
```

Leak detection was explicitly disabled for ASan because this environment's LeakSanitizer/ptrace incompatibility was established earlier. **No leak-check pass is claimed.** TSan is a separate successful run, not inferred from ASan or ordinary concurrency stress. The FTZ/DAZ-specific test is explicitly skipped on non-x86 targets; those platforms still run representation and mailbox checks.
