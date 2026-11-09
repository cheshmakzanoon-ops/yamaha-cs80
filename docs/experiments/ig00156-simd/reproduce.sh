#!/usr/bin/env bash
# Uses local git history only. Output must be a NEW directory.
set -euo pipefail
root=$(cd "$(dirname "$0")/../../.." && pwd)
out=${1:?usage: reproduce.sh NEW_OUTPUT_DIRECTORY}
cmake=${CMAKE:-cmake}
cxx=${CXX:-c++}
mkdir "$out"
out=$(cd "$out" && pwd)
baseline=9f89f8a
mkdir "$out/baseline"
git -C "$root" archive "$baseline" | tar -xf - -C "$out/baseline"
"$cmake" -S "$root" -B "$out/current-build" -DCS80_BUILD_PLUGIN=OFF -DCS80_ENABLE_AVX2_FILTER_BATCH=ON -DCMAKE_BUILD_TYPE=Release
"$cmake" --build "$out/current-build" --target cs80_ig00156_batch_tests cs80_filter_batch_engine_tests cs80_filter_batch_scheduling_tests -j2
"$cmake" -S "$out/baseline" -B "$out/baseline-build" -DCS80_BUILD_PLUGIN=OFF -DCS80_BUILD_TESTS=OFF -DCMAKE_BUILD_TYPE=Release
"$cmake" --build "$out/baseline-build" --target cs80_core -j2
"$cxx" -O3 -DNDEBUG -std=c++20 -DCS80_BATCH_BASELINE -I"$out/baseline/src" \
  "$root/tests/FilterBatchEngineTests.cpp" "$out/baseline-build/libcs80_core.a" -pthread -o "$out/baseline-corpus"
"$out/current-build/cs80_ig00156_batch_tests" | tee "$out/stage.log"
"$out/current-build/cs80_filter_batch_scheduling_tests" | tee "$out/scheduling.log"
"$out/current-build/cs80_filter_batch_engine_tests" --write-corpus "$out/current.raw" | tee "$out/current.log"
"$out/baseline-corpus" --write-corpus "$out/baseline.raw" | tee "$out/baseline.log"
cmp "$out/baseline.raw" "$out/current.raw"
sha256sum "$out/baseline.raw" "$out/current.raw" | tee "$out/corpus-sha256.txt"
# Optional benchmarks are intentionally not CTest gates.
"$cxx" -O3 -DNDEBUG -std=c++20 -I"$root/src" \
  "$root/docs/experiments/ig00156-simd/stage-benchmark.cpp" "$out/current-build/libcs80_core.a" -o "$out/stage-benchmark"
"$cxx" -O3 -DNDEBUG -std=c++20 -I"$root/src" \
  "$root/docs/experiments/ig00156-simd/engine-benchmark.cpp" "$out/current-build/libcs80_core.a" -pthread -o "$out/current-benchmark"
"$cxx" -O3 -DNDEBUG -std=c++20 -DCS80_BATCH_BASELINE -I"$out/baseline/src" \
  "$root/docs/experiments/ig00156-simd/engine-benchmark.cpp" "$out/baseline-build/libcs80_core.a" -pthread -o "$out/baseline-benchmark"
printf 'Optional: %q 500000 > %q\n' "$out/stage-benchmark" "$out/stage-timing.csv"
printf 'Optional: python3 %q %q %q %q\n' "$root/docs/experiments/ig00156-simd/compare-benchmarks.py" "$out/baseline-benchmark" "$out/current-benchmark" "$out/engine-timing.csv"
