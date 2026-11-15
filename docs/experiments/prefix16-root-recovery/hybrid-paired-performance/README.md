# Whole-engine prefix16 comparison

Six runs, three alternating pairs: baseline c51120121490854bf088c88ebfd64fca83a6d289 versus candidate bc55c11c49d40a62dbc9dd7536cde9bfe6f47a4e. Both fresh AVX2-enabled Release builds with -ffp-contract=off (not global -march=native/FMA). Runtime metadata records selected backend.48kHz,256samples,1024heldcallbacks, performance suite39cases/100phase rows. Runner and input hashes preserved.

All six exits0; all three pairs match100rowkeys and output checksum strings, with zero nonfinite samples. SourcePerformance33 held-case median candidate/baseline ratios0.989510/1.002836/1.006596; release0.980855/0.992068/1.000455. These are medians of per-case median-time ratios, not ratio of overall totals or individual paired-call costs.

No consistent whole-engine speedup. Separate failure-heavy synthetic benchmarks show improvement but cannot be generalized to ordinary engine CPU reduction. No real-time or hardware-fidelity claim. Independent audit passed; see INDEPENDENT-AUDIT.md.
