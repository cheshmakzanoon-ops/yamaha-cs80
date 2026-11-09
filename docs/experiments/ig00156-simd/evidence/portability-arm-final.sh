#!/usr/bin/env bash
set -euo pipefail
SOURCE=${SOURCE:-/workspace/shared/yamaha-filter-simd}
BASELINE=${BASELINE:-/workspace/shared/yamaha-filter-simd-baseline}
ROOT=${ROOT:-/tmp/cs80-portability/apt/root}
OUT=${OUT:-/tmp/cs80-portability}
CMAKE=${CMAKE:-/tmp/yamaha-dsp-tools/cmake/data/bin/cmake}
export LD_LIBRARY_PATH="$ROOT/usr/lib/x86_64-linux-gnu${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export PATH="$ROOT/usr/bin:$PATH"
for variant in candidate; do
 src="$SOURCE";define=()
 if [[ $variant == baseline ]]; then src="$BASELINE";define=(-DCS80_BATCH_BASELINE);fi
 "$CMAKE" -S "$src" -B "$OUT/arm-$variant-build" -DCS80_ENABLE_AVX2_FILTER_BATCH=ON -DCS80_BUILD_PLUGIN=OFF -DCS80_BUILD_TESTS=OFF \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_SYSTEM_NAME=Linux -DCMAKE_SYSTEM_PROCESSOR=aarch64 \
  -DCMAKE_SYSROOT="$ROOT" -DCMAKE_CXX_COMPILER="$ROOT/usr/bin/aarch64-linux-gnu-g++-14" \
  -DCMAKE_C_COMPILER="$ROOT/usr/bin/aarch64-linux-gnu-gcc-14"
 "$CMAKE" --build "$OUT/arm-$variant-build" --target cs80_core -j2
 "$ROOT/usr/bin/aarch64-linux-gnu-g++-14" --sysroot="$ROOT" -std=c++20 -O3 -DNDEBUG \
  "${define[@]}" -I"$src/src" "$SOURCE/tests/FilterBatchEngineTests.cpp" \
  "$OUT/arm-$variant-build/libcs80_core.a" -o "$OUT/arm-$variant-engine"
done
for variant in candidate; do
 "$ROOT/usr/bin/qemu-aarch64" -L "$ROOT/usr/aarch64-linux-gnu" "$OUT/arm-$variant-engine" \
  --write-corpus "$OUT/arm-$variant.f32" > "$OUT/arm-$variant-engine.log"
done
cmp "$OUT/arm-candidate.f32" /tmp/cs80-portability/final-opt-in/arm-baseline.f32
sha256sum "$OUT/arm-"*.f32
