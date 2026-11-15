#!/bin/bash
set -u
cd "$(dirname "$0")";export PYTHONPATH="$PWD/build-tools";export PATH="$PWD/build-tools/bin:$PATH";mkdir -p hybrid-integrated-evidence
cmake -S audio-root-hybrid -B hybrid-avx-build -DCMAKE_BUILD_TYPE=Release -DCS80_BUILD_PLUGIN=OFF -DCS80_BUILD_TESTS=ON -DCS80_ENABLE_AVX2_FILTER_BATCH=ON -DCMAKE_CXX_FLAGS=-ffp-contract=off >hybrid-integrated-evidence/configure.log 2>&1
s=$?;echo "$s" >hybrid-integrated-evidence/configure-exit.txt;[ "$s" = 0 ] || exit "$s"
targets=(cs80_filter_batch_cap_tests cs80_filter_idle_fallback_tests cs80_ig00156_cache_tests cs80_ig00156_nodal_tests cs80_ig00156_filter_tests cs80_active_tail_continuity_tests cs80_source_performance_tests cs80_source_performance_audit_tests cs80_ig00156_batch_tests cs80_filter_batch_engine_tests cs80_filter_batch_scheduling_tests)
cmake --build hybrid-avx-build -j2 --target "${targets[@]}" > hybrid-integrated-evidence/build.log 2>&1
s=$?;echo "$s" > hybrid-integrated-evidence/build-exit.txt;[ "$s" = 0 ] || exit "$s"
regex=$(IFS='|';echo "${targets[*]}")
ctest --test-dir hybrid-avx-build --output-on-failure -j1 -R "^($regex|cs80_filter_(batch_cap|idle_fallback)_(missing|truncated)_tests)$" > hybrid-integrated-evidence/ctest.log 2>&1
s=$?;echo "$s" > hybrid-integrated-evidence/ctest-exit.txt;exit "$s"
