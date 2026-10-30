# Exact prechange audio regression

**Passed:** immutable prechange `932c6d4781cad174d90fcc6297b11e685238393d` versus the Source Performance implementation identified by the source hashes in [results.json](results.json). This is a core render comparison, not a genuine-host or measured-hardware claim.

The identical [harness](raw_regression.cpp) runs 84 configurations: three rates (44.1/48/96 kHz), both preexisting Legacy/Isolation policies, and 14 source pairs. Seven timed stages cover notes, pressure, MPE, CC64, ribbon, live source and effective panel changes, releases, a parity transition and MEMORY I/II. The appended parameter stays default-off. No old fixture ordering, parameter values or RNG draws are changed to obtain compatibility.

`cmp` returned zero for **1,448,832 raw float bytes**. Both files have SHA256:

```text
f83e99697ba048f9a3603aafc0445d2b21932b4d6f89c8a5f6f11ce1534b577c
```

Reproduce from the repository root with `bash docs/source-performance/compatibility/reproduce.sh`. The baseline commit must exist locally. The script checks current input hashes, creates a fresh temporary directory, builds both versions using C++20/O2, compares their raw output exactly and preserves the generated files. It does not fetch, install, overwrite historical evidence or remove files. Compiler-specific output may differ across platforms; pre/post equality must not be weakened.

Separately, `tests/SourcePerformanceAuditTests.cpp` passed **2,705 checks**, covering neutral Source Performance versus Isolation bit equality across all 22 tones and PANEL/MEMORY, live EG/model gates, policy precedence/lifecycle, source-local ownership, once-only modern CV overlays, and no ordinary/aligned C++ allocations on the exercised post-prepare core transitions/render path. This does not replace the existing strict seven-tone/V1 and 222-program tests. Host migration, keyboard ownership, native AU and complete real-time qualification remain separate.

Final 2,705-check test also passed AddressSanitizer and UndefinedBehaviorSanitizer (`-O1 -g1 -fno-omit-frame-pointer -fsanitize=address,undefined`). LeakSanitizer cannot operate under this execution environment’s ptrace, so the explicit ASan/UBSan rerun used `ASAN_OPTIONS=detect_leaks=0`. **No leak-check pass is claimed.**
