#!/bin/bash
set -u
cd "$(dirname "$0")";export PYTHONPATH="$PWD/build-tools";export PATH="$PWD/build-tools/bin:$PATH";mkdir -p audio-integrated-evidence
targets=(cs80_filter_batch_cap_tests cs80_filter_idle_fallback_tests cs80_ig00156_cache_tests cs80_ig00156_nodal_tests cs80_ig00156_filter_tests cs80_active_tail_continuity_tests cs80_source_performance_tests cs80_source_performance_audit_tests cs80_ig00156_batch_tests cs80_filter_batch_engine_tests cs80_filter_batch_scheduling_tests)
cmake --build audio-avx-build -j2 --target "${targets[@]}" > audio-integrated-evidence/build.log 2>&1
s=$?;echo "$s" > audio-integrated-evidence/build-exit.txt;[ "$s" = 0 ] || exit "$s"
regex=$(IFS='|';echo "${targets[*]}")
ctest --test-dir audio-avx-build --output-on-failure -j1 -R "^($regex|cs80_filter_(batch_cap|idle_fallback)_(missing|truncated)_tests)$" > audio-integrated-evidence/ctest.log 2>&1
s=$?;echo "$s" > audio-integrated-evidence/ctest-exit.txt;exit "$s"
