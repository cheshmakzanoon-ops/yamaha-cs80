#!/bin/bash
set -eu
cd /workspace/scratch/ee447c520988
export PYTHONPATH="$PWD/cs80-restored/build-tools"
export PATH="$PWD/cs80-restored/build-tools/bin:$PATH"
src=$PWD/cs80-restored/cpu-avx-default
out=$PWD/cs80-restored/cpu-avx-default-evidence
b=$PWD/cs80-restored/cpu-avx-explicit-off-build
[ ! -e "$b/CMakeCache.txt" ]
taskset -c 1-8 cmake -S "$src" -B "$b" -DCMAKE_BUILD_TYPE=Release -DCS80_BUILD_PLUGIN=OFF -DCS80_ENABLE_AVX2_FILTER_BATCH=OFF > "$out/off-configure.log" 2>&1
grep '^CS80_ENABLE_AVX2_FILTER_BATCH:BOOL=OFF$' "$b/CMakeCache.txt" > "$out/off-option.txt"
taskset -c 1-8 cmake --build "$b" -j 2 --target cs80_ig00156_batch_tests > "$out/off-build.log" 2>&1
taskset -c 1-8 "$b/cs80_ig00156_batch_tests" > "$out/off-test.log" 2>&1
echo PASS > "$out/off-complete.txt"
