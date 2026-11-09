#!/usr/bin/env bash
set -uo pipefail
source_dir=/workspace/scratch/ee447c520988/sixteen-note-release-impl
build_dir=/workspace/scratch/ee447c520988/build-sixteen-release-core
evidence=/workspace/scratch/ee447c520988/sixteen-release-evidence/core
cmake=/workspace/scratch/ee447c520988/pitch-power-recovery/tools/cmake/data/bin/cmake
ctest=/workspace/scratch/ee447c520988/pitch-power-recovery/tools/cmake/data/bin/ctest
exec 3>"$evidence/full-core-statuses.txt"
{
  date -u +'%Y-%m-%dT%H:%M:%SZ'
  git -C "$source_dir" rev-parse HEAD
  printf '%q ' "$cmake" --build "$build_dir" --parallel 2; printf '\n'
  printf '%q ' "$ctest" --test-dir "$build_dir" --output-on-failure --parallel 2; printf '\n'
  grep -E '^(CMAKE_BUILD_TYPE|CS80_BUILD_PLUGIN|CS80_BUILD_TESTS|CS80_ENABLE_AVX2_FILTER_BATCH|CMAKE_CXX_COMPILER):' "$build_dir/CMakeCache.txt"
} | tee "$evidence/full-core-commands.log"
"$cmake" --build "$build_dir" --parallel 2 2>&1 | tee "$evidence/full-core-build.log"
status=${PIPESTATUS[0]}
printf 'build_exit=%s\n' "$status" >&3
if (( status != 0 )); then exit "$status"; fi
"$ctest" --test-dir "$build_dir" --output-on-failure --parallel 2 2>&1 | tee "$evidence/full-core-ctest.log"
status=${PIPESTATUS[0]}
printf 'ctest_exit=%s\n' "$status" >&3
cp "$build_dir/Testing/Temporary/LastTest.log" "$evidence/full-core-LastTest.log"
if [[ -f "$build_dir/Testing/Temporary/LastTestsFailed.log" ]]; then cp "$build_dir/Testing/Temporary/LastTestsFailed.log" "$evidence/full-core-LastTestsFailed.log"; fi
date -u +'%Y-%m-%dT%H:%M:%SZ' >&3
exit "$status"
