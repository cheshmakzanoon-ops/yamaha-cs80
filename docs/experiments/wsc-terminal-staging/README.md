# WSC terminal staging: research checkpoint

**Research checkpoint. No performance or production admission; no native-plugin qualification.**

The frozen V1b integrated correctness gates and complete 216-slot cost study are recorded here. The outcome is **no performance admission in this environment**; see [the cost evidence](cost/README.md).

## Three distinct arms

| Arm | Meaning | Evidence in this cut |
|---|---|---|
| **B** | Untouched shipping `e9e8175f4278d2457279fcb8809cbba1ddb7be2b` | Primary oracle. |
| **S** | Original-position **scalar** terminal staging, scalar V2 | Five-arm, nine-scene exact trace/RNG comparisons; reverse-link idle/BBD and mixed/fallback controls. Not vector work. |
| **V** | S plus the genuine AVX2 terminal-node kernel and render-call-local binding, integrated **V1b** | Exact critical-function isolation; 81 integrated nine-scene comparisons; actual vector-return/consumption witnesses; supplemental lifecycle/mixed/reverse and public-render trap checks. Cost does not meet admission. |

Apply `stock-to-scalar.patch` to B, then `scalar-to-vector-v1b.patch` to S. These are exact frozen normal-candidate patches, not evidence hooks. No duplicated source tree or production build integration is included. `provenance.json` binds both patches, changed/compiled sources, normal archives, component sources, scalar trace executables, and completed vector driver/observer/executable hashes.

## What passed, within scope

- **S:** 81 exact comparisons across nine scenes; 253,646,285 public/private/FP bytes per arm. The separately observed ordered rank-RNG association covers 21,661,858 draws and 260,052,960 raw bytes. These are **scalar** results. Each held/moving scene has 262,144 actual late four-rank scalar terminal groups; negative scenes have zero admission.
- **Numerical component:** 2,354 four-owner cohorts /188,320 node-field comparisons, 30 transactional rejections and 192 preparation/CPU-OS-control cases. Five intentional mutants are caught, including a scalar impostor that passes numerical comparison but fails the packed-work check.
- **Standalone optional vector finish:** 9,414 actual kernel attempts, 9,411 true returns, three real rejections; 42,772,633 checks. A poisoned successful record fails at raw diagnostics, demonstrating consumption rather than scalar recomputation. These are **component**, not Engine workload, counts.
- **Integrated V1b full traces:** all 81 comparisons pass, covering 253,646,285 public/private/FP bytes per arm and the same 21,661,858 ordered rank-RNG draws. Each late factory held/moving and Panel held/moving scene has **262,144 actual successful vector calls and 1,048,576 consumed node records**; Pure and the other declared negative scenes have zero calls. Six semantic cases match 923,244 bytes/arm; block/owner/copy-reset cases match 124,904,389; directed-rounding idle/BBD controls match 64,267,367. Block tests show arm parity, not universal partition invariance: stock and every other arm retain the same partition-dependent differences.
- **Integrated V1b public render:** 22 cases, seven genuine in-renderer SIGFPE cases, partial-state and four-frame continuation. Normal and reverse links match B/S in 17,019,552 bytes per arm and normalized original trap sites/FP/errno. This separate trap fixture does not itself count successful vector groups; the preceding integrated trace provides that witness.
- Original Engine scalar and batched bodies (32,154 and 37,511 bytes), and public WSC tick (12,063 bytes), retain exact bytes and symbolic relocations. The imported numerical-kernel object is unchanged.

Eligibility, an ever-deferred mask, actual kernel success, consumed output records, and a measured benefit are different claims. None is substituted for another.

## Revisions retained honestly

- Header-body terminal V1 fails a directed-rounding idle/wake case even through its unchanged public-tick control. The corrected same-stock null passes. This is a compilation-context qualification failure, not a proved split-scheduler defect.
- Earlier complete-tick staging V1a passes its bounded Engine-first tests but emits a competing weak public tick. It is not S and not an arbitrary-link guarantee.
- Out-of-line direct terminal V2 and original-position scalar cohort V1 pass separate component screens. The original-position form, not the direct four-input gather API, underlies S.
- Combined vector V1 changes the original batched fallback from 37,511 to 37,927 bytes. Its independent observer was prepared but **not built/run** after that static stop. V1b isolates the public render wrapper in another TU and restores the critical-code gate. V1 evidence is not silently relabeled V1b.

## Replay and omissions

Run `python3 -B verify.py` and `python3 -B -m unittest test_verify.py`. They only check small saved receipts, arithmetic/count relationships, source/patch bindings, scope labels and package hashes. **They do not replay DSP or independently prove omitted raw traces.** Twenty negative tests reject corruption, wrong counts/associations, missing integrated gates and unsupported admission claims.

`render-traps.cpp`, its fixed diagnostic observer and `reproduce.py` provide a portable, separately invoked public-render probe. See `METHOD.md` before compiling. Preparing this package performed no build, DSP rerun, timing, native load or network action.

Not bundled: binaries, raw audio/state/RNG streams, giant disassembly, the full nine-scene harness/dependencies, or native module artifacts. Cost data and replay are separately packaged under `cost/`. Saved hashes identify omitted artifacts; they are not download links. The patched normal source is reconstructible from the baseline and two patches. This remains bounded GNU 14.2/Linux x86-64 research, not a full CTest/native/DAW, cross-compiler, LTO, arbitrary-link or speed claim.

## Cost disposition, not a vector speed claim

The descriptive **S-versus-B scalar-staging** estimates are about **9.5% lower CPU for factory held** and **6.7% lower for factory moving**. They are not incremental vector gains and are not admitted improvements. V is slower than S in those contrasts; V versus B is only about 3.1% lower for the primary held case (95% interval 1.83% to 4.37%). The **predeclared noise-adjusted rule** yields a 21.48% superiority floor for this run; primary and two secondary gates fail. All 216 slots are retained. Detailed contrasts, limits, raw data and offline replay belong to [cost/README.md](cost/README.md), not to the component occupancy counts.
