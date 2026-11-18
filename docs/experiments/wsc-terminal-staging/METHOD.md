# Method and reproduction boundary

## Source-faithful mechanism

S runs the scalar scheduler at each **original rank position**. Only a genuine ordinary terminal `nodes(+1)` and existing epilogue may wait. A local rejection drains older pending tokens, then calls the separately compiled original tick once at the same position. Already advanced schedulers are never replayed. The cumulative ever-deferred mask is not the live pending mask.

V uses a stack-local render-span binding for four fixed numbered groups. Setup checks four immutable owners and selects a safely dispatched kernel. Each actual sample still obeys the original policy/filter intersection and staging/environment gates. Vector finish requires live `pendingMask == 0xf`, a valid binding and a true guarded kernel return. Rejected scratch is not read. Successful 20-field node records are copied into named fields and the unchanged scalar commit epilogue runs once per ordinal. Mixed/rejected groups finish scalar, not by reticking.

The kernel performs genuine AVX2 double arithmetic, including division; it does not merely pack scalar results. No global fast-math, FMA or native-ISA compile option is added. The existing `Ig00156Batch.cpp` and new `WscNodeKernelAvx2.cpp` alone additionally use `-ffp-contract=off`, matching the frozen normal build. Observed admission is the qualified GNU/x86-64 caller, CPU/OS AVX2 support, x87 CW `0x037f` and MXCSR control `0x1f80` or `0x9fc0`. Sticky flags are not cleared to authorize work. Whole-sample eligibility remains deliberately narrow: full eight held Source Performance voices at 48/96 kHz, with declared lifecycle/matrix/event restrictions. Vector-specific refusal falls back to S. When S declines, the original B sample path is retained, including the stock batched-filter route where appropriate.

Normal objects contain no observer hooks. Diagnostic copies and harnesses are not timing binaries. Source duplication isolates this experiment; it is not a proposed production refactor. Engine/WSC persistent layout is unchanged, while the optional vector cohort is a larger stack object (192 bytes versus scalar 176).

## Evidence families cannot be combined into a stronger result

1. `scalar-evidence.json`: S's five-arm nine-scene receipts. Each scene renders 1536 callbacks plus continuation at 48k/64/internal 2x, with the frame applied before MIDI every callback. Explicit all 16 WSC continuation/cache/events, public PCM/readouts and FP/errno are captured every callback. Rank RNG is recorded immediately after each actual draw, not inferred from final state. Reverse idle/BBD and three real Engine mixed/fallback cases are separate supplements.
2. `component-evidence.json`: numerical adapter and standalone optional vector finish. Counts describe those synthetic/controlled cohorts, not integrated Engine occupancy or speed. Mutation outcomes distinguish numerical, FP and structural guards.
3. `integrated-vector-evidence.json`: completed V1b nine-scene normal/counting/reverse comparisons, per-numbered-group actual attempts/returns/consumed records and render-span bindings; plus six semantic cases, small/irregular-block and interleaved-owner/copy-reset controls, and reverse idle/BBD controls. The stock and all four comparison arms share the same 16 nonidentical-to-one-frame and four identical partition checkpoints per arm. This qualifies arm parity for tested partitions, not partition invariance.
4. `integrated-public-probes.json`: V1b's **actual public `Engine::render`** probe, one host frame/two internal steps, 22 cases, seven real exceptions, then four real continued frames. Current/future residual sNaNs, elapsed-state seeds and exact reset boundaries are deliberate diagnostic constructions. x87-only and directed-rounding cases are fallback controls, not fabricated signals. Signal FP state comes from saved `ucontext`; trap PCs are normalized to selected symbol plus offset.

The public probe uses heap-resident Engine state and does not use a private sample-method bypass. Its signal recovery must not be generalized to arbitrary live automatic cohorts: the trapping cases are genuine original fallback routes. The exact observer roster is bundled. It is fieldwise, not a claim to serialize every private Engine/FX field. Compare both bytes and normalized signal sites; equal final audio alone is insufficient.

## Cheap offline replay (safe during an exclusive measurement)

From this package directory:

```sh
python3 -B verify.py
python3 -B -m unittest test_verify.py
```

The validator checks saved evidence and file hashes only. Raw traces are omitted. A passing replay means the **receipt package is internally consistent and unchanged**, not that the historical executions were independently rerun.

## Optional later DSP reproduction (not run during this export)

Requirements: a local clean baseline checkout, GNU 14.2-compatible C++20 tools, `ar`, `nm`, Python 3 and Linux x86-64/glibc signal facilities. AVX2 plus OS XMM/YMM support is needed for a positive vector witness; this public fallback/trap fixture has no successful-vector census. No downloads, package installation or native plugin/DAW loading occur.

Create independent B/S/V roots at the baseline commit. Apply `stock-to-scalar.patch` with `git apply` to S and V, then `scalar-to-vector-v1b.patch` to V. Do not apply both patches directly to B in the wrong order or modify a production checkout. The canonical patch hashes are checked by `verify.py`.

Later, explicitly invoke (paths are placeholders for your own isolated roots):

```sh
python3 -B reproduce.py --arm B --source-root work/B --output results/B
python3 -B reproduce.py --arm S --source-root work/S --output results/S
python3 -B reproduce.py --arm V --source-root work/V --output results/V
python3 -B reproduce.py --arm V --source-root work/V --output results/V-reverse --reverse
python3 -B reproduce.py --compare results/B results/S results/V results/V-reverse
```

The helper checks the selected source fingerprints, builds normal core objects without access overrides, then builds only the diagnostic TU with private access and no PIE. It runs no benchmark. It refuses an existing output directory. Command resource limits are safeguards, not timings. The source-root check covers the fingerprints listed in provenance, not all unchanged baseline headers: a **clean baseline reconstruction remains required**. Whole-object/executable hashes can vary with compiler, library and source-path metadata; they bind the historical artifacts and are not promised portable binary outputs. Raw/output and normalized signal comparisons across the new arms are the reproduction criterion.

The helper was syntax-checked only when preparing this package. The exact bundled C++ probe/observer are unchanged from the successful historical public-render test. The large scalar/component suites are represented by saved receipts and source/driver fingerprints, not fully self-contained rerunners in this lean package.

## Final disposition and separate cost export

`integrator-hook.json` now binds the completed integrated receipt and the independent `cost/` export. It records the complete 216-slot result and **no performance admission**, not promotion. The scalar-versus-stock descriptive estimates must not be renamed vector arithmetic gains. The predeclared rule's realized superiority floor is 21.48%; the V-versus-B primary lower confidence bound does not meet it, and two secondary gates also fail. See [cost/README.md](cost/README.md) for the full design, raw records, replay and limitations.

Any later work needs a new versioned cut with newly declared source/archive/driver/executable bindings and prospective gates. No full CTest/native/DAW or arbitrary-compiler/link qualification is implied. No DSP execution occurred while assembling this portable research package.
