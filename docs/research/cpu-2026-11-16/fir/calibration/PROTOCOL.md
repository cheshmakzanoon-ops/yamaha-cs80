# FROZEN bounded native stereo-FIR cost screen

Baseline is recovered published5836041138bc1708c3fd2d4240cf63a3e1def4b3, not an earlier experimental lineage. Candidate is qualified stereo FIR only. Native means executing on this CPU, not changing compiler ISA/math flags. Same Release optimization, compiler, AVX/recovery defaults and benchmark instrumentation in both binaries. No publication from instruction counts: exposed3.7%Ir is not predicted CPU saving. Isolated FIR throughput is separate mechanism evidence, never substituted for whole Engine results.

## Preconditions
Owner final ready + all-compute-idle notification, explicit start marker, frozen runner/protocol/analysis hashes. Require focused exactness, generic/native/OFF full Engine/history/state/dispatch and scalar fallback qualification as applicable, source cleanliness and source/flag provenance, both executable hashes. Preserve every run; stop on missing clocks, nonfinite, inventory/workload/checksum mismatch or hash change. CPU0 only, no owned concurrent builds/tests. No source changes or retuning after data.

## Fixed calibration
First run exactly four identical-binary A/A pairs, logical order AB,BA,AB,BA. Both roles use the same qualified baseline executable. Quick performance suite48k/128/256held callbacks,15cases/36rows, including fixed pedal/release durations. Report absolute callback-thread CPU, CPU/audio ratio, all per-case ratios and order strata; these are single-thread budget ratios, not systemCPU%.

Calibration usability guard: total CPU and source-held aggregate ratios in every A/A pair must fall within[0.98,1.02]; every case must fall within[0.90,1.10]. If not, stop before candidate timing: this host/run does not support this bounded modest-benefit screen. Four pairs do not establish1% or2% precision, a distribution or statistical confidence. No repeat calibration to obtain a favorable pass.

Here and below, total CPU ratio is the ratio of sums over all36case/phase rows. The source-held aggregate for each pair is the median of individual C/B ratios across source_performance warmed_held rows, not a ratio of summed source-held CPU. This definition also applies to calibration and E.

Define E as the largest absolute deviation from1 across the eight calibration aggregate ratios (four total, four source-held). This fixed formula is predeclared before observing data; it is only an empirical noise margin, not estimated uncertainty.

## Fixed candidate screen, conditional on usable calibration
Exactly six alternating AB/BA baseline/candidate pairs, same full workload/instrumentation. No adaptive additional rounds, trimming, bias subtraction or treating cases/windows as independent replications.

A practical material-benefit signal requires BOTH total callback CPU and source-held aggregate medians <=0.98 (2%floor), each median improvement strictly >2*E, each aggregate candidate/baseline ratio<1 in at least5/6pairs, and each AB/BA stratum median<1. This is a deliberately bounded engineering screen, NOT a proven2%gain or1%resolution. The previous universal5%bar is not silently reused; the explicit calibration gate/margin and2%floor address this smaller mechanism. If these fail, benefit is unestablished and no further timing/promotion follows.

Hard loss: aggregate median>1.05 with5/6ratios>1, or any case median>1.10 with5/6>1.05. Caution blocks advancement: aggregate median>1.03 with5/6>1; case median>1.05 with5/6>1; or practical hard-limit median with inconsistent direction. Enumerate all36cases, including tails/pedal/Legacy/isolation. Large inconsistent effects require manual inconclusive interpretation. No benefit overrides loss/caution, and near-neutral is not equivalence.

## Conditional low-load
Only after signal plus manual review of all per-case/order/dispersion outcomes: exactly four AB/BA pairs baseline suite48k/128/512held callbacks,11cases/19rows. No low-load stage on failed calibration or benefit. Case median>1.10 with3/4>1.05 hard loss; median>1.05 with3/4>1 caution; inconsistent large median loss also inconclusive. Passing means no gross loss detected in this scope, not universal no-regression.

Actual plugin/callback/user-device qualification remains separate. No under2%systemCPU assertion, publication, or candidate adjustment authorized by this protocol. Protocol and source runner are frozen before data, after independent source review. No execution until owner signals all qualification complete and all compute idle.
