# Preregistration: exact idle TWS fixed-state and global-engine continuation

2026-11-02 · **test only; production `src` unchanged; CPU targets not met**

## Independent original and scope

The previous full-phase-lane preregistration is test-only commit `71a0226e861a751ae8c0ffef41d07ed009369170`. This separate idle registration begins from its *unchanged* source tree `7dd159d491012fd47b46c0aa651e86ea959ca289`.

`tests/fixtures/touch_idle/TouchCircuitBefore.h` is a static, independent copy of `src/core/TouchCircuit.h`. `diff -u` proves its **only** changes are the two `namespace cs80::touch` declaration/comment lines, changed to `cs80::touch_original_20261102`. Original SHA-256 `f89506cc7222b778b9e694d36315e0eb8433730320f2919a8c40a19f62272045`, frozen SHA-256 `9da1b2e24182aca0f23666a0aba70205fd780d47cf4b996a642d0b0f01a91471`, original Git blob `1a50ecfeba752576fd60cb4d1060e94054609538`. These are sealed in `tests/fixtures/touch_idle/manifest.json`; neither frozen code nor source can be rewritten from a future fast-path implementation.

`tests/TouchIdleExactOracleTests.cpp` co-compiles **different instances** of the untouched live and frozen `TouchVoice`, its complete coefficient preparation and shared TRG5 diode-bus solver. The software continuation is an independent reference, **not recovered-hardware validation**. The original physical TWS/TRG tests remain mandatory.

## Positive control before any optimization

| Coverage | Original-source result |
| --- | ---: |
| Rates | 48,000 / 96,000 / 384,000 Hz |
| FP modes | 4 standard rounding × 4 supported x86 MXCSR FTZ/DAZ states |
| Hold leakage | 0 / 3.3 seconds |
| Independent voices | Eight TWS a/b/C states with pressure, copy, reset, trigger and wake |
| Total TWS samples | 6,291,456 |
| Samples exposing zero-state behavior | 2,532,096 |
| Shared diode-bus comparisons | 786,432 |
| Copies / resets / triggers | 96 / 96 / 288 |
| Explicit bit/FP checks | **51,119,247** |
| IEEE bit mismatches | **0** |

Every TWS tick checks the actual returned binary64 A/B/**signed C** and subsequent `state()` bits against the frozen original. All nine coefficient fields and TRG5 bus results are checked, and selected samples begin with FE_INEXACT raised so a candidate cannot silently clear sticky exception status. Caller rounding/FTZ/DAZ controls must be unchanged.

This caught an important shortcut trap **before coding**. At all-positive-zero physical state, the original computes `y=b-memory` then returns `C=-y`. With downward rounding, the sign of an exact zero subtraction need not equal its nearest-rounding sign. Never hardcode `C=-0` for all modes or treat an unchanged audio gate as a proof of unchanged physical/controller state. The candidate must use original scalar behavior in any unqualified FP mode.

## Engine original-source and partition witnesses

The same test renders 150,528 host frames (48 kHz, original 2× internal rate), with notes, per-key pressure, note-off, ribbon movement, long quiet-idle, and wake events at fixed absolute frame positions. It compares the entire stereo audio stream and a 256-frame checkpoint trace of all eight TWS A/B/C readouts, rank filter/env/oscillator controls, 32 Q receiver ports, 16 LC ports and shared buses, plus three always-clocked output tap/capacitor readouts. Each scenario renders with callback partitions of **1, 17 and 256** frames: all three traces must agree exactly. Two policies are exercised: PURE source isolation with zero analog noise, and sourcePerformance with active modern drive FX and deterministic nonzero noise.

The original-source Linux x86-64 GCC 13.3/glibc 2.39 fixture emitted:

| Policy | Every partition: audio FNV64 | Every partition: global-state FNV64 |
| --- | --- | --- |
| PURE isolation | `7d101956e93b525d` | `6046acf7ccf9a6d7` |
| sourcePerformance + FX/noise | `70ba56de3cba74ad` | `a4cedcc115867e62` |

These recorded original-source bit-stream **fingerprints** are immutable in the driver and manifest and asserted on the matching Linux/GCC/glibc runtime. Other target/compiler/libm combinations still run the full independent frozen TWS test and intra-host 1/17/256 exact comparison; they do **not** claim Linux FNV cross-platform equality. FNV64 is a sensitive regression sentinel, **not a collision-free proof**; existing independent strict V2 *byte* compatibility, engine/click and wake source/physical checks remain mandatory before optimizing Engine code. On a new platform, qualify and freeze its original engine baseline *before* testing any changed source.

## Five separate negative controls

Each intentionally wrong candidate operation compiled successfully at the original `-O3 -DNDEBUG -std=gnu++20` numerical options and failed normally with `FAIL` and **exit code 1**, not a crash or skipped test:

| Mutation definition | First named failure | Witness |
| --- | --- | --- |
| `CS80_TOUCH_IDLE_MUTATE_ZERO_SIGN` | `touch.C.signedZeroAndWake` at sample 0/lane 0 | `+0` vs frozen `-0` output bit |
| `CS80_TOUCH_IDLE_MUTATE_CROSS_LANE` | `touch.a` at sample 321/lane 1 | Neighbor pressure contamination |
| `CS80_TOUCH_IDLE_MUTATE_BUS` | `TRG5.busVolts` at sample 321 | Shared diode-bus sum changed independently of lane state |
| `CS80_TOUCH_IDLE_MUTATE_ENGINE_GOLDEN` | `engine.frozenAudioBits` | All three partitions identically corrupted: self-comparison stays green, frozen original catches the difference |
| `CS80_TOUCH_IDLE_MUTATE_STICKY` | `touch.candidateStickyFlagsPreserved` at sample 0/lane 0 | Cleared caller FE_INEXACT flags |

The compile format is `g++ -O3 -DNDEBUG -std=gnu++20 -fPIE -Isrc -Itests -D<mutation> tests/TouchIdleExactOracleTests.cpp build-core/libcs80_core.a -o build-core/<isolated-mutant>`. Expected-fail mutants are **not** CTest positives. Any future actual-source shortcut additionally requires compiled mutations of stale b/memory/elapsed, a skipped first wake, wrong FP mode, shared-bus fault and a disabled fast-branch-work witness.

## Registered work and absolute qualification gates

The prospective exact branch is **only** an exactly fixed TWS physical state, never a general quiet/audio-gated silence fabrication. If implemented, test-only instrumentation must be on *actual* full-tick versus qualified fast-source entries and lane commits, count real TRG5/control clocks, prove that every qualifying voice executes the new work and that unsupported/moving/wake cases fall back. Wrapper counters or fake claimed SIMD width do not prove work was removed; no telemetry belongs in shipping state. The already-shipped `updateSourceReadouts` uses an exact equality cache for Q/LC memoryless network algebra, so proposing another coefficient cache does not create a new optimization.

Targeted unmodified-stock idle `gprof` attributed self CPU: `tickGlobals` 17.96%, source readouts 12.86%, `tickOutput` 9.53%, mix bus 8.43%, control smoothing 7.10%, BBD 6.65%, filter EG 5.76%, rank tick 6.87%. Phase processing was almost absent. These are **instrumented sampled shares**, not production RTF or measured candidate benefit. A TWS-only shortcut cannot plausibly close the entire ~3× idle gap on its own. The original hard held target **0.15–0.20 RTF** and quiet-idle **≤0.02 RTF** remain **RED**.

Before any production-source change, run at least **three uncontended same-host baseline** source-factory rounds. Then implement an isolated candidate only after frozen/negative controls, source-site work counts and strict compatibility; acquire another exclusive window for **three serial after** rounds, preserving all 51 fixture checksum/peak/RMS, p99, deadline misses, Legacy controls and raw outliers. No optimized DSP publication, re-pin, PR6/main merge or release is authorized by this test-only registration. The independent WSC shortcut experiment remains rejected and separately archived; it is not an eligible speedup.
