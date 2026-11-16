# Source-only proposal: scoped pitch-memo cost screen

Not implemented, frozen, or executed. No workflow/request changes authorized here. Candidate:46812652364337aec1c8d51443427d1b63df5351, based on production5836041138bc1708c3fd2d4240cf63a3e1def4b3 (later published changes are documentation/tooling only). This is the pow memo, not the unpromoted exp2 candidate.

## Prerequisite: deployment qualification is a real stop gate

The current glibc2.41 SSE-only/errno-safe proof does **not** qualify an Ubuntu runner's actual resolved powf. A sample grid alone cannot establish the absence of x87/errno side effects for every admitted value. Do not enable the option merely because the OS says glibc or focused tests pass.

Recommended two-step process, both separately authorized before remote execution:
1. An inspection-only fresh-runner probe with production/memoOFF: capture compiler and wrapper disassembly, actual dynamic powf binding, libc/libm/loader build IDs and SHA256, glibc version, resolved function offset relative to mapped library, full relevant disassembly/normal-domain control-flow and CPU/IFUNC capabilities. Record LD_PRELOAD/LD_AUDIT presence; reject unexpected interposition. Inspect wrapper's actual PLT/GOT call resolution, not only a possibly unrelated function-pointer symbol. Preserve the existing dense-grid status/errno/x87 proof under all four denormal modes and x87 controls as supplementary evidence. No calibration or candidate cost measurements in this phase.
2. Independent review establishes the specific normal base2/exponent[-120,120] path uses no x87/status/errno mutation or unexpected calls and excludes all error tails. Record an explicit reviewed fingerprint (library hashes/build IDs, resolved implementation offset/function bytes and wrapper compiler/codegen). Only then can a fresh cost runner enable memoON, and it must re-resolve and match that fingerprint before any opt-in tests or timing. Different library/IFUNC/interposition => stop unqualified, not switch to a permissive gate or claim a scalar fallback result qualifies the memo.

A full relevant function/branch proof is necessary; matching a function name or a few opcodes is insufficient. ASLR addresses are not stable identity; relative offsets+library bytes are. A new host can choose a different IFUNC even with the same package. This inspection phase is not a timing retry. If no deployable reviewed fingerprint can be established, do not run the native memo screen.

## Three immutable arms: separate optimization from default-layout cost

A: unchanged production583, original pow; no memo fields.
B: candidate4681265, CS80_EXPERIMENTAL_PITCH_MEMO_SSE_LIBM=OFF (default).
C: identical candidate4681265, optionON after library qualification.

Same strict generic GCC Release settings, AVX ON/recovery OFF, no native/FMA/fastmath/LTO/PGO. Record all compiler/core/driver commands and hashes. B and C differ only the explicit option. All consumers must be freshly compiled against their own Engine headers: Rank layout is unconditional, so old frontend archives/serialized object bytes cannot be reused. C's out-of-line hz call, live guards and bypass path remain in measured cost. B/A is essential: defaultOFF can still change object/cache layout despite the original arithmetic branch. C/B measures added memo machinery; C/A is the actual deployable opt-in comparison.

Prerequisites: generic/native/memoOFF/AVXOFF correctness and focused lifecycle/status tests, fresh matching actual callback corpus, actual hit/miss/bypass counts in the intended contexts, and generated code reviewed. On the cost host repeat fresh native correctness/history/dispatch and a separate count-only probe; neither benchmark executable may contain counters. Require expected hit-positive static cases and preserve all bypass/miss counts. No borrowed prior-host timing claim.

## Fixed benefit population, chosen before timings

Audited repeat census identifies the static dry paired_tones held rows as eligible. Primary active-suite groups are kept distinct:
- Three source_performance / paired_tones / selectors0,1,10 / warmed_held rows.
- Three isolation / paired_tones / selectors0,1,10 / warmed_held rows.

For each triplet and contrast, compute the median of the three individual callback-thread CPU ratios within each group. Whole-suite absolute utilization is sum(callback CPU seconds)/sum(audio seconds); because per-run audio durations must match, the total candidate/comparator ratio is the ratio of summed CPU seconds. Do not use all nine source-performance held rows as a benefit target: six expressive MPE/FX held rows deliberately bypass the memo. Do not drop them either; they are mandatory loss controls. This is not a post-timing favorable-subset choice: the population comes from the independently audited exact-repeat census.

Retain the complete performance suite15cases/36rows, including expressive held/pedal/release, Legacy, isolation and tails. Keep the complete baseline/lowload suite11cases/19rows, including four eligible nonidle isolation held rows, Legacy and all idle cases. Moving tabs/pressure/wah does not imply pitch modulation; its known eligible classification is retained. Return-from-bypass/LFO/matrix/cold-context correctness remains in dedicated tests, since this census does not cover those transitions.

## Proposed fixed calibration and measurement sequence

Use the same48kHz/128-frame benchmark with --quick --callback-cpu: performance256held callbacks; baseline/lowload512. Warmup/tail/pedal durations remain the original fixture defaults. Pin one already-allowed CPU, no other owned work during timing, record host/affinity/frequency metadata read-only. No governor/security/permission changes.

Before candidate timings, four A/A pairs per suite, alternating label order. Performance calibration must satisfy every pair: total CPU ratio and BOTH primary three-row group ratios in[0.98,1.02], every individual row in[0.90,1.10]. Lowload uses total plus the four-isolation-held median with the same bounds. Any failure stops all candidate timing; no retry. Let E_g be the maximum absolute A/A deviation from1 for each exact aggregate g, recorded without subtraction. The active source primary uses only its active source-three-row E; active isolation primary uses only its active isolation-three-row E. Both C/A and C/B use that same corresponding A/A E. Lowload uses its own total/four-row-isolation E, never active E. E is not used to relax any loss threshold. Four pairs give an empirical guard, not2% precision or a confidence interval.

If both calibrations are usable, exactly six three-arm triplets for EACH suite, with fixed orders ABC,ACB,BAC,BCA,CAB,CBA. No extra rounds. This balances each arm's run position and every pair's earlier/later position. Keep all absolute CPU/audio ratios and every contrast C/A,C/B,B/A; no subtracting B/A to manufacture adjusted benefits. All runs must match their suite's first reference inventory/checksum/finiteness/rate/block/callback/audio-duration/clock-count. Stop on first correctness/hash/clock failure, not after silently dropping a run.

The extra B arm is necessary to answer the defaultOFF cost question. If this fixed workload exceeds a reasonable predefined job timeout, redesign scope BEFORE any timing; never truncate based on results. No promises that a fresh runner is stable.

## Proposed preregistered interpretation

A material eligible-workload signal requires, separately for both three-row primary groups and BOTH C/A and C/B contrasts:
- median ratio <=0.98;
- improvement strictly greater than2*E_g;
- improvement in at least5/6triplets;
- median below1 in each relative-order stratum (C before/after comparator).

All36active and19lowload rows, all three contrasts, also face loss gates. For each suite/contrast, hard loss: total median>1.05 with>=5/6ratios>1, OR any row median>1.10 with>=5/6ratios>1.05. Caution blocks advancement: total median>1.03 with>=5/6>1, OR row median>1.05 with>=5/6>1, OR total/group median>1.05 regardless directional count, OR any row median>1.10 regardless directional count (unless already classified as hard loss). These are automatic blocking cautions, not a discretionary invitation to discard noisy cases. Apply the same total guards to primary/lowload aggregate groups and report all per-row medians/ranges/order strata. No gain overrides loss/caution. B/A has no benefit requirement, but defaultOFF losses block promotion. Passing these coarse gates does not prove zero default cost or precise equivalence.

Whole-suite total gain is secondary, not required to match eligible-static gain; total losses still block. Report C/A eligible gains as scoped gains only, never general all-source/DAW/systemCPU savings. Instrumented callback-thread CPU/audio ratio is not the user's under2% system-CPU target. No automatic promotion, threshold changes, repeated calibration or favorable reruns. An independent artifact audit and later explicit decision are required even if all proposed rules pass.

## Boundedness and recommended next action

This plan has52 timing executions if calibration passes:16 identical-baseline executions and36 three-arm executions. The prior frozen pitch Actions run's36 starts spanned604seconds, with median inter-start gap21seconds and maximum22seconds (recorded metadata, not a new timing experiment). On that past host52 runs would be of order15–20minutes; this is only budgeting, not predicted stability or memo performance. Propose fixed90-minute cost-job timeout including fresh builds/tests, with every subprocess bounded and timeout classified incomplete, not shortened workloads/retries. Proof-only inspection job can have a separate fixed15-minute timeout and produce artifacts only.

Recommendation: prepare a qualification-only hosted-library inspection job first; no memoON, calibration, or timing until its actual runtime binding is independently audited. Then a separately approved cost job may use the frozen reviewed fingerprint, rechecking it on its own runner and stopping on drift. Do not put an unaudited automatic 'SSE opcode grep passed' shortcut in the cost job. This keeps ownership of the full experiment while recognizing that external runtime qualification is a prerequisite, not a benchmark result.
