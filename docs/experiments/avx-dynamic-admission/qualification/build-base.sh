#!/bin/bash
set -euo pipefail
export PATH=/workspace/scratch/ee447c520988/cs80-restored/build-tools/bin:$PATH
export PYTHONPATH=/workspace/scratch/ee447c520988/cs80-restored/build-tools
cmake -S /tmp/cs80-dynamic-base -B /tmp/cs80-dynamic-base-build -DCMAKE_BUILD_TYPE=Release -DCS80_BUILD_PLUGIN=OFF >/tmp/cs80-dynamic-integrated-evidence/base-configure.log 2>&1
cmake --build /tmp/cs80-dynamic-base-build --parallel 2 --target cs80_source_factory_benchmark >/tmp/cs80-dynamic-integrated-evidence/base-build.log 2>&1
