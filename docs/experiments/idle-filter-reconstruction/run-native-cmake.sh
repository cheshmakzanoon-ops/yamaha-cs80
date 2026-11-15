#!/bin/bash
set -u
cd "$(dirname "$0")"
export PYTHONPATH="$PWD/build-tools"; export PATH="$PWD/build-tools/bin:$PATH"
e="$PWD/idle-native-cmake-evidence";mkdir -p "$e"
targets=(cs80_ig00156_cache_tests cs80_ig00156_nodal_tests cs80_ig00156_filter_tests cs80_active_tail_continuity_tests cs80_source_performance_tests cs80_source_performance_audit_tests cs80_ig00156_batch_tests cs80_filter_batch_engine_tests cs80_filter_batch_scheduling_tests)
cmake --build idle-native-cmake -j2 --target "${targets[@]}" > "$e/build.log" 2>&1
s=$?; echo "$s" > "$e/build-exit.txt"; [ "$s" = 0 ] || exit "$s"
regex=$(IFS='|';echo "${targets[*]}")
ctest --test-dir idle-native-cmake --output-on-failure -j1 -R "^($regex)$" > "$e/ctest.log" 2>&1
s=$?;echo "$s" > "$e/ctest-exit.txt";exit "$s"
