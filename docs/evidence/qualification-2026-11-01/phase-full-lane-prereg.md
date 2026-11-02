# Preregistration: full nonlinear IG00156 independent-lane phase kernel

2026-11-02 · **test only / source DSP unchanged / not a performance claim**

## What is pinned

- Independent scalar source baseline: feature documentation commit `3236c0cff5f6330fb1c6c44744f5f6b2d56ed24f`; `src` tree `7dd159d491012fd47b46c0aa651e86ea959ca289`.
- Frozen source manifest: `tests/fixtures/exact_scalar_kernels/manifest.json`, original `Ig00156PhaseCorrection.h` SHA-256 `26d7919d1dc8a1fe210d2bbc47798354b804476eaf6452221d51ef85c8171947`, independently frozen in the earlier exact-scalar contract. The new driver includes the **unmodified** `ExactScalarFilterTests.cpp` and its independently namespace-materialized math dependencies; it does not regenerate frozen code from a future candidate.
- Executable: `cs80_phase_lane_oracle_tests`, source `tests/PhaseLaneOracleTests.cpp`, precise prospective contract `tests/cpu/phase-full-lane-batch.json`.
- Neither the original full scalar reference, the phase DSP, the CMake compiler flags, nor existing compatibility hashes were changed in this preregistration.

## Measured hotspot; test motivation, not sound-law evidence

Targeted stock-source `gprof -pg -O3` of the warmed eight-voice held `String 1`/`String 2` fixture (after 1 s warmup, 2,048 rendered callbacks) attributed **29.22% self sampled CPU** to `Ig00156PhaseCorrection::process` over 36,634,624 process invocations, **34.39%** to WSC `tick` over 18,317,312 calls, and **14.69%** to `Engine::tickRanks`. `PhaseCorrection::set` contributed only **0.17%** after warmup. These are instrumented sampled shares, **not production RTF** and not proof that a proposed batch is faster.

The rejected WSC cached-scheduler experiment was separately published for audit as `experiment/rejected-wsc-fastpath-20261102` at `fe50198c0cfc01d30dbd5c3b05225e16efd32494`; its six same-host rounds failed to establish the needed absolute held/idle targets and **are not integrated into PR6**. This proposal targets the *complete* per-sample nonlinear phase solve across independent lanes, not the previously rejected within-stage matrix-column rearrangement that produced signed-zero and downward-rounding discrepancies.

## Exact-bit positive control

Every active source lane processes its own input; every source lane is compared with a different instance of the original frozen kernel. The nonparticipating lanes must keep their full states bit-identical; no candidate SIMD code exists yet. The grid includes:

| Dimension | Frozen cases |
| --- | --- |
| Width | 1, 2, 3, 4, 7, 8, 15, 16 lanes |
| Networks/rates | Alternating nominal HP/LP; all 8 original scalar rates mixed across lanes |
| FP controls | 4 rounding modes × each supported x86 MXCSR FTZ/DAZ combination (or available target-specific modes) |
| Time/control | 4,096 samples/case; changing and static cutoff/Q; deterministic independent noise |
| Lane lifetime | Sample-position masks, copy, snapshot/restore, reset; round/denormal mode changes while preserving stored state |
| Output/state | All 3 output float bits, 4 double nodes, 4 input-history doubles, input rate, 2 predictor slopes, 2 OTA currents, accepted residual and Newton correction count on every sample |

Positive control performed on unchanged source:

```
x86 MXCSR supported denormal control combinations=4
Preregistered original-scalar lane oracle: widths=8 rates=8 cases=128
lane-samples=3670016 active=3387616 masked=282400
copies=112 restores=352 resets=240 FP-transitions=512
direct-scalar-calls=3387616 vector-backend-calls=0
bit-checks=79337496 mismatches=0
```

The `vector-backend-calls=0` entry is intentional: this is a **scalar-only control**, not a SIMD benchmark or proof of reduced work. A future batch backend must provide a source-site instrumented work witness before any speedup claim.

Local validation used GCC 13.3, `-O3 -DNDEBUG -std=gnu++20`, the unchanged source tree and the repository's portable CMake/Ninja. The registered single-test CTest passed in 2.64 s. The complete 108-case core sweep completed in 669.80 s with **105 PASS and three `BAD_COMMAND`/not-run**, strictly due to generated ELF test binaries lacking execute bits (mode `0644`): `cs80_cold_idle_timing_tests`, `cs80_keyboard_contour_key_map_tests`, `cs80_touch_circuit_tests`. After read-only confirmation of all three ELF identities and a narrowly approved `chmod +x` on **only those generated binaries**, a new CTest process reran exactly those three: **3/3 PASS** in 29.78 s. Thus **108 distinct CTest cases passed across two runs**, not a claim of one uninterrupted 108/108-green run. The environmental anomaly and initial failure are preserved; native CI is still required.

This sparse test worktree initially omitted the tracked MTS vendor directory. The complete, original four-file vendor tree was restored by mechanical copy from a separately cached checkout after each blob's Git SHA had been verified against `HEAD`; no file under `deps/` appears in the tracked diff. No numerical, benchmark, release, or compatibility source was edited to make tests pass.

## Candidate-specific work witness, registered before implementation

In an instrumented candidate *test build only*, independently count (a) actual batch-kernel entries, (b) vector nonlinear loop-iterations, (c) physical-state commits by active lane, and (d) actual unchanged-scalar fallbacks. These counts must be wired to executed source operations, **not wrapper flags or nominal lane width**. For qualified 4/8/16-lane backend cases on supported hardware and FP mode, require nonzero true batch/iteration work, commits equal to the active-mask population, and zero scalar fallbacks for lanes advertised as vector-work. Unsupported modes, tail widths and exceptional values may take the measured scalar fallback. Mutation of dispatch to scalar-only, omitting one active commit, or processing a masked lane must fail a *named* work or state assertion. Do not ship telemetry state.

The future kernel must execute each lane's original `fastTanh`, dynamic Newton stop decision, residual, slopes and state write in the same floating operation order and with identical IEEE exceptional semantics. Masks and mixed convergence count independently; FP-mode or ISA dispatch may use the unchanged scalar fallback. Do not reorder the HP→LP dependency, mix ranks, use fast-math/FMA changes, or convert this software-continuation evidence into a physical-model claim.

## Mutation, integration and absolute gates

The preregistration tests three intentionally false candidate-only behaviors: toggle LP output bit at lane 0/sample 1, advance a masked lane 0/sample 0, and contaminate lane 1 input at sample 1. Each mutation must **compile, run and fail a named assertion with exit 1**; failure to compile, crash or timeout is not a kill. Later SIMD source mutations and instrumented-work fallbacks must also be killed before qualification. No mutation may touch the frozen source or relax acceptance.

All three isolated `g++ -O3 -DNDEBUG -std=gnu++20` candidate-call mutations compiled and were killed by normal assertions on 2026-11-02:

| Compile definition | Exit | First named failure | Observation |
| --- | --- | --- | --- |
| `CS80_PHASE_LANE_MUTATE_OUTPUT` | 1 | `lane.output[0]`, width 1/lane 0/sample 1 | LP `0xb47ff4e7` vs frozen `0xb47ff4e6` (one bit) |
| `CS80_PHASE_LANE_MUTATE_MASK` | 1 | `phase.node[0]`, width 1/lane 0/sample 0 | Masked state advanced; 10 bit-state field failures |
| `CS80_PHASE_LANE_MUTATE_CROSS_LANE` | 1 | `lane.output[0]`, width 2/lane 1/sample 1 | Independently frozen input exposed 13 field/output differences |

Run format: `g++ -O3 -DNDEBUG -std=gnu++20 -fPIE -Isrc -Ibuild-core/scalar-oracle -D<definition> tests/PhaseLaneOracleTests.cpp -o build-core/<isolated-mutant>`, followed by its executable. Neither an expected-fail mutant nor its binary is registered in the positive CTest or carried into the shipping target. Each failure was emitted with `FAIL`, candidate/frozen IEEE bits, and a nonzero normal return rather than crash/timeout.

Existing full scalar, circuit, event, oscillator, wake, legacy/V1/V2 and numerical tests remain binding. If a true backend is implemented and passes, measure at least **three uncontended same-host before and three after** rounds of the untouched `cs80_source_factory_benchmark` at 48 kHz/256/8 voices. Report complete per-fixture held/moving/idle, p99, misses, unchanged Legacy controls/checksums and *all* raw outliers. The held **0.15–0.20 RTF** and quiet-idle **≤0.02 RTF** hard targets are currently **unmet**; a phase-only improvement cannot be assumed to fix idle (where phase processing is largely inactive). No source push/integration, re-pinning, merge or release follows merely from this test-only preregistration.
