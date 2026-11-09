# Independent final integration verification

Candidate HEAD f7292e3c8fa0d8d84894ab9ed25085c13d740e49. Untouched actual remote-main baseline 68fcb884d49afb2a938dd703328529fca06f331d extracted using git archive. All builds fresh; MAKEFLAGS=--jobserver-style=pipe and -j2.

Five complete Engine runs: GCC baseline, GCC candidate with option omitted, GCC candidate ON, Clang19 baseline, Clang19 candidate ON. Every run passes 4,135,998 checks / 285,328 stereo frames / zero failures. Every 22,400,288-byte corpus has SHA256 30dd6762c840465bfce5a0286cf2bf17b04ec38133424519a388f3c875e66557; same-compiler baseline/candidate comparisons are exact.

Option/compile evidence:
- GCC omitted: cache CS80_ENABLE_AVX2_FILTER_BATCH=OFF, PUBLIC CS80_DISABLE_AVX2_FILTER_BATCH=1.
- GCC ON: cache ON, no disable definition, ordinary scalar flags -O3 -DNDEBUG unchanged.
- Clang ON: cache ON, PUBLIC CS80_DISABLE_AVX2_FILTER_BATCH=1, scalar-only; no contraction-off scalar flag added.

Focused stage/scheduling/admission CTest: 3/3 pass each for GCC ON and Clang ON. GCC stage 1,126,360 checks, 15,685 accelerated groups. Clang stage 1,142,908 checks, zero accelerated groups. Both scheduling 869,465 checks / 135,454 timeline frames (four Engine variants). Admission test output retained in each build's Testing/Temporary/LastTest.log.

Before/after hashes match for all 166 src files and 70 PNG/JPG/WebP files. No production/source/screenshots edited; no push. Build logs, cache/flags, scripts, executables, archives and corpora are under this directory. Only upstream docs may have changed while tests ran. No universal performance claim.
