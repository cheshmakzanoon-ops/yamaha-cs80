#!/usr/bin/env bash
set -euo pipefail
export MAKEFLAGS=--jobserver-style=pipe
OUT=/tmp/cs80-integration-review
SRC=/workspace/shared/yamaha-filter-simd-integration
BASE=$OUT/baseline
ROOT=/tmp/cs80-portability/apt/root
CMAKE=/tmp/yamaha-dsp-tools/cmake/data/bin/cmake
export LD_LIBRARY_PATH=$ROOT/usr/lib/x86_64-linux-gnu
cp "$SRC/tests/FilterBatchEngineTests.cpp" "$OUT/FilterBatchEngineTests.cpp"
find "$SRC/src" -type f -print0 | sort -z | xargs -0 sha256sum > "$OUT/source-before.sha256"
find "$SRC" -type f \( -iname '*.png' -o -iname '*.jpg' -o -iname '*.webp' \) -print0 | sort -z | xargs -0 -r sha256sum > "$OUT/images-before.sha256"
for name in gcc-baseline gcc-default gcc-on clang-baseline clang-on; do
 source=$SRC; args=(); define=(); compiler=c++
 if [[ $name == *baseline ]]; then source=$BASE; define=(-DCS80_BATCH_BASELINE); fi
 if [[ $name == *-on ]]; then args+=(-DCS80_ENABLE_AVX2_FILTER_BATCH=ON); fi
 if [[ $name == clang* ]]; then compiler=$ROOT/usr/bin/clang++-19; args+=(-DCMAKE_CXX_COMPILER=$compiler -DCMAKE_C_COMPILER=$ROOT/usr/bin/clang-19); fi
 "$CMAKE" -S "$source" -B "$OUT/$name-build" -DCS80_BUILD_PLUGIN=OFF -DCS80_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Release "${args[@]}" > "$OUT/$name-build.log" 2>&1
 targets=(cs80_core)
 if [[ $name == *-on ]]; then targets+=(cs80_ig00156_batch_tests cs80_filter_batch_scheduling_tests);fi
 "$CMAKE" --build "$OUT/$name-build" --target "${targets[@]}" -j2 >> "$OUT/$name-build.log" 2>&1
 if [[ $name == gcc-default ]]; then define+=(-DCS80_DISABLE_AVX2_FILTER_BATCH=1); fi
 if [[ $name == clang-on ]]; then define+=(-DCS80_DISABLE_AVX2_FILTER_BATCH=1); fi
 "$compiler" -std=c++20 -O3 -DNDEBUG "${define[@]}" -I"$source/src" "$OUT/FilterBatchEngineTests.cpp" "$OUT/$name-build/libcs80_core.a" -o "$OUT/$name-engine"
 "$OUT/$name-engine" --write-corpus "$OUT/$name.f32" > "$OUT/$name-engine.log" 2>&1
 if [[ $name == *-on ]]; then
  "$CMAKE" -E chdir "$OUT/$name-build" /tmp/yamaha-dsp-tools/cmake/data/bin/ctest --output-on-failure -R 'cs80_(ig00156_batch_tests|filter_batch_scheduling_tests|filter_batch_admission_tests)$' > "$OUT/$name-focused.log" 2>&1
 fi
done
find "$SRC/src" -type f -print0 | sort -z | xargs -0 sha256sum > "$OUT/source-after.sha256"
find "$SRC" -type f \( -iname '*.png' -o -iname '*.jpg' -o -iname '*.webp' \) -print0 | sort -z | xargs -0 -r sha256sum > "$OUT/images-after.sha256"
cmp "$OUT/source-before.sha256" "$OUT/source-after.sha256"
cmp "$OUT/images-before.sha256" "$OUT/images-after.sha256"
python3 - <<'PY'
import pathlib,hashlib,json
p=pathlib.Path('/tmp/cs80-integration-review');d={'files':{},'comparisons':{}}
for f in p.glob('*.f32'):d['files'][f.stem]={'bytes':f.stat().st_size,'sha256':hashlib.sha256(f.read_bytes()).hexdigest()}
for a,b in [('gcc-baseline','gcc-default'),('gcc-baseline','gcc-on'),('clang-baseline','clang-on')]:
 d['comparisons'][a+'/'+b]=(p/(a+'.f32')).read_bytes()==(p/(b+'.f32')).read_bytes()
(p/'summary.json').write_text(json.dumps(d,indent=2)+'\n');print(json.dumps(d,indent=2))
PY
