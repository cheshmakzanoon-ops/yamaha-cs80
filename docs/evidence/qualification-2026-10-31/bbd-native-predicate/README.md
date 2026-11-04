# Native Intel BBD history predicate repair

2026-10-31 (America/Halifax). This is compiler/software evidence, not a
hardware measurement. The production repair is
`1f842b9a18c117a31c09e7d4dae1aaa26c8dce4e`; its separate circuit-engine
V2 pin is `475063ca4b5640c1b17cb45eb89a23706ad18873`.

## Proven mechanism

On Apple Clang 17, the original integer `bit_cast`/magnitude-mask predicate
is optimized into LLVM `fcmp une` and x86 `cmpneqss`. With DAZ enabled,
that instruction compares a stored subnormal as zero. A BBD history entry
can therefore be incorrectly classified as empty and its continuation
discarded. The predicate must classify stored bits independently of the
caller's floating-point denormal mode.

An explicitly observed `volatile uint32_t` keeps this operation in the
integer domain. It neither changes the floating-point environment nor
changes the BBD, noise, filter or interpolation equations. Both signed
zeros remain zero; subnormals and nonfinite bit patterns remain nonzero.
There is no claim that `volatile` synchronizes threads; this local object
exists solely to prevent the demonstrated compiler rewrite.

## Independent native qualification

[Native diagnostic run 41256875634](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41256875634),
[job 124701801203](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41256875634/job/124701801203),
used the unchanged existing acceptance contract and test source. Its
artifact, compiler output, predicate probe and mutation logs are retained
here with SHA-256 provenance in `manifest.json`.

| Native variant | Original gate checks | Failures |
| --- | ---: | ---: |
| Original production predicate | 1,245 | 1 |
| Integer-observation candidate | 1,245 | 0 |
| Reintroduced floating-comparison mutation | 1,245 | 1 |

The separate 16-case predicate probe includes positive/negative subnormals
and signed zeros under all four DAZ/FTZ combinations. The original has four
misclassifications; the candidate has zero. The test restores the incoming
MXCSR. Neither a tolerance nor an assertion was relaxed.

The published repair and separate pin passed
`scripts/check-compat-gates.sh --strict-v2` at commit
`475063ca4b5640c1b17cb45eb89a23706ad18873`: legacy default and tails are
byte-identical to `d56911a`, V1 circuit numerics to `22d6a32`, and circuit
V2 audio to `1f842b9`. The complete log is `strict-compat.log`. The full
Linux core and plugin suites are still running at this evidence checkpoint;
no result is inferred from their partial progress. A passing Linux run alone
cannot substitute for the native Intel experiment above. CPU timing is a
separate checkpoint; this repair makes no performance claim.
