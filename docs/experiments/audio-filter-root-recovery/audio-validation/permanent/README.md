# Permanent audio fallback regressions

Production source was unchanged by this test integration. `hashes.txt` records inputs and direct-run binaries. `commands.txt` and `run.sh` retain direct compile/run commands; the initial runner stopped on native batch's explicit unavailable status 77, then was corrected to recognize that expected nonqualification. No failing assertions were rerun away.

`FilterIdleFallbackTests.cpp` now requires all 768 captured stage equations to repair under the strict residual gate, retains baseline reproduction checks and named finite continuation checks. Generic, native and ASan/UBSan builds exited 0. Leak detection disabled in sanitizer environment.

`FilterBatchCapTests.cpp` is the production-backend test with strict capture schema/count checks. Generic and sanitizer runs each passed 648,358 checks, 2,048 lane placements and 17,328 successful vector commits. This count is not fifth-iteration attribution. Separate earlier scratch observer evidence supplies that attribution. Native `-march=native` enables the unsupported FMA build contract and returns 77; the explicitly disabled build returns 77 too. Neither qualifies SIMD. Both disabled and ordinary binaries reject missing/truncated data with 2 before dispatch.

Separate generic Release CMake build `audio-cap-test-build`, `CS80_BUILD_PLUGIN=OFF`, `CS80_ENABLE_AVX2_FILTER_BATCH=ON`: the registered production-backend test and missing/truncated negative tests passed 3/3. Native/platform-unavailable runs are registered with SKIP_RETURN_CODE 77 and should be reported as skipped, not as validation passes. No performance measurements in this directory.
