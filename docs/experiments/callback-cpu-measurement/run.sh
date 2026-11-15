#!/bin/bash
set -eu
cd /workspace/scratch/ee447c520988
s=cs80-restored/cpu-callback-clock
o=cs80-restored/cpu-callback-cpu-evidence
lib=cs80-restored/cpu-avx-default-build/libcs80_core.a
# Production core unchanged from b5b25297; reuse qualified library.
taskset -c 3-4 g++ -std=c++20 -O3 -DNDEBUG -I$s/src $s/tests/CallbackCpuMeasurementTests.cpp -o $o/unit > $o/unit-build.log 2>&1
taskset -c 3-4 $o/unit > $o/unit.log 2>&1
taskset -c 3-4 g++ -std=c++20 -O3 -DNDEBUG -I$s/src $s/src/tools/source_factory_benchmark.cpp $lib -o $o/benchmark > $o/build.log 2>&1
taskset -c 3-4 g++ -std=c++20 -O3 -DNDEBUG -I$s/src cs80-restored/cpu-avx-publish/src/tools/source_factory_benchmark.cpp $lib -o $o/baseline > $o/baseline-build.log 2>&1
taskset -c 3-4 python $s/tests/test_callback_cpu_benchmark.py --benchmark $PWD/$o/benchmark --baseline $PWD/$o/baseline > $o/schema.log 2>&1
taskset -c 3-4 python $s/tests/test_benchmark_preflight.py --benchmark $PWD/$o/benchmark > $o/preflight.log 2>&1
echo PASS > $o/complete.txt
date -u > $o/compute-idle.txt
