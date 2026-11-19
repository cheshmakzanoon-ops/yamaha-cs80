# Borrow a held oscillator's previous right node without copying it

The source-selected rank renderer can borrow its preceding, already-committed
endpoint read-only when the frequency, threshold and two gates have identical
raw bits and the engine can prove ordinary supported control and FP
conditions. PANEL/memory ranks skip the new eligibility check. No input
range, event root, segment integration, output filter, diagnostic policy,
sample rate, or active/idle circuit clock changes. A new endpoint remains
local until the same final completion. The original and optimized
entries use one canonical event scheduler and output calculation.

The guarded GNU 14.2 Linux x86-64 path requires the CMake-linked build,
ordinary masked floating-point controls, supported precision and rounding,
an initialized endpoint, and supported bounded finite controls. Other
builds, changed controls, interrupted/fault environments, and standalone
Engine.cpp consumers retain the original code. The known-old stock scalar
oscillator tick, event leaves and resolved operands remain independently
checkable. The new implementation symbol is not a plug-in entry point or
state/parameter API.

## Tested whole plug-in work

The actual default stock and candidate Linux Release VST3s compare exact
main `7f7b7a1` and the one change. The fixed work window is four native
host callback boundaries translated for deterministic counters, at 48
kHz/64 frames. No callback clock is used for the comparison.

| Scene | Guest instructions, default | Guest writes, default |
| --- | ---: | ---: |
| Eight equal held notes | −2.987% | −14.688% |
| Eight varied held notes | −2.964% | −14.686% |
| Varied wake | −2.179% | −10.788% |
| Fresh quiet | −0.105% | Unchanged |

Duplicate and independently counted fifth-callback controls match. The
separate long standalone source-core windows qualify the mechanism and
bound active fallback work. Neither diagnostic is an elapsed hardware
CPU, DAW, deadline or energy measurement.

An earlier **unshipped** compile-time filter-batch-disabled prototype
([research work](no-batch-work.json)) used 3.042% fewer guest instructions
for equal held, 3.019% fewer during varied held, 2.243% fewer at wake, and
0.067% more in fresh quiet. Its tested interruptions matched, but the
generated stock-fallback tick body was not the historical body. The
sealed change therefore compiles out this optimization when the filter
batch is explicitly disabled at build time. This is distinct from
runtime selection of scalar filtering in the normal release on hardware
without AVX2.

- [Normal Release work](guest.json) reports both arms of the actual default
  VST3 measurement. The wider public 48 kHz VST3 protocol independently
  passed both modules and produced byte-identical reports.
- [Direct oscillator cases](direct.json) compared 114,688 pairs and
  573,467 checks. Disabling the held-control equality protection was a
  negative control, not an admitted source variant.
- [Real loaded-module fallback](traps-native-modules.json) compares the
  17,472- and 67,480-case interruption matrices and continuations.
- [Scalar machine code](scalar-machine.json) records equality of the stock
  fallback oscillator tick, event code and their resolved operands.
- An independent normal CMake Release core built the guarded translation
  unit and renderer objects byte-identically to the ones tested. Direct
  Engine compilation without the CMake definition deliberately keeps the
  previous link interface and passed the 4,555-check Controller Reset test.

All seven extended 48 kHz VST3 histories matched (112,640 callbacks per
arm); the eight additional 44.1/88.2/96/192 kHz rate/mode groups matched
across the recorded PCM, public state and continuations. The focused
[frequency and flag boundary checks](edges.json) compared 8,640 pairs
across 54 populations. A deliberate omission of the original bound
evaluation when the inexact flag was still clear failed on all 1,296
reachable clean-to-dirty checkpoints.

The new portable CTest compares source-admitted and direct scalar
oscillator output, public state and every diagnostic event slot. In the
qualified Release, 2,372 out of 3,072 test ticks took the borrowed path;
the explicitly disabled build completed the same 3,072 ticks on the
scalar path. Changed and unsupported controls are present in both cases.

The [sealed compile-time-disabled build](explicit-disabled-build.log) is
ELF-byte identical to the previous stock software scalar comparator.
The final normal candidate remains the already-tested ELF
`755ce5cd20a0228fd5c4b2c0772c1d6967d7e6898a158fc2e2554564dfd34b1d`.
[Other compiler configurations](profiles.json) check generated unchanged
fallback leaves on enabled builds and whole Engine objects where the
feature compiles out.

The [admitted exceptional-state probe](poison.json) compared 28,512
ticks and all named observations at each of GCC O1, O2, O3 and Og.
It covered restored states with injected exceptional values and observed
344 signaling-NaN invalid events per profile without an arm discrepancy.

The machine-readable [long-history receipts](native-long.json) and
[other native rates/modes](native-cross.json) bind those observations.

In a distinct software diagnostic using the normal compiled core and
turning off filter batching at runtime (rather than compiling it out),
the real engine held cases used 2.80–2.81% fewer guest instructions in
short windows and 3.17–3.33% fewer in long windows. PANEL work increased
0.102%. These are core-only counts, not measurements on a processor
without AVX2 or an optimized physical DAW result.
