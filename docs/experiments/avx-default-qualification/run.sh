#!/bin/bash
set -eu
cd /workspace/scratch/ee447c520988
export PYTHONPATH="$PWD/cs80-restored/build-tools"
export PATH="$PWD/cs80-restored/build-tools/bin:$PATH"
src=$PWD/cs80-restored/cpu-avx-default
out=$PWD/cs80-restored/cpu-avx-default-evidence
b=$PWD/cs80-restored/cpu-avx-default-build
[ ! -e "$b/CMakeCache.txt" ]
taskset -c 1-8 cmake -S "$src" -B "$b" -DCMAKE_BUILD_TYPE=Release -DCS80_BUILD_PLUGIN=OFF > "$out/configure.log" 2>&1
grep '^CS80_ENABLE_AVX2_FILTER_BATCH:BOOL=ON$' "$b/CMakeCache.txt" > "$out/default-option.txt"
taskset -c 1-8 cmake --build "$b" -j 4 --target cs80_ig00156_batch_tests cs80_filter_batch_engine_tests cs80_filter_batch_scheduling_tests cs80_filter_default_history_tests > "$out/build.log" 2>&1
taskset -c 1-8 ctest --test-dir "$b" -V -R '^(cs80_ig00156_batch_tests|cs80_filter_batch_engine_tests|cs80_filter_batch_scheduling_tests|cs80_filter_batch_admission_tests|cs80_filter_default_history.*tests)$' > "$out/ctest.log" 2>&1
taskset -c 1-8 "$b/cs80_filter_batch_engine_tests" --write-corpus "$out/audio.bin" > "$out/corpus.log" 2>&1
sha256sum "$out/audio.bin" > "$out/corpus.sha256"
grep '^08a381f4ac9de1709a9c9197a78baefb12ffac93cd79f76d4b1e92d4f329e195 ' "$out/corpus.sha256"
echo PASS > "$out/complete.txt"
