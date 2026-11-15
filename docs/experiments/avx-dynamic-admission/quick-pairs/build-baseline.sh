#!/bin/sh
set -eu
src=/workspace/scratch/ee447c520988/cs80-restored/ci-source-compat/fix
git -C "$src" rev-parse HEAD > /tmp/cs80-avx-dynamic-check/baseline-source.txt
taskset -c 4,5 g++ -O3 -DNDEBUG -fPIC -std=c++20 -ffp-contract=off -I "$src/src" "$src/src/core/Engine.cpp" "$src/src/core/FactoryPresets.cpp" "$src/src/core/Ig00156Batch.cpp" "$src/src/tools/source_factory_benchmark.cpp" -o /tmp/cs80-avx-dynamic-check/bench-baseline
sha256sum /tmp/cs80-avx-dynamic-check/bench /tmp/cs80-avx-dynamic-check/bench-baseline > /tmp/cs80-avx-dynamic-check/binary-sha256.txt
printf 'Matched baseline build finished; no active compute from cpu_profile.\n' > /tmp/cs80-avx-dynamic-check/compute-idle.txt
