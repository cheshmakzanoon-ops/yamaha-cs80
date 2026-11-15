# Targeted scalar recovery / SIMD admission check

Scratch-only GNU C++20 O3 build, `-ffp-contract=off`, no global AVX/FMA flag and no disabled-batch define. Actual AVX2 kernel required; unavailable backend exits3, not success. run-batch.sh compiles an observed copy of Ig00156Batch.cpp with only three counters: admitted, final cap rejection, successful fifth evaluation. Production header/source unchanged.

512 captured warped states × each of4lane positions:2048 eligible capped calls verified by admitted/cap counters. Other lanes start at reset and converge early. Direct kernel false leaves all12named Memory values and every output sentinel unchanged. Existing wrapper then matches authoritative scalar output+Memory. Includes32subsequent samples, all4lanes failing, duplicate pointers with sequential semantics, and unsupported rate/unwarped/Inf/oversized-input admissions.20k reset-origin noise groups exercise3907 fifth-evaluation successes without false scalar rejection.648362checks passed.

UNOBSERVED build links the identical harness to the actual unmodified production Ig00156Batch.cpp, repeating all numeric/state/sentinel checks.648362checks passed. Its printed success17328 counts ALL vector successes, not specifically fifth evaluations; only observed run can establish fifth count. `intested_positions=0` in logs is a cosmetic label typo meaning ineligible captured positions; observation established zero. Test is correctness/atomicity evidence, not performance, sanitizer or hardware qualification.

Commands:
- bash run-batch.sh
- g++ -std=c++20 -O3 -ffp-contract=off -DUNOBSERVED -I../audio-root-recovery/src batch-cap.cpp ../audio-root-recovery/src/core/Ig00156Batch.cpp -o batch-production
- ./batch-production ../audio-root-recovery/tests/data/ig00156-cap-states.csv
