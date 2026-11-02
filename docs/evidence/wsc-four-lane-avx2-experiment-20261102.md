# Four-double-lane WSC AVX2 experiment — exact oracle only

**Status: test-only arithmetic prototype, NOT a measured CPU optimization.**
This isolated branch starts at the published test-only WSC oracle commit
`58108ba7f16aec002634908f3af20a36bac907a4`, original production `src`
tree `7dd159d491012fd47b46c0aa651e86ea959ca289`. It changes only its
own branch. No `main`/PR #6 source, sound fixture, release, timing gate,
benchmark producer, tolerance or production Engine dispatch is altered.
The final *experimental* `src` tree is
`d9d2630913786722984866a3e868ac09de458d04`; modified oscillator header
SHA-256 is `f04866d70728362f6f00c2f1ebca8e397411f7edd367aa018f847fadbbb188bd`.
The frozen independent truth
`tests/reference/WscLoopOriginal.h` still hashes SHA-256
`9976f2e1a2de7ec2513c156cf70cc020911edfe486b70651c4425e97bdca4c4c`.

## Test-first history and actual observations

The committed steps are deliberately separable for audit:

1. `12ec4d7`: add an opt-in four-lane oracle executable (`CS80_BUILD_WSC_FOUR_LANE_EXPERIMENT`), and dispatch *genuinely* independent positions in groups of four through `tryTickFour`; other samples use the original scalar `tick()`. Before a backend existed, `g++ -fsyntax-only ... -DCS80_WSC_FOUR_LANE_BACKEND_TEST=1` exited **1** with `tryTickFour is not a member of Current`.
2. `c092ff4`: introduce a candidate AVX2 physical-state arithmetic method, all-or-nothing admission and scalar event/control fallback, with no Engine call site. An earlier full positive oracle without the additional rejected-admission FP assertion passed 96 cases with 3,271 admitted batches.
3. `6cfdcba`: independently enforce caller FP equivalence even on **rejected** four-lane batch probes. The uncorrected candidate compiled, then failed exactly three independent checks (exit **1**) despite otherwise identical state and output:

   ```text
   FAIL fp.mxcsr case=7 sample=83 lane=11 actual=8098 expected=8096
   FAIL fp.mxcsr case=23 sample=83 lane=11 actual=8098 expected=8096
   FAIL fp.mxcsr case=31 sample=83 lane=11 actual=8098 expected=8096
   WSC independent-lane FAIL: WSC registered lane/work/bit acceptance failed
   ```

   Bit 1 is the x86 MXCSR denormal-operand sticky flag. A speculative finite
   comparison of one subnormal control in a *rejected* batch could set it.
   This is a real adversarial negative witness, not a numerical tolerance.
4. `196c378`: snapshot the FP environment **before any** lane admission
   comparison, restoring it on every rejected path; preserve accepted
   arithmetic and every fallback. With the same GCC command and full fixtures,
   the strict test now passes with zero state, diagnostic, audio or FP-bit
   mismatches.
5. `2a44d71`: a final fail-closed x87 exception-mask guard rejects batching
   if x87 traps are unmasked, independently of the SSE mask test. This does
   not change the 96-case positive numbers (all original modes still run).
   An additional adversarial witness clones a four-lane state **proven eligible
   with normally masked exceptions**, then unmasks the x87 invalid trap
   (leaving SSE masks intact), requiring the exact same batch to be rejected
   without changing FP status or physical state:

   ```text
   WSC x87 unmasked-trap negative: eligible-masked=1 rejected-unmasked=1 fp-mismatches=0 state-mismatches=0
   ```

   This negative is sensitivity-tested by explicitly defining the test-only
   `CS80_WSC_FOUR_LANE_MUTATE_X87_GUARD=1` bypass: it compiles, executes and
   correctly **exits 1** with `WSC independent-lane FAIL: x87 unmasked-trap
   batch admitted or changed FP status`. No x87 trap is unmasked during the
   regular 96-case fixture.

Reproduce the exact candidate positive on this branch:

```sh
g++ -O3 -DNDEBUG -std=c++20 -frounding-math -pthread -Isrc -Itests \
  -DCS80_WSC_FOUR_LANE_BACKEND_TEST=1 \
  tests/WscIndependentLanePreregTests.cpp -o /tmp/cs80-wsc-four-lane-fixed-20261102
/tmp/cs80-wsc-four-lane-fixed-20261102
```

Observed GCC Ubuntu 13.3.0, x86-64, normal masked SSE rounding, default host
AVX2 support (the 96-case fixture itself also uses four FE rounding modes and
all four available FTZ/DAZ combinations):

| Exact invariant | Frozen scalar oracle | Four-lane experiment |
| --- | ---: | ---: |
| Cases | 96 | 96 |
| Widths / rates / FE rounding / FTZ+DAZ modes | 8 / 4 / 4 / 4 | 8 / 4 / 4 / 4 |
| Active / masked lane slots | 69,916 / 159,460 | 69,916 / 159,460 |
| Events / no-event ticks | 5,315 / 64,720 | 5,315 / 64,720 |
| Each copy / reset / restore / idle-wake | 96 | 96 |
| Bit-field comparisons | 178,329,376 | 178,294,626 |
| Bit mismatches | **0** | **0** |
| Successful true AVX2 four-lane calls | 0 | **3,271** |

The 34,750 lower comparison count is exactly accounted for: each of the
3,271 successful four-lane batches compares the five FP fields **once after
the batch**, instead of four times between individually sequenced ticks:
`3,271 × (20 − 5) = 49,065` fewer observations. The 2,863 rejected batch
probes now also assert the same five FP fields **before scalar fallback**:
`2,863 × 5 = 14,315` additional observations. Net difference
`−49,065 + 14,315 = −34,750`. Full 16-lane state including all 3×16
double residual slots and all 64 active/inactive event slots per lane per sample, raw
diagnostics and every pin remain independently compared as before. The
FP proof for an accepted vector call is the correct *caller-observable group
exit* (sticky flag union), rather than unobservable intermediate per-lane
states inside one call. Rejected calls must preserve FP state even before
the scalar fallback, and that assertion caught the three real failures.

`objdump -dC` on the positive GCC binary showed an out-of-line
`ReconstructedOscillator::advanceTickFourAvx2(...)` function with **5 packed
`vaddpd`, 4 `vsubpd`, 8 `vmulpd` and 1 `vdivpd` instructions** operating on
physical state. Its target attribute is `avx2,no-fma`. Thus 3,271 is not
an artificial loop counter or a mere vector load surrounding scalar ticks:
it represents 13,084 state-update lane-ticks in actual four-wide arithmetic.
The control/event scheduler, expensive `nodes()`/sine readout and residual
audio epilogue are still scalar. The caller returns false and resumes the
existing `tick()` when AVX2, rounding, mask, controls, event-cache or physical
boundary eligibility is not proven, including unmasked x87 traps. All pre-admission state changes are
transactional, including caller sticky FP status.

### Required prereg negative controls

The unmodified four existing named oracle mutations were each compiled
successfully with the same `g++` flags and their respective single test-only
definitions `CS80_WSC_LANE_MUTATE_{MASK,CROSS_LANE,EVENT,FP}=1`. Each ran the
frozen scalar oracle, **exited 1** from a named numerical mismatch (not a
compilation/fixture error), and was never accepted:

| Mutation | First diagnostic | Outcome |
| --- | --- | --- |
| `MASK` | `lane.state.charge`, case 0 sample 13 lane 0 | EXIT 1 |
| `CROSS_LANE` | `lane.state.charge`, case 0 sample 17 lane 0 | EXIT 1 |
| `EVENT` | `events.absoluteSeconds`, case 0 sample 1 lane 0, one ULP | EXIT 1 |
| `FP` | `fp.exceptions` / `fp.x87StatusExceptions` / `fp.mxcsr`, case 0 sample 19 lane 0 | EXIT 1 |

The original prereg executable, compiled without the backend test macro,
also passes and retains `vector-backend-calls=0` and its full
178,329,376 comparison count. No source comparator or registration fixture
was removed or weakened.

The CMake experiment executable is opt-in and defaults **OFF**. The header's
unused `tryTickFour` declaration is currently available to GCC/Clang x86
translation units even with that CMake option off, but there is no production
call site or runtime dispatch. The opt-in positive test explicitly requires
an x86 CPU with AVX2; on an unsupported host it fails its backend-call
minimum by design. This is **not** a portable production implementation.

## Qualification decision

**No 3-before/3-after whole-engine timing yet.** The engine still invokes
`r.wsc.tick(freq, wscControls)` once per rank/voice at
`src/core/Engine.cpp:2261`; no call to the new `tryTickFour` exists outside
the standalone test. A whole-engine benchmark of this branch would measure
the **old scalar engine**, not the AVX2 method, and any claimed CPU speedup
would be misleading. Before a later timing trial, a separately reviewed
engine-level grouping/dispatch plan must preserve cross-voice ordering,
controls, FP environment, branch-cache semantics and existing sound gates,
then prove the integrated candidate against the same full-state oracle.
The old rejected phase-SIMD experiment's ≤0.20 held / ≤0.02 idle RTF
budgets remain in force; this branch makes **no** new CPU-gate claim.
The changed experimental `src` tree is intentionally **not** the frozen
`7dd159d4` source tree; production full-audit source-pin compatibility is
therefore **not satisfied by this experimental branch**. The untouched
release/main/PR #6 trees remain authoritative for their existing gates.
