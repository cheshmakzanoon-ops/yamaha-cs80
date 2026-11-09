#!/usr/bin/env bash
set -euo pipefail
ROOT=/tmp/cs80-portability/apt/root
OUT=/tmp/cs80-portability/final-wrapper
export LD_LIBRARY_PATH=$ROOT/usr/lib/x86_64-linux-gnu
cd /workspace/shared/yamaha-filter-simd
$ROOT/usr/bin/clang++-19 -std=c++20 -O2 -mfma -Isrc -c tests/Ig00156BatchTests.cpp -o $OUT/clang-fma-caller.o
$ROOT/usr/bin/clang++-19 -std=c++20 -O2 -ffp-contract=off -Isrc -c src/core/Ig00156Batch.cpp -o $OUT/clang-mixed-backend.o
$ROOT/usr/bin/clang++-19 $OUT/clang-fma-caller.o $OUT/clang-mixed-backend.o -o $OUT/clang-mixed-stage
$OUT/clang-mixed-stage > $OUT/clang-mixed-stage.log
