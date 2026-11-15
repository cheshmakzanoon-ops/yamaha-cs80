#!/bin/bash
set -u
cd "$(dirname "$0")"
for mode in native sanitizer; do
 flags='-O3 -march=native'
 [ "$mode" = sanitizer ] && flags='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer'
 echo "g++ -std=c++20 $flags -I../idle-filter-reconstruction/src -I../idle-filter-reconstruction/src/core helper-test.cpp -o helper-$mode" >> commands.txt
 g++ -std=c++20 $flags -I../idle-filter-reconstruction/src -I../idle-filter-reconstruction/src/core helper-test.cpp -o helper-$mode > helper-$mode-build.log 2>&1
 status=$?; echo $status > helper-$mode-build-exit.txt; [ $status = 0 ] || exit $status
 ASAN_OPTIONS=detect_leaks=0 ./helper-$mode run.log > helper-$mode.log 2>&1
 status=$?; echo $status > helper-$mode-exit.txt; cat helper-$mode.log; [ $status = 0 ] || exit $status
done
