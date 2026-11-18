# Eight-stage filter Engine integration

Research checkpoint, not a shipping change or performance approval. This is a true eight-stage filter solve replacing two adjacent shipping AVX2 quartet solves. It is neither the earlier four-lane EVEX register experiment nor oscillator staging. See the [direct numerical component](../README.md) separately.

## Actual V2, including the rejected first integration

`candidate.patch` is the **exact patch used for V2**, not a retrospective approximation. `source-inventory.json` authenticates all 193 stock files at `e9e8175f4278d2457279fcb8809cbba1ddb7be2b` and the resulting 196-file candidate. The original setter, scalar oscillator and four-lane filter backend are unchanged; there are no new class fields.

V1 passed identity of the original Engine bodies but emitted a competing **17,523-byte** oscillator tick from the separate filter TU, versus stock's **12,063 bytes**. It was stopped **before any candidate Engine render**. V2 adds one original-TU bridge and replaces that same-position call in the filter TU. `bridge-v1-to-v2.patch` records the exact remedy, not a numerical change. The 14-byte bridge calls the sole original tick.

V2's original scalar `<false>` body (32,154 bytes /646 relocations), batch `<true>` body (37,511 /693), and oscillator tick (12,063 /136) are byte-identical with resolved symbolic relocations identical. No competing protected definition is emitted. Source, object, archive and diagnostic identities are in `provenance.json` and `records/static-v2.json`.

## Transaction and ordering

The shipping order is prepare four → solve/commit four → prepare the next four. A general preparation reorder would change metadata, histories, FP/errno or partial state on exceptions.

The new path is narrowly guarded: Source Performance, strict supported build, nearest/all-masked SSE and canonical x87 control, a preselected supported CPU/OS kernel, distinct stages, and a future quartet whose requested cutoff/Q are ordinary positive finite values. Integer observations prove canonical Q-clamp/raw-key equality, supported rate, warped mode, valid coefficients and an unchanged idle clock. Therefore the genuine future setter will early-return and no reReference or clock write will occur.

**Those future prepare calls are not moved or skipped.** A successful eight-stage solve writes the eight independent stages now, consumes the first outputs at their original point, then retains four outputs until the second quartet's original preparation/consumption point. HP completes before LP, then VCA/mix follows. No cross-sample queue, allocation or callback CPU-feature probe is introduced. Failed prospective checks or a rejected eight kernel retain the original first quartet solve before future preparation. A subsequent quartet can try its next neighbor, so attempted pairs need not equal the census's fixed disjoint pairing.

## Actual opportunities, not voice-count inference

The stock-only census used eight held notes, 48 kHz/64 frames, 2x internal rate, drift .37/dispersion .18, frame before MIDI every callback; 1,024 warm callbacks, 16 counted, one continuation. Every counted pass actually supplied four ordered HP quartets and four LP quartets: **four disjoint eight-stage opportunities per internal sample**. All eight owners were distinct and every pair had heterogeneous input bits.

| Scene | Both real stock quartets accepted /8,192 pairs | Future-key/clock no-op observation |
|---|---:|---:|
| Factory held | 8,067 | 8,192 |
| Factory moving | 8,017 | 170 |
| Panel moving | 8,192 | 0 |
| Pure held | 8,053 | 8,192 |

The census's finite-control Q screen was an observation, not a complete exceptional-input gate. V2 adds the explicit ordinary-bit requirements. **Pure's stock quartet eligibility does not imply integration eligibility:** V2 rejects Pure by policy and its measured integration hit count is zero.

## Fresh Engine evidence

Normal core contains no reporting hooks. A separate reporting copy observes the real eight-kernel boolean return and the later four-output consumption; `observer.patch` reconstructs its exact recorded source.

| Fixture | Equal bytes per arm | Tested arms | Counted actual eight successes |
|---|---:|---|---:|
| Factory held | 24,109,781 | Stock, normal, observed | 8,067 |
| Factory moving | 24,489,391 | Stock, normal, reverse, observed | 328 |
| Panel moving | 24,492,625 | Stock, normal, reverse, observed | 0 |
| Pure held | 24,027,930 | Stock, normal, reverse, observed | 0 |
| Sparse three voices | 24,008,449 | Stock, normal, reverse, observed | 0 |

Each has 1,024 warm +16 counted +1 continuation callbacks. Held has 8,223 attempts and exactly 8,067 later consumptions; moving Factory has 345 attempts and 328 consumptions. These are actual dispatch observations, not proposed savings. No duplicate owner was observed.

Traces contain every PCM sample and before/after MXCSR, x87 control/status and errno; public source/rank readouts; explicit full fields of all 32 filter stages and idle clock references; all 16 oscillator physical, residual, endpoint and active diagnostic/event state; audio/modulation/idle/modern-note RNG states. These RNG states are not an independent every-draw-order proof, and not every private Engine field is claimed. Reporting restores fenv/errno and checks restoration.

Additional fresh archived fixtures, not borrowed receipts:

- **22 public-render probes**, including seven actual SIGFPE cases, precision-only CSR `0x0f80`, masked controls and mixed boundaries. Normal and reverse match stock through complete recorded partial state and four post-target continuation samples: **17,019,552 bytes/arm**. Normalized trap function/offset, FP and errno match.
- A separate observed run confirms **zero eight attempts, successes and consumptions in all nine unsupported-FP target samples**, including SSE invalid/precision unmask, x87 invalid unmask and downward rounding. Its complete 22-case stream also matches stock.
- **16 directed-rounding /idle-wake /enabled-BBD cases**, 2,064 callbacks, including 768 idle and 1,032 enabled-BBD callbacks: normal/reverse exactly match stock's **64,267,367 bytes/arm**, including nearest-mode continuations.

All 26 fresh Engine invocations completed. The original large captures remain unbundled; their byte lengths and SHA-256 hashes are recorded here. No timing or native-module run belongs to this leaf.

## Shared-weak and proof limits

There are 14 shared weak functions. Thirteen have identical code bytes; `idleStepRank` differs, **4,615 versus 7,149 bytes**. Twelve of the equal-byte helpers have differing conservative relocation records, including stage process, PRA, BBD, envelopes and FX. Equivalent constants can live in different pools, so those records are not automatically numerical defects. The actual normal/reverse tests above pass, but **arbitrary link order, compiler, LTO, every exceptional restored state or every future configuration is not proved**.

The Engine tests do not fully qualify all custom networks, all gate/rejection mutations, every disabled/build mode, every bypass mixture, or every possible future setter/trap interaction. Direct arithmetic tests are separate. The patch intentionally leaves production integration/build wiring untouched. Hit counts do not establish a CPU win.

Run offline checks and reconstruction as described in [REPRODUCE.md](REPRODUCE.md). The portable replay recipe was prepared and source-checked, **not newly compiled or executed** for this checkpoint.
