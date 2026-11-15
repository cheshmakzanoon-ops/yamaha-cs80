#!/bin/bash
set -euo pipefail
export PATH=/workspace/scratch/ee447c520988/cs80-restored/build-tools/bin:$PATH
export PYTHONPATH=/workspace/scratch/ee447c520988/cs80-restored/build-tools
src=/tmp/cs80-dynamic-integrated
b=/tmp/cs80-dynamic-integrated-build
e=/tmp/cs80-dynamic-integrated-evidence
cmake -S "$src" -B "$b" -DCMAKE_BUILD_TYPE=Release -DCS80_BUILD_PLUGIN=OFF > "$e/configure.log" 2>&1
cmake --build "$b" --parallel 2 --target cs80_ig00156_batch_tests cs80_filter_batch_engine_tests cs80_filter_batch_scheduling_tests cs80_filter_default_history_tests cs80_source_factory_benchmark > "$e/build.log" 2>&1
ctest --test-dir "$b" --output-on-failure -R '^(cs80_ig00156_batch_tests|cs80_filter_batch_engine_tests|cs80_filter_batch_scheduling_tests|cs80_filter_batch_admission_tests|cs80_filter_default_history.*)$' > "$e/ctest.log" 2>&1
g++ -O2 -std=c++20 -I "$src/src" "$src/src/tools/source_factory_compatibility_probe.cpp" "$b/libcs80_core.a" -o "$e/probe"
"$e/probe" "$e/audio.bin"
cmp "$e/audio.bin" /workspace/scratch/ee447c520988/cs80-restored/ci-source-compat/audio-v2/audio.bin
sha256sum "$e/audio.bin" > "$e/corpus.sha256"
g++ -O1 -g -std=c++20 -ffp-contract=off -fsanitize=address,undefined -fno-omit-frame-pointer -I "$src/src" "$src/tests/Ig00156BatchTests.cpp" "$src/src/core/Ig00156Batch.cpp" -o "$e/test-san" > "$e/sanitizer-build.log" 2>&1
ASAN_OPTIONS=detect_leaks=0 "$e/test-san" > "$e/sanitizer.log" 2>&1
printf 'All qualification steps completed\n' > "$e/completed.txt"
