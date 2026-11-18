# Isolated production modern-mirror method cost, v1

This is a distinct method microbenchmark, not a rerun of any whole-Engine campaign. No timing is authorized by this plan. It uses the genuine uninstrumented native `ModernLayerState::mirrorToEngine` object, the normal native compiler/options, and the unchanged stock core. Candidate V1 only; V2 is rejected and excluded. Native wrapper objects precede caller objects in the link, and the map must attribute the selected original/fallback `ModernLayer::setConfig` to the native wrapper, not to a caller-emitted weak copy. No LTO, algorithm surrogate, source timer, altered sanitizer, or whole-plugin render is in the timed region.

## Scope and state

Each fresh process independently constructs two Engine pairs: a main pair representing the real sixteen-voice extension and a separate same-state known-work shadow pair. Each Engine has eight held notes, 48 kHz / 64-frame full controls and 16 genuine warm rendering callbacks. All preparation, configurations, transports, rendering, identity checks, PCM continuation and output serialization are outside timing.

Two fixed scenes: `default` uses canonical default modern shapes/empty slots; `dense` uses three explicitly specified 32-point canonical shapes, all sixteen routing slots and nonzero amounts. The exact values live in `src/host.cpp`. A genuine post-render mirror is executed before the measured loop. The loop then calls the actual method **131,072 times** on the same already-mirrored owners. It measures **settled, hot repeated-mirror method + loop/call cost**, not callback-separated execution. In particular transportSamples is zero during this loop, unlike a normal first post-render call. There is no timing claim for edits, fallback, cold caches or native audio callbacks. Those remain separate correctness qualifications.

The `extra` control also calls the same actual stock method on the separate warmed shadow once per eight main calls, exactly 16,384 extra calls. All regular calls and genuine rebuild/transport behavior remain. Loop bounds and direct calls must be verified from the final linked machine code; the loop itself is uninstrumented, not falsely described as a runtime call-counter oracle.

## Fixed one-shot schedule

Two scenes × ten independent ABBA batches × four fresh processes = **80 slots**. Each scene has three exact-identical-stock shams, a known-work control in each direction, and five stock-versus-candidate batches. The fixed role sequence is sham0, compare0, extra-A, compare1, sham1, compare2, extra-B, compare3, sham2, compare4. Scene order rotates by round. Initial ABBA/BAAB order alternates by scene index plus replicate ordinal within each role class, so shams and comparisons contain both orientations and positive directions are opposite. `plan.json` serializes every slot before clocks.

A/B are neutral collection labels. A sham maps both labels to the **same physical stock executable**, not independently optimized variants. Comparison maps A stock/B candidate. Positive maps both stock, with only the designated label receiving extra work. The collector does not print outcomes or calculate effects during the run. The fixed binding is unblinded by the analyzer only after all 80 slots finish; no claim of concealment from an investigator who opens the sealed plan is made.

Pin CPU 0. Each process has 3 CPU / 5 wall seconds; whole one-shot 120 CPU / 180 wall seconds. One attempt only. Any interruption, timeout, artifact/identity/clock failure or exhausted budget consumes the whole trial. Do not stop for an unfavorable numerical result, exclude anything, resume, replace slots or rerun. Preserve all observations.

For each process, three empty clock pairs precede one full timed method loop. Read order: thread CPU begin, monotonic wall begin, actual loop, wall end, CPU end. No timing overhead is subtracted. Thread CPU is primary; every wall interval is reported. No callback/iteration is an independent replicate: the independent comparison unit is its fresh-process four-slot ABBA batch.

## Frozen interpretation gates

Correctness/provenance and complete valid records are mandatory. Using per-process CPU ns/main-call (including any designated extra work), each batch contrast is mean(log B) minus mean(log A), with two observations of each. All calculations keep every slot.

- Each sham batch has absolute signed apparent effect `abs(100*expm1(log B/A))` at most 3%; each scene's mean-log sham effect has magnitude at most 1.5%.
- Each positive, oriented extra/ordinary, has ratio between 1.08 and 1.18 inclusive. Both directions must pass; neither is a candidate result.
- Each process's largest empty-clock CPU duration is below 0.5% of its main interval, and the mean below 0.1%. Never subtract overhead.
- If any control gate fails, candidate measurements are descriptive only and no reliable isolated cost difference is admitted. No revised floor, filtering or repeat may rescue it.

Report every process and batch, median/range across the five independent comparison batches per scene, absolute ns/mirror, and signed ns difference. Arithmetic scaling at 48 kHz/64 is **750 mirror calls per second of audio**, so ns difference ×750/1e6 gives ms per second of audio *under this hot-method assumption*. It is not measured full-plugin CPU%, a DAW speedup, deadline margin, or extra polyphony. Preserve this limitation even if the isolated effect is large.

Numerical percentage/ratio comparisons use a fixed 1e-10 floating-roundoff allowance; strict overhead comparisons use exact integer products. This allowance is not a measurement tolerance.
