#!/usr/bin/env bash
set -euo pipefail
ROOT=/tmp/cs80-portability/apt/root
OUT=/tmp/cs80-portability/final-wrapper
SOURCE=/workspace/shared/yamaha-filter-simd
export LD_LIBRARY_PATH=$ROOT/usr/lib/x86_64-linux-gnu
/tmp/yamaha-dsp-tools/cmake/data/bin/cmake -S "$SOURCE" -B "$OUT/clang-build" -DCS80_ENABLE_AVX2_FILTER_BATCH=ON -DCS80_BUILD_PLUGIN=OFF -DCS80_BUILD_TESTS=OFF -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER="$ROOT/usr/bin/clang++-19" -DCMAKE_C_COMPILER="$ROOT/usr/bin/clang-19"
/tmp/yamaha-dsp-tools/cmake/data/bin/cmake --build "$OUT/clang-build" --target cs80_core -j2
"$ROOT/usr/bin/clang++-19" -std=c++20 -O3 -DNDEBUG -I"$SOURCE/src" "$SOURCE/tests/FilterBatchEngineTests.cpp" "$OUT/clang-build/libcs80_core.a" -o "$OUT/clang-engine"
"$OUT/clang-engine" --write-corpus "$OUT/clang-candidate.f32" > "$OUT/clang-engine.log"
cmp "$OUT/clang-candidate.f32" /tmp/cs80-portability/final-opt-in/clang-baseline.f32
sha256sum "$OUT/clang-candidate.f32"
