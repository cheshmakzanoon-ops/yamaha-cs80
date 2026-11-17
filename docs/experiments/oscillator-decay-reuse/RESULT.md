# Isolated invocation-local oscillator decay reuse

Candidate SHA256: `489cc6f9a442691a3ed73032bc37605a3e819e61e49343defc06f448629d2959`.
Original production header SHA256: `c037a8970f939bdcd7d98ea56348006f82f2412cc64cd912fbf0f85b5ffa9373`.

The only arithmetic-path change retains the first `decay(limit/tau)` within `nextBranchEvent`. The later shaper-end evaluation uses that value only for an identical-bit positive finite interval; otherwise it evaluates the original decay. No persistent state, quotient, root solver, residual or `advance` change. No production checkout edits.

Primary compilation uses GNU14.2, `-O3 -DNDEBUG -std=gnu++20 -fPIC -Wall -Wextra -Wpedantic`, matching normal core flags. No contraction override. Current environment is recorded separately; do not inherit platform statements from earlier runs.

Completed primary evidence:
- Identical-token A/A: 328,508 checks, zero failures, 161,787 ticks.
- Candidate full archived explicit-bit corpus: same counts, zero failures.
- Known-prestate tick FP corpus: 1,961,180 checks, zero failures, 647,148 ticks.
- Full API FP corpus: 6,011,358 checks, zero failures, 970,722 ticks, 1,008,348 paired API calls. Each call pair independently starts with identical known clean/seeded environment, masked exceptions and nearest rounding; FTZ/DAZ on/off. Compare MXCSR, relevant x87 control/status and errno, not raw fenv instruction pointers.
- Coverage-only instrumented corpus: 199,235 reused spans, 19,535 fallback spans, 20,692 input-level roots, 60 turning points, 30,657 shaper-knee cases; 195,843 polynomial-domain and 22,927 libm-domain initial evaluations. Instrumentation is not part of candidate or timing.
- Unconditional-reuse negative control: 137,221 failures, exit1.
- Perturbed-reuse negative control: 167,376 failures, exit1.

Native-ISA stress final log reports 1,961,180 checks/zero failures. Its initial process handle vanished during environment recovery; the independent reviewer subsequently reran it and verified exit0. The reviewer also independently rebuilt/reran the full API FP corpus with original flags and verified all counts and exit0. Native-ISA stress is additional sensitivity coverage, not the primary plugin compilation contract.

Preparation issues retained: the first A/A compile encountered GCC pragma-once deduplication of identical separate files. A comment-only baseline inclusion marker resolved it without changing production tokens. An initial tick-FP draft preserved inherited flags, potentially hiding differential exceptions; it is not relied on. The known-prestate and API tests supersede that draft without changing candidate code or compiler flags.

Independent exactness review gave GO for an isolated cost trial. Cost-plan approval/freeze remain separate gates. No benchmark or CPU-saving claim is made here. The earlier profiler warning and unresolved requested1kHz/observed~250Hz accounting remain; its packet concentration only motivated qualitative investigation.

## Frozen cost outcome: do not advance

The approved one-shot component walltime diagnostic completed both bounded phases with exit0, no watchdog event and empty owned process groups. A/A took15.064seconds and A/B45.719seconds. All384 rows retain their original order; paired and cross-phase output/state checksums agree, CPU0 affinity witnesses agree, and frozen inputs remain unchanged. The original archived probe was used byte-for-byte with normal GNU14/PIC flags. No run was dropped, retried or replaced.

Across ten active cases, geometric mean candidate/baseline ratio is0.9939333955: apparent0.6067% improvement, bootstrap95% gain interval **−1.9276% to+3.1633%**. The largest absolute active-case A/A block gain was23.0605%, above the frozen3% floor. Ten of twelve individual case intervals, including both idle cases, also fail the lower-bound−3% guard.

Decision: **NO_ADVANCE_GAIN_OR_UNCERTAIN_GUARD**. The candidate passes the scoped exactness tests but has not demonstrated useful benefit above noise. These uncertain intervals are not proof of regression. No plugin-pair rebuild, production integration, retry or relaxed gate follows this result. Independent review verified both execution receipts, all immutable hashes,384 valid rows and independently reproduced the decision and intervals using log-space bootstrap. Raw CSV, receipts, immutable plan and fixed analyzer are retained in `cost/`.
