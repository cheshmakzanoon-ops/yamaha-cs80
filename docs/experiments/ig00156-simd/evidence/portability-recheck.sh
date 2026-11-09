#!/usr/bin/env bash
set -euo pipefail
SOURCE=${SOURCE:-/workspace/shared/yamaha-filter-simd}
ROOT=${ROOT:-/tmp/cs80-portability/apt/root}
OUT=${OUT:-/tmp/cs80-portability}
cd "$SOURCE"
export LD_LIBRARY_PATH="$ROOT/usr/lib/x86_64-linux-gnu${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
CLANG="$ROOT/usr/bin/clang++-19"
"$CLANG" -std=c++20 -O2 -Isrc -c tests/Ig00156BatchTests.cpp -o "$OUT/clang-default-caller.o"
"$CLANG" -std=c++20 -O2 -ffp-contract=off -Isrc -c src/core/Ig00156Batch.cpp -o "$OUT/clang-backend.o"
"$CLANG" "$OUT/clang-default-caller.o" "$OUT/clang-backend.o" -o "$OUT/clang-default-stage"
"$OUT/clang-default-stage" > "$OUT/clang-default-stage.log"
c++ -std=c++20 -O2 -ffp-contract=off -Isrc tests/Ig00156BatchTests.cpp src/core/Ig00156Batch.cpp -o "$OUT/gcc-stage"
"$ROOT/usr/bin/qemu-x86_64" -cpu Nehalem "$OUT/gcc-stage" > "$OUT/qemu-nehalem.log"
PATH="$ROOT/usr/bin:$PATH" "$ROOT/usr/bin/aarch64-linux-gnu-g++-14" --sysroot="$ROOT" -std=c++20 -O2 -Isrc \
 tests/Ig00156BatchTests.cpp src/core/Ig00156Batch.cpp -o "$OUT/arm-stage"
"$ROOT/usr/bin/qemu-aarch64" -L "$ROOT/usr/aarch64-linux-gnu" "$OUT/arm-stage" > "$OUT/qemu-arm.log"
