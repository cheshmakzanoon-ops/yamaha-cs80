# WSC denormal-domain admission: test-first RED

This extends an **isolated, uncalled AVX2 experiment**, not production Engine
code. The parent `9785f9e5d3d4290e450f12db9597bb211aecfa9f` correctly falls back
for unqualified denormal modes. The new test exposes missing intended vector
coverage, not a previously demonstrated numerical defect.

Two independently inspected facts motivate the new registration:

1. The unchanged stock benchmark enables FTZ+DAZ (`MXCSR 0x8040`), while the
   experimental backend rejects either bit. Therefore that backend, even if
   wired unchanged into Engine, could not accelerate the stock workload.
2. The original width-4 FP matrix presents lanes `(pos*3+sample)%16`. Its active
   indices 0..3 never form four contiguous presented lanes. The old aggregate
   admitted-call count came from wider, nearest/gradual-underflow sweeps; it did
   not establish vector work in those FP cells.

## Preregistered extension

`tests/cpu/wsc-denormal-admission.json` freezes the intended domain before any
backend edit. All original 96 x86 cases remain. An additional 64 full-16-lane
cases cover every rate × rounding × supported FTZ/DAZ combination. Each cell
must make at least 100 admission attempts; each nearest cell must admit at least
100 batches; other rounding modes must admit zero. Added cells also inject
bit-constructed signed minimum/maximum subnormal residuals and signed zeros.
Each nearest cell must actually **consume** a subnormal at its current residual
cursor in an admitted batch, not merely hold one in a future ring slot.

All frozen output, physical state, 48 residual slots, 64 active/inactive event
records and FP environment comparisons remain exact. Rejected attempts retain
caller FP state; original copy/reset/restore/idle/wake and x87 trap probes remain.
The reference header SHA-256 remains
`9976f2e1a2de7ec2513c156cf70cc020911edfe486b70651c4425e97bdca4c4c`.

## Before-source result

Compiled serially under the shared heavy-work lock with Debian GCC 14.2.0:
`-O3 -DNDEBUG -std=gnu++20 -frounding-math -Isrc -Itests`.
The candidate build additionally defines `CS80_WSC_FOUR_LANE_BACKEND_TEST=1`.

| Control | Cases | Field comparisons | Mismatches | Result |
| --- | ---: | ---: | ---: | --- |
| Original scalar positive | 160 | 280,880,288 | 0 | exit 0 |
| Untouched parent AVX2 backend | 160 | 281,201,628 | 0 | expected exit 1 |

The parent admitted 4439 nearest/gradual batches and consumed stored subnormals
there. In each nearest FTZ, DAZ or FTZ+DAZ rate cell, 1226 attempts admitted zero
batches. It failed the newly registered
`WSC denormal-mode actual-admission work gate failed` assertion normally.
The x87 unmasked-trap negative probe continued to reject transactionally.

Raw logs, hashes and the actual collector command are retained. The merged
stdout/stderr RED log contains one interleaved error message; neither content
nor failure status is omitted. No production source was changed in this
registration/evidence commit. No CPU speedup, physical fidelity or release claim
follows from it. Hard held ≤0.20 RTF and idle ≤0.02 remain unchanged and RED.

## Narrow experimental extension and adversarial controls

After the RED registration, the candidate removes **only FTZ/DAZ bits** from the
admission rejection mask. Independent SSE rounding, all masked SSE exceptions,
all masked x87 exceptions, full lane eligibility and rejected-admission FP
rollback remain. Caller denormal modes are never changed. Scalar `tick()`, node
laws, coefficients, event handling and Engine scheduling are untouched.

The extended candidate passes 160 cases, 281,092,938 field comparisons and zero
mismatches, with 7943 admitted batches. Each nearest FTZ/DAZ cell admits 284–303
batches, including 31–36 batches consuming nonzero subnormal residuals; every
other rounding-mode cell admits zero. The unchanged x87 negative probe still
rejects unmasked traps transactionally.

`run-wsc-denormal-negative-controls.py` compiles independent source copies
serially, never overwrites existing evidence, and accepts only the specified
ordinary exit status/assertion (not a compile error or signal). Twelve variants
were observed:

- Scalar and experimental vector positives: exit 0.
- Reinstated old denormal guard and disabled dispatch: exit 1 at per-cell work.
- Skipped physical vector update and cross-lane frequency: exit 1 at exact state.
- Removed x87 guard: exit 1 at the unmasked-trap admission assertion.
- Original MASK, CROSS_LANE, EVENT and FP mutants: their original named failures.
- **Forged scalar work:** four scalar physical advances preserve all numerical
  outputs and admitted-call counts (exit 0), but the independent compiled-work
  inspector rejects the binary with ordinary exit 1. Thus an accepted-call
  counter alone is demonstrably insufficient.

The actual candidate contains a called `advanceTickFourAvx2` with five YMM
`vaddpd`, four `vsubpd`, eight `vmulpd` and one `vdivpd` instructions. This
instruction witness is paired with runtime per-cell admission and frozen state
checks; it is not itself a throughput result. The unchanged x87 probe is run
*after* the new per-cell work gate so a disabled backend fails its intended
work assertion instead of the probe's eligible-positive precondition.

Detailed commands, compiler/binary/source hashes and separate stdout/stderr for
all twelve variants are retained under `controls/`. No production Engine caller
or whole-engine acceleration claim has been added.

## Unchanged oscillator regression gates

After the extension, the original standalone gates also pass without tolerance
or source-reference edits:

- Frozen original scheduler: 56,576 ticks, 44,294,052 bit comparisons, zero mismatches.
- Cache continuation: worst pin difference `3.39565708884e-10` V and event-time
  difference `2.38697950294e-15` s; snapshot restoration exactly zero difference;
  307 analytic searches per 8192 samples, within the unchanged work bound.
- Physical oscillator suite: 1,418,466 checks, zero failures. Actual 255-tap 2×
  decimator off-harmonic energy is −92.3189 dBc against its unchanged −70 dBc gate.

These exercise the unchanged scalar oscillator and guard against unrelated
regression. They are not a new full-Engine SIMD/alias or CPU qualification; the
new vector domain's continuation claim comes from the independent exact oracle.
`run-regressions.sh` uses the original CTest arguments, including `no-benchmark`.
