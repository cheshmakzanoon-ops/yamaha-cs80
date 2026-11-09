# Final internal-linkage wrapper qualification

Source review: caller-specific macro-dependent processIg00156Four is now a namespace-scope static inline function. Its definitions have internal linkage; the common Ig00156Batch class contains identical aliases and dispatcher declarations across translation units. This resolves the identified wrapper ODR disagreement. No solver or scheduling arithmetic was changed. This does not promise arbitrary mixed-flag interoperability of every pre-existing inline scalar function or exhaustive floating-point sticky-status equivalence.

Focused stage results (zero failures):
- Clang19 default caller + contraction-off baseline backend: 1,126,360 checks, 15,685 accelerated groups.
- Clang19 FMA caller + same baseline backend: 1,142,908 checks, zero accelerated groups.
- GCC14 under qemu-x86_64 Nehalem: 1,126,359 checks, zero accelerated groups.
- GCC14 AArch64 under qemu-aarch64: 1,134,001 checks, zero accelerated groups.

Fresh Clang19 whole-core CMake build explicitly enables CS80_ENABLE_AVX2_FILTER_BATCH=ON. Engine harness passes 4,135,998 checks / 285,328 stereo frames. Its 22,400,288-byte corpus exactly matches preserved final-opt-in Clang original-baseline corpus, SHA256 30dd6762c840465bfce5a0286cf2bf17b04ec38133424519a388f3c875e66557.

Frozen sources and hashes, scripts, logs, objects and executables retained here. Official tool provenance in parent /tmp/cs80-portability. Default option remains OFF. No native ARM timing or unsupported compiler qualification asserted.

## Final whole-Engine emulated requalification

Fresh final-wrapper GCC core builds explicitly ON also pass full Nehalem and AArch64 Engine corpora: 4,135,998 checks, 285,328 stereo frames each, zero failures. Each candidate exactly matches its preserved same-emulator original baseline from the identical <cstdlib>-inclusive harness. Nehalem SHA2224c82dc224efe918ea010e5991d8b7609256c7b2615ef724efc3b2677014da; AArch64 SHAd10f8d05c9abb70071f758657c4bb6ed92bed9b7f307e8fd799636b5761eb97d. See summary.json and final build scripts/logs. No final-wrapper timing rerun.
