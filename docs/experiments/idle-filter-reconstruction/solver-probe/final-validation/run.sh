#!/bin/bash
set -u
cd "$(dirname "$0")/../.."
e="$PWD/solver-probe/final-validation";r="$PWD/idle-filter-reconstruction"
export PYTHONPATH="$PWD/build-tools";export PATH="$PWD/build-tools/bin:$PATH"
for mode in generic native sanitizer;do
 flags='-O3';[ "$mode" = native ]&&flags='-O3 -march=native';[ "$mode" = sanitizer ]&&flags='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer'
 echo "g++ -std=c++20 $flags -I$r/src $r/tests/FilterIdleFallbackTests.cpp -o $e/$mode" >> "$e/commands.txt"
 g++ -std=c++20 $flags -I"$r/src" "$r/tests/FilterIdleFallbackTests.cpp" -o "$e/$mode" > "$e/build-$mode.log" 2>&1
 s=$?;echo $s > "$e/build-$mode-exit.txt";[ "$s" = 0 ]||exit "$s"
 ASAN_OPTIONS=detect_leaks=0 "$e/$mode" "$r/tests/data/ig00156-cap-states.csv" > "$e/$mode.log" 2>&1
 s=$?;echo $s > "$e/$mode-exit.txt";[ "$s" = 0 ]||exit "$s"
done
targets=(cs80_filter_idle_fallback_tests cs80_ig00156_cache_tests cs80_ig00156_nodal_tests cs80_ig00156_filter_tests cs80_active_tail_continuity_tests cs80_source_performance_tests cs80_source_performance_audit_tests cs80_ig00156_batch_tests cs80_filter_batch_engine_tests cs80_filter_batch_scheduling_tests)
cmake --build idle-native-cmake -j2 --target "${targets[@]}" > "$e/build-registered.log" 2>&1
s=$?;echo $s > "$e/build-registered-exit.txt";[ "$s" = 0 ]||exit "$s"
targets+=(cs80_filter_idle_fallback_missing_tests cs80_filter_idle_fallback_truncated_tests)
regex=$(IFS='|';echo "${targets[*]}")
ctest --test-dir idle-native-cmake --output-on-failure -j1 -R "^($regex)$" > "$e/ctest.log" 2>&1
s=$?;echo $s > "$e/ctest-exit.txt";tail -5 "$e/ctest.log";exit "$s"
