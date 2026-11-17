# One-shot packed-pan native cost study

Research-only isolated candidate; production DSP and standard CI remain unchanged.
This is a distinct exact SIMD pan candidate, not a retry of closed Q-hoist,
lazy-drive sanitation, oscillator, FIR or prior cloud calibration experiments.
One fresh free standard ubuntu-24.04 public-repository runner. No paid/larger
runner or subscription; do not presume stable hardware from its designation.
Run number 1 and attempt 1 only, no manual dispatch or retries.

## Frozen inputs and semantics

Exact src/tests/CMake/cmake object IDs in run.py pin unchanged production source.
GNU C++20 -O3 -DNDEBUG, Ig00156Batch -ffp-contract=off, no native/fast-math flags.
Compile original and isolated patched sources with identical compiler/options.
Candidate only packs two finite constant-power pan gains in the existing strict
GNU x86-64 deferred rank loop. Exact sqrtps, not reciprocal approximation.
Original clamp/add/subtract/sqrt/multiply expressions and VCA/mix order remain.
Unsupported builds, exceptional inputs and unmasked exceptions stay scalar;
odd tails are not padded. No global compiler flags or audio contracts change.
Trapping/other-platform behavior outside scalar fallback is not qualified.

Strict paired component probe first: 131072 pairs across rounding/DAZ/FTZ,
random float bits, endpoints, signed zero, NaN/Inf, output and FP flags/errno.
Scheduling probe: false-return nonmutation, all 63 rejected exception-mask
states, nonfinite pairs, counts 0..16, VCA interleaving and odd tails.
Force and verify the scalar-build component fallback separately.

Then existing quick full-engine qualification, 16 callbacks, unchanged events,
warmup, releases and case inventory: 19 baseline + 36 performance rows,
3916800 + 11563008 PCM bytes. All bytes and non-timing fields must match.
Separate correctness-only hit binary verifies positive packed-path successes
balanced by second-rank consumptions and identical performance PCM/work fields.
Counters/logging are absent from uninstrumented cost binaries; binary hashes
must stay unchanged. Reachability is not a speed claim. Any failure stops before
cost. Local independent review verified these controls and workload hits.

## Fixed calibration and acceptance gates

Stock unmodified performance quick benchmark at 48 kHz/128 frames,
512 callbacks, callback-thread CPU clock, 15 cases/36 rows. Pin serial children
to first allowed logical CPU and verify affinity immediately before exec.
Primary: summed callback CPU / summed audio time for Source Performance
warmed-held rows. Retain all modes, tails and rows; not DAW/driver/whole-machine
CPU or target-hardware certification.

Six baseline-only runs form three consecutive A/A pairs. Every pair must have
primary ratio within 2%, all-row aggregate within 2%, isolation warmed-held
within 3%, and every row within 10% of one. Validate all completion, positive
finite clock fields, callback counts, roster and non-timing work fields.
Any failed pair is TERMINAL_UNUSABLE_CALIBRATION, with zero candidate cost runs.

Only after all three pass: exactly three cost pairs A/C, C/A, A/C. Geometric
mean candidate/base primary must be <=0.97 (at least 3% improvement), all-row
and isolation aggregates <=1.02, and every row <=1.05. Otherwise reject.
No selected faster repeat, removed row, changed gate or trial retry.

20-minute job ceiling, 15-minute total runner deadline. Exceptions/timeouts are
terminal incomplete, never passes. Preserve raw receipts even on failure where
possible: source/compiler/platform/protocol/patch/binary hashes, command and
exit ledger, PCM/CSV and decision. Hard kills may prevent final receipts.
A green workflow means the study completed, not the optimization passed.
Even a scoped cost pass requires further integration/platform review and does
not establish general CPU savings. Existing terminal experiments remain closed.
