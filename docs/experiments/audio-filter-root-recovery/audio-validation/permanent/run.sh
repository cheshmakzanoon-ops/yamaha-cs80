#!/bin/bash
set -u
cd "$(dirname "$0")/../.."
r="$PWD/audio-root-recovery";e="$PWD/audio-validation/permanent";input="$r/tests/data/ig00156-cap-states.csv"
for mode in generic native sanitizer;do
 flags='-O3';[ "$mode" = native ]&&flags='-O3 -march=native';[ "$mode" = sanitizer ]&&flags='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer'
 for test in fixture batch;do
 source="$r/tests/FilterIdleFallbackTests.cpp"; extra=''
 [ "$test" = batch ]&&source="$r/tests/FilterBatchCapTests.cpp"&&extra="-ffp-contract=off $r/src/core/Ig00156Batch.cpp"
 echo "g++ -std=c++20 $flags -I$r/src $source $extra -o $e/$test-$mode" >> "$e/commands.txt"
 g++ -std=c++20 $flags -I"$r/src" "$source" $extra -o "$e/$test-$mode" > "$e/$test-$mode-build.log" 2>&1
 s=$?;echo $s > "$e/$test-$mode-build-exit.txt";[ $s = 0 ]||exit $s
 ASAN_OPTIONS=detect_leaks=0 "$e/$test-$mode" "$input" > "$e/$test-$mode.log" 2>&1
 s=$?;echo $s > "$e/$test-$mode-exit.txt";if [ $s != 0 ];then [ "$test" = batch ]&&[ "$mode" = native ]&&[ $s = 77 ]||exit $s;fi
 done
done
g++ -std=c++20 -O3 -ffp-contract=off -DCS80_DISABLE_AVX2_FILTER_BATCH=1 -I"$r/src" "$r/tests/FilterBatchCapTests.cpp" "$r/src/core/Ig00156Batch.cpp" -o "$e/batch-disabled" > "$e/disabled-build.log" 2>&1
s=$?;echo $s > "$e/disabled-build-exit.txt";[ $s = 0 ]||exit $s
"$e/batch-disabled" "$input" > "$e/disabled.log" 2>&1
s=$?;echo $s > "$e/disabled-exit.txt";[ $s = 77 ]||exit 1
for binary in fixture-generic batch-generic batch-disabled;do
 for input in "$r/tests/data/ig00156-cap-states-missing.csv" "$r/tests/data/ig00156-cap-states-truncated.csv";do
 "$e/$binary" "$input" >> "$e/negative.log" 2>&1;s=$?;echo "$binary $input exit=$s" >> "$e/negative.log";[ $s = 2 ]||exit 1
 done
done
sha256sum "$r/tests/FilterIdleFallbackTests.cpp" "$r/tests/FilterBatchCapTests.cpp" "$r/src/core/Ig00156Filter.h" "$r/src/core/Ig00156Batch.cpp" "$r/src/core/Ig00156RootFallback.h" "$e/fixture-"{generic,native,sanitizer} "$e/batch-"{generic,native,sanitizer,disabled} > "$e/hashes.txt"
