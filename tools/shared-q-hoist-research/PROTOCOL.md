# Single new-runner shared-Q hoist study

Research only. No production DSP or standard build/test workflow changes.
The old cloud CPU calibrations are terminal and will not be rerun. This is a
different, fresh standard GitHub-hosted ubuntu-24.04 VM, not presumed stable.
The public repository uses free standard hosted runners; no larger runner,
subscription, security configuration change or paid service is requested.

## Frozen inputs and correctness first

Source is the unchanged src/tests/CMake/cmake tree from main220de323, verified
by exact Git object IDs and clean working paths. Compile baseline and isolated
patched copy with the same GNU C++20 -O3 -DNDEBUG; Ig00156Batch uses
-ffp-contract=off. No native/fast-math flags. Record compiler, source/patch,
binary hashes, OS/CPU description and observed child CPU affinity. All processes
run serially on the same first available logical CPU; no multicore DSP.

Before cost: run the paired component probe (four rounding modes/four DAZ/FTZ
modes, invalid-state transaction and full FP flags/errno; probe alone uses
-frounding-math -fsignaling-nans), then both existing
quick full-engine suites at16callbacks. Original events, controls, warmup,
release minima/caps, checks and case selection remain unchanged. The identical
qualification-only driver writes raw PCM; all bytes and all non-timing CSV
fields must match. 19 baseline rows and36 performance rows required. Failure
terminates the study before calibration. This is not full cross-platform or
all22-tab qualification. No original fixture/tolerance may be weakened.

## Native cost gate, fixed before running

Use unmodified stock performance benchmark, --quick --callbacks512
--callback-cpu:48kHz/128frames,15cases/36rows. Primary metric is summed callback
thread CPU divided by summed audio time for Source Performance warmed-held
rows. Record all rows, tails, legacy/isolation controls; not whole-machine CPU,
DAW meter, audio-driver latency or target-hardware certification.

First six baseline-only executions form three consecutive A/A pairs. Each
pair must have primary ratio within2%, all-row aggregate within2%, isolation
held aggregate within3%, and EVERY individual-row CPU ratio within10% of1.
All non-timing fields must agree, callback clock available/positive/finite,
observed callback count correct, full completion/inventory exact. Any failure
is TERMINAL_UNUSABLE_CALIBRATION with ZERO candidate timing executions.

Only after all three calibrations pass, run three pairs in fixed order A/C,
C/A,A/C. Geometric mean candidate/baseline ratios must show at least3% primary
improvement; all-row aggregate and isolation held must not regress over2%;
no individual row may regress over5%. All output/check fields still match.
No discarded row/pair, retry, changed workload, selected faster repeat or
threshold adjustment. A failed gain/regression gate rejects promotion.

One workflow run only, first attempt only; no manual dispatch trigger. The job
requires public repo and standard ubuntu-24.04 runner. It has20min total limit;
the runner has15min total work deadline. Any exception/deadline/cancellation is
incomplete/terminal, never a passing result. Upload raw receipts even on failure
where possible. Retain qualification PCM, CSVs, command/exit records and hashes;
do not upload executables or alter production files. Cost is scoped to this
new VM even if accepted; full source integration/cross-platform checks remain.

Local correctness evidence:131072 paired component cases/28672 invalid cases,
UBSan pass,55 engine rows and15479808 PCM bytes exact. Independently reviewed.
Static division instructions26→12 are NOT measured CPU savings. gprofng's
timer warning means the exploratory profile is not quantitative attribution.

Trapping FP and cross-compiler excess-precision behavior are not qualified.
Execution ledger is saved before each child launch; timeout receipts retain
errors and started/completed counts where the process can finalize. Hard kills
may prevent final receipts and are incomplete, never evidence of zero attempts.
