# A47 — original Windows failure and accepted fixture correction

**Windows qualification failed and remains pending.** Original source `ef1c472b161c37c490945f7c469086508aae6aef`, run **45439302351**, attempt **1**, producer **138605635333**, completed failure at **2026-11-24 17:36:59 UTC**. Original complete evidence has **261 registered: 258 passed, one failed, two expected formal skips**, with 11 separately counted nested Python skips.

The sole failure is **`cs80_sixteen_voice_release_tests`**, JUnit `SEGFAULT`, **1.60477 seconds**, empty LastTest output. No native numeric exit/NTSTATUS, PE, dump or backtrace was uploaded. The Test runner step exited 1; full CTest time was 2490.92 seconds. Damper passed **231 checks**, and separate host release passed **768 checks**. Windows product upload/package and pluginval were skipped. No Windows installation exists for this attempt.

Artifact **14946638643** was retrieved completely: **87,218 bytes**, SHA-256 `6b7944de9feb18b054d96f7a33af49e7acedd51a21e9b7e4fb744342b283ff0f`, agreeing with original API and upload log. Full raw evidence and API collections are in `a47-failure/`; immutable recovery identities are in its proof packet/map.

## Diagnosis and narrow correction

CMake already applies **8 MiB** to this test. Large by-value `Pair` fixtures make stack use compiler-dependent. GCC14.2 measures `sizeof(Engine)=552392`; the original largest optimized frame is **6,634,096 bytes**, or **8,846,384 bytes** with stack-slot reuse disabled. The latter exceeds the existing budget.

The accepted proposal changes only test fixture ownership: two `unique_ptr<Engine>` owners and the existing `first`/`second` reference aliases. Clock construction/lifetime order, every assertion, event/render sequence, production source, timeouts and stack budget remain unchanged. Root accepted this correction for later integration; A47 did not edit main.

| Local GCC control | Original | Proposed fixture |
|---|---|---|
| Ordinary optimization, 8 MiB | 1,418 checks pass | 1,418 checks pass |
| Ordinary optimization, 4 MiB | SIGSEGV, empty output | 1,418 checks pass |
| No stack-slot reuse, 8 MiB | SIGSEGV, empty output | 1,418 checks pass |

Proposed largest frames fall to **2,764,784 bytes** normally and **3,320,960 bytes** without reuse. This demonstrates a real fixture stack risk; it does **not** establish the exact MSVC crash mechanism. Fresh source-bound complete Windows tests and both original pluginval lanes remain required. No blind rerun, weakened check or platform acceptance occurred.
