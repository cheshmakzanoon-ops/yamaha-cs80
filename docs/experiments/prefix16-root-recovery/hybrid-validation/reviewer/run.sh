#!/bin/bash
set -u
cd "$(dirname "$0")/../.."
r="$PWD/audio-root-hybrid";e="$PWD/hybrid-validation/reviewer"
for mode in generic native sanitizer;do
 flags='-O3';[ "$mode" = native ]&&flags='-O3 -march=native';[ "$mode" = sanitizer ]&&flags='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer'
 g++ -std=c++20 $flags -I"$r/src" "$r/tests/FilterIdleFallbackTests.cpp" -o "$e/$mode" > "$e/$mode-build.log" 2>&1
 s=$?;echo $s > "$e/$mode-build-exit.txt";[ $s = 0 ]||exit $s
 ASAN_OPTIONS=detect_leaks=0 "$e/$mode" "$r/tests/data/ig00156-cap-states.csv" > "$e/$mode.log" 2>&1
 s=$?;echo $s > "$e/$mode-exit.txt";[ $s = 0 ]||exit $s
done
