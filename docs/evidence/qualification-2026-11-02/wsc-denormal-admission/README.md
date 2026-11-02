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
