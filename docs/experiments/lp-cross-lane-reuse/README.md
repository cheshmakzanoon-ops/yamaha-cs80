# Same-pass LP coefficient reuse, V2

**The whole-core CPU gate did not pass. No production promotion or native plugin run.** The sole primary, factory-moving, measured -1.29% standalone CPU reduction (95% whole-pair interval -10.03% to +4.13%; primary floor 11.71%). Controls also had to clear their predeclared −3% lower bounds. See the [complete 256-observation screen and all four scenes](cost/README.md). Positive values mean less CPU; negative values mean more. Helper hits alone are not CPU evidence.

Baseline: `e9e8175f4278d2457279fcb8809cbba1ddb7be2b`. The exact normal-source [candidate.patch](candidate.patch) has SHA256 `0979c5279ecb5d7dbcdb2c4791c806c18d8d8ed93e9ddcd81d9b7927a975e8b4`. [Provenance](provenance.json) and [normal-arm receipt](evidence/normal-arms.json) identify the inputs, reconstructed patch and archives. The 193-file source comparison found only `src/core/Engine.cpp`, `Ig00156Batch.cpp` and `Ig00156Batch.h` changed. `Engine.h` and `Ig00156Filter.h` remain byte-identical to baseline.

## What changes, and what does not

The existing exact-batch Engine scheduler prepares independent filters within the current internal sample. V2 adds a transient memo only to its active LP preparation pass. A completed original full refresh can donate ten Q-independent coefficient doubles to a later eligible stage in that same pass. The recipient recomputes its own Q-dependent values with the original expressions. It does not copy another voice's histories, nodes or slopes.

Admission is deliberately narrow: ordinary positive inputs, the actual standard LP network, identical constructor-derived fields, supported rate (8 kHz through 1.536 MHz), conservative raw-cutoff saturation (`cutoff >= sampleRate/2`), matching active clocks, valid destination coefficients, nearest rounding and masked SSE exceptions. Current MXCSR controls must match the donor's and sticky status must contain its post-generation flags. Same raw cutoffs and Q-only updates cannot become donors. Rejection performs no FP arithmetic or stage writes; the original path remains responsible for the operation.

Crucially, V2 **always invokes the unchanged original `prepareStageClock` call**, with its original cutoff/Q arguments, even after a reuse hit. A hit has realized the correct raw metadata and coefficients, so the original setter executes its own Q clamp and key-return logic. A miss follows the ordinary refresh and may capture its completed result. Clock/restore transitions retain original handling. There is no persistent Engine or per-stage cache, Stage/Engine layout growth, cross-sample queue, new latency, Newton change, approximation, relaxed tolerance or fast-math flag. Existing unsupported-build/scalar/idle paths remain outside this optimization.

This preserves the existing discrete model; it is not new evidence of analog-model accuracy.

## Why V1 was stopped

V1 skipped the original preparation invocation on a hit. Although its scalar source branch was untouched, GCC changed the non-batch Engine body from 32,154 to 39,040 bytes and changed resolved relocations. It failed the predeclared isolation gate before candidate runtime qualification or timing. That is a structural failure, not a demonstrated audio or trap mismatch.

V2 retains the invocation. Its non-batch body is exactly 32,154 bytes with the original SHA256 and all 646 resolved relocations equal. See [V1 comparison](history/README.md) and [V2 isolation receipt](evidence/scalar-isolation-v2.json). No large disassembly is included.

## Recorded untimed checks

These are retained results, not tests rerun while exporting this folder.

| Gate | Recorded result |
|---|---|
| Direct component/state/FP | 88,982 checks, zero failures; 339 hits, 1,696 misses, 524 donors; two intentional corruptions detected |
| A/A oracle control | 86,886 checks, zero failures; no target-arm hits |
| Compile-disabled backend | 88,704 checks, zero failures; no hits or donors |
| Trap/partial-state fixture | 17 cases, all A/A and A/B equal; 10 baseline cases actually trap; one masked positive-hit case |
| Four source mutations | All detected: 169 / 4 / 1,683 / 2 failed assertions respectively |
| Standard `Ig00156BatchTests` | 1,183,425 checks, zero failures, 15,685 accelerated groups |
| Standard `FilterBatchEngineTests` | 4,135,998 checks, zero failures, 285,328 stereo frames |
| Standard `FilterBatchSchedulingTests` | 869,465 checks, zero failures, 135,454 timeline frames |

The component fixture compares initialized complete stage representations, raw keys, continuation outputs, MXCSR, x87 status/control and errno. Its grid includes rounding and FTZ/DAZ combinations, seeded/cleared flags, boundaries, wrong networks/derived values, aliasing, restore/reset and stale raw/Q-only donors. The trap fixture records original partial state and signal context in isolated child processes. It compares actual precision, invalid, divide-by-zero, denormal, overflow and underflow cases; it is not merely a no-hit counter test. The four source mutations remove sticky eligibility, permit raw/Q-only donors, corrupt a shared coefficient by one ULP, and remove derived-field identity.

See [direct/control receipt](evidence/direct-controls.json), [trap log](evidence/logs/traps.log), [mutation receipt](evidence/mutations.json) and [focused-test receipt](evidence/focused-tests.json). The three project tests are **not the full CTest suite**.

## Normal-core traces and separate positive-hit witnesses

Ten scenes compare normal stock, normal V2 and a separately counted V2 executable. All 20 recorded streams match bit-for-bit, totaling **94,002,480 bytes per arm**. They cover PCM/public readouts/MXCSR plus explicit fields for all 32 HP/LP stages and x87/errno observations. Capture includes the onset and late windows and sparse observations between them, not every warm-up block. Eight scenes retain the historical sparse control-frame schedule; two additionally apply the complete frame before MIDI on every block.

Actual late-window counted hits include:

| Scene | Hits |
|---|---:|
| Factory moving, historical schedule | 838,176 |
| Factory moving, frame-before-MIDI | 838,176 |
| Release/wake/policy/rate transitions | 166,246 |
| Factory held, panel held/moving, Pure held, Legacy held, quiet controls | 0 |

Factory-moving windows contain 1,048,576 helper calls. Hits are not the number of all render operations and must not be translated into a CPU percentage. Onset hits in held scenes do not establish steady-state savings. The [trace receipt](evidence/trace-report.json) retains all hashes, scene counts and comparisons with historical stock where applicable.

The separately prepared four-scene cost fixture also passed its **untimed** identity/diagnostic gate, with matching normal driver object bytes and no counter symbols in ordinary arms. Its compact [receipt](evidence/cost-fixture-qualification.json) is not timing evidence.

## Instrumentation boundary and limits

The normal patch contains no census counter, trace writer or observer call. [Evidence-only hooks](evidence-hooks/README.md) use a counted-only linker wrapper around the real helper. Trace-private inspection is enabled only for the fixture translation unit, not the core objects. Normal and counted captures agree; ordinary timing arms must not link the wrapper or enable diagnostics.

Qualification is bounded to the recorded GNU x86-64/Linux builds and fixtures. It is not proof for every accepted custom state, arbitrary x87/trap environment, compiler/native flags, platform, final plugin link, DAW, audio driver or deadline. No universal FP-behavior or performance claim follows from these receipts.

## Export and reproduction

[BUILD.md](BUILD.md) describes the explicit inputs and portable sources. C++ fixtures and the normal candidate patch are unchanged copies. The mutation/isolation Python tools are path-portable editorial adaptations, not newly runtime-qualified implementations. Original receipts with paths were reduced or sanitized, with their original digests recorded where useful.

Executable/object/archive files, raw PCM/state/trap records, giant disassemblies, machine-local build paths and arbitrary source maps are intentionally omitted. Hash receipts identify omitted artifacts but cannot replace their bytes for a fresh independent replay. The pinned baseline checkout, compiler/toolchain and a new output directory are needed to regenerate them. Exporting this cohort performed no compilation, DSP execution, native module loading or timing.
