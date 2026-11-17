# Production-PIC PGO: compatibility evidence, terminal calibration failure

[Study 44078082712](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/44078082712), first attempt at `2101bc343394e1da54236b16fe9cc6f8ce5fe8f2`, completed with `TERMINAL_UNUSABLE_CALIBRATION`. Workflow success is not release qualification. Artifact `14326774302` ZIP SHA256 `b2974e2fa8307ce72d1e2d9227ab9d5287b3cfed4c2eb9864be18c795e4f8eeb`.

## What passed

Pinned GNU13.3 integer CMake/profile-path prerequisite; real production-PIC baseline/candidate builds; exact preliminary Engine and real-plugin compatibility. Both arms match across 151 held-out rows, 66,428,928 PCM bytes, external FP transcripts, five core-test logs, 22,400,288-byte filter state corpus, and 462 Sub/Ring host captures (15,138,816 bytes) per arm. These are scoped compatibility checks, not the complete release suite.

## Why the experiment stopped

Independent audit recomputed all 600 raw A/A rows. The first two baseline-versus-baseline pairs passed. The third pair failed the predeclared individual-row limit: Legacy paired tones, selector 10, release tail, ratio `0.6180998694459842`, outside `[0.9, 1.1]`. Raw callback CPU totals were 0.055071458 and 0.034039661 seconds for identical 173-callback work. Aggregate gates passed, but that does not override the failed row gate. No cause for this variability is established.

The runner was Linux x86-64 on an AMD EPYC 7763, with GNU13.3 production flags. Affinity was checked; it does not prove exclusive host CPU access. Zero candidate-cost runs were started. There is no calibrated production-PIC speedup estimate, and the earlier standalone Xeon result cannot supply one.

## What did not run

The complete 569-test release gate, demo/UI checks, three pluginval validations and packaging were conditional on a scoped cost pass and therefore did not run. `release_qualified` remains false. Production defaults and DSP sources were not enabled or changed by this study. No DAW CPU, Windows or macOS inference is justified.

This terminal study is closed. No retry, relaxed threshold, excluded row or retrospective reclassification is permitted. Retained actual artifacts and independent audits preserve the useful compatibility evidence without turning it into an optimization approval.
