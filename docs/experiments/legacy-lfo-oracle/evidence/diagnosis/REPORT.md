# Native legacy SUB LFO investigation

No production, repository test, or assertion changes. GCC 14.2.0, C++20, -O3 -DNDEBUG, generic or -march=native, CS80_DISABLE_AVX2_FILTER_BATCH=1. Baseline headers and libraries were used together, from pitch-power-recovery/baseline at 4ef0666e3aa5b6f4157afb403f538b82c5af1f50. All diagnostic executions pinned to CPUs 0-3; no timing measurements.

## Findings
The instrumented copy of complete CoreTests reproduces exactly four failures among 11,508 checks. Waves 0, 1, 2, 3 first diverge at host frame 398 (zero-based). The output sample still equals the oracle at that frame; the speed alone differs: oracle 0x1.89e99cp+2, Engine 0x1.89e99ep+2. This is one float ULP. Waves 4 and 5 pass.

SpeedModels.cpp independently tracks explicit std::fma(7-x, .0015f, x) and a separated multiply/add with a volatile product. For all six waveforms, native Engine matches fused speed through all 512 frames, while separated speed first differs at frame 398. Generic Engine instead matches separated speed through all 512 frames, and fused first differs at frame 398.

The full native test disassembly directly confirms separate vsubss/vmulss/vaddss recurrence at addresses 0x238f8..0x23920 in testOriginalSubOscillator's waves-0-through-3 path. Thus identical source expressions in the DUT and oracle do not imply identical contraction under this compilation profile. This is a pre-existing test/profile contraction-contract discrepancy, not evidence that the pitch cache changed LFO behavior.

A scratch-only full CoreTests copy replaces only the oracle recurrence with a noinline helper containing the same expression. Against the unchanged native baseline library it passes all 11,508 checks. No assertions were relaxed. This is a diagnostic control, NOT a recommended portable correctness fix: noinline is not a language-level guarantee of identical contraction across compilers.

## Proposed next step, not implemented
Specify the oracle's floating-point arithmetic contract before changing this test. A helper that explicitly models the selected build's contracted or separate recurrence can preserve exact comparisons, provided the Engine compilation contract is independently verified and both profiles exercised. Merely disabling contraction in the test TU is not a valid fix for a contracted DUT. Alternatively, a separately justified numerical-error contract would require measured/proved bounds across the waveform/phase domain; this investigation does not justify blanket tolerance relaxation. Do not silently alter shipping Engine arithmetic just to satisfy the oracle.

The historical default integration log concerns distinct modern/scope tests and flags, so does not contradict this baseline CoreTests native failure. This investigation establishes the speed discrepancy for the specified 512-frame workload, not universal equivalence of contracted/noncontracted waveforms or a complete compiler-proof replacement oracle.

## Artifacts
CoreTestsInstrumented.cpp and core-native.log retain first mismatch and original assertions. SpeedModels.cpp and speed-generic.log/speed-native.log retain the explicit recurrence comparison. CoreTestsOutlined.cpp and core-outlined-native.log retain the unchanged-assertion diagnostic pass. core-native.asm contains objdump -d -C output of core-native. Binary/source/log hashes are recorded separately.
