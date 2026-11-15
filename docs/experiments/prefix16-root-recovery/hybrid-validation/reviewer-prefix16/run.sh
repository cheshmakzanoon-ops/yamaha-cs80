#!/bin/bash
set -u
cd "$(dirname "$0")/../.."
r="$PWD/audio-root-hybrid";e="$PWD/hybrid-validation/reviewer-prefix16"
for mode in generic native sanitizer;do
 flags='-O3';[ "$mode" = native ]&&flags='-O3 -march=native';[ "$mode" = sanitizer ]&&flags='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer'
 g++ -std=c++20 $flags -I"$r/src" "$r/tests/FilterIdleFallbackTests.cpp" -o "$e/$mode" > "$e/$mode-build.log" 2>&1
 s=$?;echo $s > "$e/$mode-build-exit.txt";[ $s = 0 ]||exit $s
 ASAN_OPTIONS=detect_leaks=0 "$e/$mode" "$r/tests/data/ig00156-cap-states.csv" > "$e/$mode.log" 2>&1
 s=$?;echo $s > "$e/$mode-exit.txt";[ $s = 0 ]||exit $s
done
for mode in generic sanitizer;do
 flags='-O3';[ "$mode" = sanitizer ]&&flags='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer'
 for test in edges batch;do
 source="$e/edges.cpp";extra='';[ "$test" = batch ]&&source="$r/tests/FilterBatchCapTests.cpp"&&extra="$r/src/core/Ig00156Batch.cpp"
 g++ -std=c++20 $flags -ffp-contract=off -I"$r/src" "$source" $extra -o "$e/$test-$mode" > "$e/$test-$mode-build.log" 2>&1
 s=$?;echo $s > "$e/$test-$mode-build-exit.txt";[ $s = 0 ]||exit $s
 ASAN_OPTIONS=detect_leaks=0 "$e/$test-$mode" "$r/tests/data/ig00156-cap-states.csv" > "$e/$test-$mode.log" 2>&1
 s=$?;echo $s > "$e/$test-$mode-exit.txt";[ $s = 0 ]||exit $s
 done
done
sha256sum "$r/src/core/Ig00156RootFallback.h" "$r/src/core/Ig00156Filter.h" "$r/src/core/Ig00156Batch.cpp" "$e/edges.cpp" "$e/"{generic,native,sanitizer,edges-generic,edges-sanitizer,batch-generic,batch-sanitizer} > "$e/hashes.txt"
