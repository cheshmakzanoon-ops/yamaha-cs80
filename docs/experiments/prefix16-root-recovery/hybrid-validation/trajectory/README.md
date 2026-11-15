# Prefix16 broad trajectory validation

Native and ASan/UBSan runs each passed 432 cases / 1,769,472 samples: zero mismatches before the baseline's first capped failure in each case, zero nonfinite output or named-memory values. Both builds and runs exited 0. Leakage checking disabled as established in environment; address and undefined behavior instrumentation enabled. Initial build failed solely for missing src/core include search path; original log retained, corrected commands recorded. Production files unchanged by reviewer.

Baseline capped totals differ by compiler math configuration: native 6347/752/631 versus sanitizer 6420/740/631 for unwarped 3 kHz / warped 88.2 kHz / warped 96 kHz. Do not conflate these with candidate solver failure counts: this harness counts baseline failures. Changed sample counts 190192/30380/24178 in both builds reflect divergence after repair, not failed parity. No convergence or exact-equivalence claim after first baseline failure. All tested candidate continuation states remain finite.

IMPORTANT: reused harness emits CAP rows combining candidate pre-state with baseline equation coefficients. After trajectory divergence these rows are not a consistent replayable capture. Do not use them as replacement fixture data or root equation/state evidence. Only the SUMMARY/CLOCK/CHANGED observations above are used here. Frozen original captured fixtures remain separate.

Source/binary provenance in hashes.txt; commands in commands.txt/run.sh. No timing or hardware fidelity claim.
