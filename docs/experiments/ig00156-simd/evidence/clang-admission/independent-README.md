# Independent Clang exclusion qualification

Clang19.1.7, Release -O3 -DNDEBUG, two profiles: ordinary strict defaults and -funsafe-math-optimizations. Each profile independently builds untouched original 9f89f8a, current option ON, and current option OFF. All six pass 4,135,998 checks / 285,328 stereo frames with zero failures. Within each profile all three 22,400,288-byte corpora are exact. Strict SHA30dd6762c840465bfce5a0286cf2bf17b04ec38133424519a388f3c875e66557; unsafe SHA3f4f8cfec00cd2aba2791e9afd2781112fbdb2a4e331e7b6e4fdb0667623c941.

Strict and unsafe differ, as expected from the scalar compilation contract; no original/current scalar-context difference was observed within either tested profile. These are corpus results, not proof for every unsafe flag or input. Clang is now always scalar, including when the user requests the CMake option ON. CMake flags confirm PUBLIC CS80_DISABLE_AVX2_FILTER_BATCH=1 and no added contraction-off setting for Clang. Prior Clang SIMD results elsewhere are historical and do not describe current compiler admission.

recheck.sh records exact builds and harness commands. Frozen source, test and hashes, build logs, binaries, core archives and corpora are retained. Tool provenance is in parent /tmp/cs80-portability. No production or test file edited during this independent qualification.
