# Eight-stage IG00156 filter: integrated research result

**This is a prototype, not an update to the shipped plug-in.** The candidate really executes an eight-double Newton solve in place of two four-double solves. In a held factory preset, native audio and public state matched stock. Reducing the number of packed solver iterations does not by itself establish a CPU saving.

## What was built and what ran

The unchanged four-wide solver accounted for about 16.3% of whole-core PC samples in earlier held eight-note captures. The new scheduler operates only when two adjacent stage quartets are independent, the next quartet's still-ordered preparations can be proven no-ops and the strict CPU/OS/floating-point requirements are met. A kernel decline retains the original ordered quartet/scalar fallback. Changing filter controls can remove almost all opportunities.

| Question | Evidence | Scope |
|---|---|---|
| Does the new kernel compute the same result as two genuine shipping quartet calls? | [Direct test, patch, and reproducer](README.md): 12,288 ordinary and 12,288 recovery-enabled cohorts, complete stage bytes and caller floating-point/errno matched. | Executed x86-64 AVX512, one strict compiler configuration; no general math or portability proof. |
| Can the complete Engine actually use it without changing the tested behavior? | [Engine integration and exceptional-state controls](engine/README.md): 8,067 successful eight-wide pairs consumed in the 16-counted-block held case, 328 moving Factory, none in moving Panel, Pure or sparse. Recorded held byte stream matched stock over 24,109,781 bytes per arm. | Includes real FP traps, partial state and continuation, alternate link order on the broader scene/exceptional matrix, idle/wake and BBD. The first, statically rejected integration is preserved. |
| How much solver work actually changed? | [Exact work ledger](work/README.md): 21,025 versus 12,254 packed Newton loop iterations (**41.72% fewer**); 70,478 active-lane iteration instances in both; 84,100 versus 97,448 total SIMD lane slots evaluated (**15.87% more**). | Actual loop executions in 16 held Engine blocks. Wider, less-uniform groups and new lookahead mean that fewer loops need not mean fewer cycles. |
| Does the real native plug-in enter the path, and does its public result match? | [Native host test](native/README.md): four fresh stock/normal/reporting runs matched over 933,293 bytes each, 1,041 actual VST3 callbacks and 66,624 stereo frames. The separate observed candidate consumed 8,090 eight-wide pairs in its 16-block window. | Real Linux IAudioProcessor call; normal and reporting modules are hybrid release prototypes. Public VST3 controls/MIDI are not assumed bit-identical to direct Engine inputs. |

The direct and Engine patches are separate: use the [complete six-path Engine patch](engine/candidate.patch) rather than applying the direct-only patch on top of it. The [reproduction notes](engine/REPRODUCE.md) describe source identities and applicability. No production source, build default, runtime dispatch or distribution binary is changed in this checkpoint.

## Native callback CPU: no speedup demonstrated

In the [one-shot, predeclared native comparison](native-cost/README.md), **96 fresh processes** completed all **49,920 measured callbacks** on this Linux VM. There were 24 four-arm blocks in six independently randomized Williams sets. Only normal stock and normal V2 modules were timed, nested clocks bracketing real `IAudioProcessor::process` calls after 1,024 warm callbacks. The two same-stock arms use one physical file; the deliberate-extra-work arm performs 544 rather than 512 callbacks on the same plugin instance. Integrity passed; no retries, exclusions or replacements.

Each ordinary observation covers 512 real 64-frame callbacks, or 0.68267 seconds of audio. Across complete process totals, stock twins pooled have a geometric mean of **457.970 ms** thread CPU versus **468.428 ms** for V2. This is **0.671 versus 0.686 seconds of measured callback CPU per audio second** for this fixture, not the whole host or a deadline statistic.

| Prespecified diagnostic | Estimate | 95% whole-block interval | Required outcome |
|---|---:|---:|---|
| Same stock, S1 vs S0, CPU change | +0.54% | −3.84% to +5.33% | Entire interval inside ±2%: failed |
| Same stock with +6.25% genuine work, CPU change | +7.48% | +4.33% to +10.61% | Entire interval inside +3% to +10%: failed |
| V2 vs pooled stock, CPU **saving** | **−2.28%** | **−4.73% to +0.36%** | Lower bound above +3%: failed |

The separately required six-set t intervals also failed: placebo −6.82% to +8.48%; positive control +3.26% to +11.87%; candidate saving −4.24% to −0.37%. Comparisons to both individual stock twins, both temporal halves and several order checks also failed. See the complete 20-gate report, including the 3 gates that passed. An [independent offline computation](INDEPENDENT-AUDIT.json) from all endpoints agreed on all 20 gate decisions and 96 process observations (largest difference across 1,473 numerical comparisons: 1.8×10⁻¹⁵). The negative point estimate is descriptive: failed calibration prevents claiming either a reliable product regression or a speedup. No subset was promoted.

**Decision:** do not integrate or enable the eight-wide scheduler. No production CPU reduction is claimed. Repeating the same screen on this VM is not the next step. A new, separately qualified measurement on stable, representative, controlled target hardware would be needed before reconsidering any prior exact-sound prototype; widening the solver merely because it executes fewer packed loops is not justified.

### Why stop this local search here?

The [stock samples](../shipping-core-profile/README.md) justify looking at oscillators, filter solving and moving filter setup, not treating their entire cost as removable. [Endpoint liveness](../oscillator-endpoint-copy/README.md), [scalar and four-rank oscillator staging](../wsc-terminal-staging/README.md), [immediate scalar completion](../wsc-immediate-rank-scalar/README.md), [cross-lane coefficient reuse](../lp-cross-lane-reuse/README.md) and [shared readout retention](../shared-readout-retention/README.md) already have explicit candidates, failed controls or concrete exception/state counterexamples. The [residual-event/ring study](../residual-ring-reviewed/README.md) is also not a fresh lead: its moving case used fewer guest instructions but more reads/writes, not a demonstrated native CPU saving. Neither the whole PRA self region (about 1.0–1.5% of sampled core) nor the [BBD routine](../bbd-cursor-census/README.md) (about 0.6–1.4%) established a large removable component. Reducing repeated floating-point admission checks remains a small unquantified idea, not another qualified material optimization. This is a decision about the evidence available, not a proof that the synthesizer cannot be made faster.
