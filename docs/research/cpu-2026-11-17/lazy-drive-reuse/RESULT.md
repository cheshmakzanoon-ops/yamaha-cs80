# Lazy drive sanitation reuse: rejected instruction screen

Research-only invocation-local lazy reuse of identical drive sanitation between cutoff and shared receiver readouts. First sanitation remains at its original validation position; no eager validation or persistent cache. This was distinct from the shared-Q arithmetic hoist.

Strict O3 and UBSan probes passed 131,072 paired combined readouts across rounding, denormal, invalid-input and active-rank combinations. Full output bits, return/saturation state, FP flags and errno matched. Wrong-rank reuse, lost saturation and accepting invalid inputs all failed negative controls. Full-engine qualification matched 55 rows and 15,479,808 PCM bytes exactly. Independent review confirmed scope and results.

The single predeclared candidate Callgrind screen completed 15 cases / 36 rows, with all non-timing fields matching the retained baseline. Instruction reads fell from **126,792,511,004** to **126,262,558,504**, a **0.4179683%** reduction, below the frozen **1%** screening threshold. Independent audit reproduced the totals and checked input hashes.

Decision: **REJECT_INSTRUCTION_SCREEN**. No retry, native-cost trial or production integration. Instruction reduction is not measured CPU improvement; native benefit remains unknown. Raw probe, qualification, profile and command evidence is retained separately. This candidate form is closed.
