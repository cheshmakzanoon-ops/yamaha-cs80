#!/usr/bin/env bash
set -euo pipefail
ROOT=/tmp/cs80-portability/apt/root
OUT=/tmp/cs80-portability/final-wrapper
SOURCE=/workspace/shared/yamaha-filter-simd
export LD_LIBRARY_PATH=$ROOT/usr/lib/x86_64-linux-gnu
/tmp/yamaha-dsp-tools/cmake/data/bin/cmake -S "$SOURCE" -B "$OUT/gcc-build" -DCS80_ENABLE_AVX2_FILTER_BATCH=ON -DCS80_BUILD_PLUGIN=OFF -DCS80_BUILD_TESTS=OFF -DCMAKE_BUILD_TYPE=Release
/tmp/yamaha-dsp-tools/cmake/data/bin/cmake --build "$OUT/gcc-build" --target cs80_core -j2
c++ -std=c++20 -O3 -DNDEBUG -I"$SOURCE/src" "$SOURCE/tests/FilterBatchEngineTests.cpp" "$OUT/gcc-build/libcs80_core.a" -o "$OUT/qemu-engine"
"$ROOT/usr/bin/qemu-x86_64" -cpu Nehalem "$OUT/qemu-engine" --write-corpus "$OUT/qemu-candidate.f32" > "$OUT/qemu-engine.log"
cmp "$OUT/qemu-candidate.f32" /tmp/cs80-portability/final-opt-in/qemu-baseline.f32
sha256sum "$OUT/qemu-candidate.f32"
