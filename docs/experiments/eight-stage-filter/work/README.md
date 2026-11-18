# Deterministic packed-filter work census

**No timing or speed claim.** In this single held-factory fixture, V2 executes fewer genuine packed solver loop bodies, but wider groups also do more inactive-lane arithmetic. This is mechanistic evidence, not performance admission, native-plugin qualification, or a rerun of failed clock controls.

## Result

Both separately instrumented arms exactly reproduce the existing **24,109,781-byte** stock trace, including all 1,024 warm callbacks, 16 counted callbacks and one continuation. The trace covers PCM, the explicit filter/control/history fields, oscillator physical/event state, selected RNG/readouts, and callback FP state/errno. It is not every private Engine field.

Counted window only:

| Quantity | Stock | V2 |
|---|---:|---:|
| Accepted 4-wide solves | 16,257 | 123 |
| Declined 4-wide calls, followed by scalar fallback | 127 | 127 |
| Accepted 8-wide solves | 0 | 8,067 |
| Declined 8-wide attempts | 0 | 156 |
| Actual packed loop-body passes | 21,025 | 12,254 |
| Newton-update passes | 4,768 | 4,064 |
| Width-weighted evaluated loop slots | 84,100 | 97,448 |
| Active-lane loop slots | 70,478 | 70,478 |

V2 performs 12,108 eight-wide loop passes plus 146 remaining four-wide passes. There are **41.717% fewer packed loop bodies**, not 50%. Width-weighted evaluated slots increase **15.872%**. These two counts measure different things; neither estimates CPU time. An update pass excludes the final residual-evaluation/convergence pass.

For 3,218 of the 8,067 accepted eight-lane groups, the two matched stock quartets need different iteration counts. All nine observed pair outcomes are retained in `observations.json`, from `1+1 -> 1` through `3+3 -> 3`. Every eight-wide count equals the maximum of its two actual stock counts, not their sum. Each split active-mask sequence matches the stock half, then remains zero once that half has finished. This retains, rather than hides, uneven convergence.

All 156 eight-wide declines occur before a solver loop. Their matched stock halves are `[decline, decline]` twice, `[decline, accept]` 91 times and `[accept, decline]` 63 times. These attempts may overlap after rejection, so these are not disjoint stage totals. The same 127 four-wide calls decline and take the unchanged scalar path in both arms. Scalar solver iterations are **not counted**. No admission decline or pass is excluded.

## Exact procedure

The existing V2 Engine and authentic `Work(1)` fixture are reused: eight factory-held notes at 48 kHz, 64 host frames, 2x internal samples, drift 0.37, dispersion 0.18. A full control frame precedes events on every callback. Initial FP setup is nearest/masked x87 and SSE with FTZ/DAZ (`0x037f`, `0x9fc0`). There is no new native module.

Only separate copies of `Ig00156Batch.cpp` and `Ig00156Eight.cpp` gain reporting. The arithmetic, admission predicates and fallback decisions remain literal original source. `workBegin` captures ordered owner IDs, exact input bits and a fingerprint of each explicitly listed stage field. A local integer counter and integer extraction of the existing active vector mask run at each genuine loop entry. A second integer counter records actual Newton updates. `workEnd` records success or decline at every return. The callbacks and fingerprints use integer byte operations, with no allocation, logging, FP arithmetic or errno calls during the render. Final-linked excerpts show genuine YMM/ZMM arithmetic and the counter endpoints.

The normal V2 Engine objects and original reporting Engine object are reused, not rebuilt. Counted backend objects precede their archives at link time. The 7 commands (three compiles, two links, two runs) all succeeded. Each run had a 60-second CPU resource ceiling and 90-second external watchdog; these were safety limits, not timing observations. No measurement clock function is called by this main. The imported fixture retains unused historical timing helpers, whose renamed main is not called.

Every callback writes the same existing trace and preserves the observation getters' FP state/errno. Both complete traces match the prior stock SHA. Kernel-event logging is deliberately off during warmup; warmup is executed and byte-traced, **not assigned invented event counts**. The window and continuation have separate aggregates. This is one run per counted arm, with no numerical retuning or repeat.

## Matching and limits

Including continuation, all **17,408 ordered stock quartet events** match the candidate's accepted eight groups expanded into two quartets plus remaining four calls. Stage identities and input words match exactly; all complete explicit-field fingerprints match. Fingerprints are FNV64 over field bytes, not a collision-free full pre-state byte proof. Active masks and count conservation are checked independently. Failed eight attempts match the next two stock quartets without advancing the replay position. Raw owner addresses are never exported; IDs identify voice/rank/HP-or-LP position.

`stock-counts.jsonl.gz` and `candidate-counts.jsonl.gz` retain **every** counted/continuation call, its phase, width, success, passes, updates, active masks and ordered stage IDs. This compact projection omits only inputs and state fingerprints. The original full event files and binary traces are deliberately unbundled: their exact bytes/hashes are in `observations.json`. Omitted hashes identify retained files; they do not let a reader independently recover or prove their content.

The standalone compact replay verifies all event order, stage grouping, loop masks, histograms, declines and conservation. Full raw replay additionally checks exact inputs, fingerprints and, when supplied, trace file hashes. It does not render DSP or read clocks.

```sh
python3 -B replay.py --self-test
# Optional, with the separately retained raw files:
python3 -B replay.py --raw-dir /path/to/raw --self-test
```

The local full replay rejects missing/reordered work, stage/input/fingerprint changes, forged pass/mask counts and discarded declines. `verification.json` records the final replay. Manifest verification catches altered exports. Correctness of this narrow census does not close V2's separate general Engine/exception/link-order gates.

## Reproduction inputs

`provenance.json` pins the original source, archives, reporting object, counted objects and executables. The two counted numerical sources are byte-identical to those used. The host and fixture have only their two include paths rewritten for portability; original hashes are retained. The shipping adapter sources are included unchanged. No binary is bundled.

`reproduce.sh` gives the exact dependency roles, compile/link order and an explicitly enabled **future untimed** reproduction. It requires the pinned stock core, the separately exported V2 source/core and original reporting Engine object (or an exactly rebuilt object). It is not run by offline replay. Changing compiler, dependency or layout is a new reproduction, not retroactive provenance for these measurements.
