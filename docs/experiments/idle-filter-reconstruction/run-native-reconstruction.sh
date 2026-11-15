#!/bin/bash
set -eu
cd "$(dirname "$0")"
r="$PWD/idle-filter-reconstruction"; e="$PWD/idle-native-qualification"; mkdir -p "$e"; exec > "$e/runner.log" 2>&1
flags=(-std=c++20 -O3 -march=native -DNDEBUG -DCS80_DISABLE_AVX2_FILTER_BATCH=1 -I"$r/src")
for unit in Engine FactoryPresets Ig00156Batch; do
 extra=(); [ "$unit" != Ig00156Batch ] || extra=(-ffp-contract=off)
 echo "BUILD $unit"; g++ "${flags[@]}" "${extra[@]}" -c "$r/src/core/$unit.cpp" -o "$e/$unit.o"
done
for test in Ig00156CacheTests Ig00156NodalTests ActiveTailContinuityTests SourcePerformanceTests SourcePerformanceAuditTests Ig00156BatchTests FilterBatchEngineTests FilterBatchSchedulingTests; do
 echo "BUILD $test"; g++ "${flags[@]}" "$r/tests/$test.cpp" "$e/Engine.o" "$e/FactoryPresets.o" "$e/Ig00156Batch.o" -o "$e/$test" > "$e/$test-build.log" 2>&1
 echo "RUN $test"; set +e; "$e/$test" > "$e/$test.log" 2>&1; s=$?; set -e; echo "$s" > "$e/$test-exit.txt"; [ "$s" = 0 ] || exit "$s"
done
echo COMPLETE
