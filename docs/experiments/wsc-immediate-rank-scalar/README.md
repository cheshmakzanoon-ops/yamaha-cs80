# WSC immediate one-rank scalar: correctness checkpoint

**Distinct research candidate C. Bounded correctness and actual-call gates pass;
no performance or production admission, and no native-plugin qualification.**

C completes each oscillator at its original rank position. It is **not** the
four-rank scalar rendezvous S or AVX2 candidate V in the
[previous staging report](../wsc-terminal-staging/README.md). Their
[cost results](../wsc-terminal-staging/cost/README.md) are not C's costs. This
checkpoint contains no C CPU measurement or supported dynamic instruction count.
Any paired measurement/control evidence is a separate study.

## Mechanism and admission

`candidate.patch` applies to shipping commit
`e9e8175f4278d2457279fcb8809cbba1ddb7be2b`. The new, separately compiled
`EngineWscImmediate.cpp` copies the original batch-true sample body, specializes
its batch constant, and replaces exactly one same-rank WSC call. A genuine ordinary
terminal segment calls an outlined scalar node evaluator plus the original
aggregate endpoint/diagnostic/residual/output epilogue. Nonordinary scheduling
finishes synchronously too. There is no cohort, token, delayed output, profile or
residual scan, new persistent state, or movement of dependent RNG/control work.
Engine and oscillator storage layouts are unchanged.

The Engine selects C **per internal sample**, inside the original filter-batch-true
choice, only for Source Performance and the strict build/FP guard: x87 CW `0x037f`,
MXCSR controls `(csr & 0xffc0)` equal to `0x1f80` or `0x9fc0`. Sticky status is
ignored. Refusal retains the genuine original true/false choice; Pure is excluded.
The direct component API is caller-prequalified. No four-active-rank requirement
is imposed, and sparse voices are exercised. FP controls must remain stable within
a call. Scalar source does not mean scalar-only emitted instructions: GCC locally
SLP-vectorizes some output arithmetic into SSE2. There is no four-lane AVX2 kernel.

## What the retained evidence establishes

| Gate | Observed result |
|---|---|
| Direct component | 38,333,413 checks / 319,444 ticks, actual 12,063-byte stock Engine-object oracle, A/A and reverse links. Five real SIGFPE seed pairs preserve all 5,984 oscillator bytes, fault site/FP/errno and continuation. |
| Protected original code | Original true/false Engine bodies and public WSC tick retain exact bytes and resolved symbolic relocations: 37,511 / 32,154 / 12,063 bytes. No competing definitions of those methods. |
| Fresh Engine suite | Nine scenes, five arms, 81 exact comparisons; 63,532,546 public/private/FP bytes per arm. Separately, 5,555,888 ordered rank RNG draws match across 66,698,376 bytes. |
| Actual chronology | Real entry, optional terminal, same-rank return and following audio RNG draw checked in order. Each factory/Panel held/moving late window has 262,144 entries and terminals; sparse-three has 98,304. Pure/Legacy/quiet have zero. |
| Restored/negative controls | Twelve seeds pass. A real exact-dt reset-end gives 32 immediate entries but only 31 ordinary terminals; mixed, nonfinite/current/future/elapsed state, custom profile and sparse controls pass. Unmasked x87, downward and Pure first calls enter C zero times. |
| Public-render traps | 22 contexts, seven genuine in-DSP signals including CSR `0x0f80`, partial state and four masked continuation frames: 17,019,552 exact bytes/arm for stock, A/A, normal and reverse C. |
| Independent reconstruction | All 196 candidate files reconstruct exactly; clean compilation of all five core objects reproduces every object and the complete normal archive. |
| Directed idle/BBD | 16 cases, 2,064 callbacks, 768 observed idle and 1,032 BBD callbacks; stock/normal/reverse match 64,267,367 bytes each. |
| Full intended fixture | Four scenes, 12 untimed C invocations. Both diagnostic arms match immutable stock full PCM/FP/route/voice traces, 1,438,632 bytes per scene. Complete 512-block windows have 1,048,576 actual entries/terminals in factory held, factory moving and Panel moving; Pure has zero. |

The Engine suite uses 48 kHz, 64-frame blocks, 2x internal processing, drift 0.37,
dispersion 0.18 and frame-before-MIDI every block. Its nine scenes have 385 callbacks
each. The separate full fixture has 1,024 warm blocks, 512 window blocks and one
continuation; warm/continuation counts are excluded from the window. Normal core
archives have no hooks. Counted/uncounted comparisons test diagnostic
noninterference in those workloads. The direct suite's eight sample rates and
custom profiles do not substitute for broader Engine coverage.

## Important adverse evidence and limits

Only **48 bytes of final Nodes rematerialization** disappear. Another 112 bytes
remain; all 160 endpoint bytes are still committed. This is not 48 bytes less total
traffic, a dynamic instruction reduction, or a CPU win. A real nested call replaces
the predicted tail call, adds a helper frame/coefficient spills, and total code
size grows. Stock already feeds many endpoint stores from registers.

The optional local dynamic work-count attempt stopped at feasibility: no supported
tracer/counter was available (`PTRACE_TRACEME` returned `EPERM`; the disabled
instruction event returned `ENOENT`). No bypass, synth execution or substitute
timing followed. This is not a performance rejection or acceptance of C.

The new Engine TU shares 16 weak helpers. Fourteen have matching raw bytes, not a
blanket resolved-relocation proof. `idleStepRank` differs (4,615 versus 7,149 bytes),
and BBD raw code differs at equal length. Directed normal/reverse tests pass, but
arbitrary link order, flags, compiler or LTO equivalence is not established.
Private traces cover the declared fields, not every Engine byte. No universal
block-partition, all-feature, concurrent mutation, native/DAW or full CTest claim
is made. Earlier harness-only compile errors had no DSP execution and did not
cause candidate source changes.

## Portable offline verification

```sh
python3 -B verify.py
python3 -B -m unittest test_verify.py
# Optional: authenticate local stock src and reconstruct/check C in memory.
python3 -B verify.py --stock-src /path/to/shipping-checkout/src
```

The Python standard-library verifier checks package hashes, receipt bindings,
scopes and count relationships. The optional source check authenticates all 193
stock files, applies the patch strictly without fuzz, and checks all 196 candidate
hashes. It writes no source, compiles/renders nothing and uses no network.
Corruption tests exercise the verifier, not the DSP. They cannot independently
prove omitted raw captures or guard against coordinated replacement of both
records and verifier; the externally retained manifest hash is the package anchor.

`provenance.json` identifies omitted original receipts, objects and executables.
`records/` contains compact, scope-labelled results; source paths in the inventory
are relative to `src`. Binaries, raw audio/state/RNG captures, full test harnesses,
full disassembly and a duplicate source tree are deliberately not bundled.
The normal archive is
`eeba3a3b635b05f8f7eb797564a26511ce738291acf6c60b5d5ba915e79869a0`;
the direct implementation is
`0beda1833b75bc9986abb958063954e73576ffe2d7a07afa90b58de41f206cf8`.
GNU C++ 14.2/Linux x86-64 and the recorded build/links bound these results.
