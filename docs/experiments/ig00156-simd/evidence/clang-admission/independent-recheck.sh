#!/usr/bin/env bash
set -euo pipefail
ROOT=/tmp/cs80-portability/apt/root
OUT=/tmp/cs80-portability/clang-admission
CURRENT=/workspace/shared/yamaha-filter-simd
BASELINE=/workspace/shared/yamaha-filter-simd-baseline
CMAKE=/tmp/yamaha-dsp-tools/cmake/data/bin/cmake
export LD_LIBRARY_PATH=$ROOT/usr/lib/x86_64-linux-gnu
for profile in strict unsafe; do
 flags=(); flagstr=""
 if [[ $profile == unsafe ]]; then flags=(-funsafe-math-optimizations);flagstr=-funsafe-math-optimizations; fi
 for variant in baseline current-on current-off; do
  src=$CURRENT; opt=ON; define=()
  if [[ $variant == baseline ]]; then src=$BASELINE; define=(-DCS80_BATCH_BASELINE); fi
  if [[ $variant == current-off ]]; then opt=OFF; define=(-DCS80_DISABLE_AVX2_FILTER_BATCH=1); fi
  name=$profile-$variant
  "$CMAKE" -S "$src" -B "$OUT/$name-build" -DCS80_ENABLE_AVX2_FILTER_BATCH=$opt -DCS80_BUILD_PLUGIN=OFF -DCS80_BUILD_TESTS=OFF -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER="$ROOT/usr/bin/clang++-19" -DCMAKE_C_COMPILER="$ROOT/usr/bin/clang-19" -DCMAKE_CXX_FLAGS="$flagstr" > "$OUT/$name-build.log" 2>&1
  "$CMAKE" --build "$OUT/$name-build" --target cs80_core -j2 >> "$OUT/$name-build.log" 2>&1
  "$ROOT/usr/bin/clang++-19" -std=c++20 -O3 -DNDEBUG "${flags[@]}" "${define[@]}" -I"$src/src" "$OUT/FilterBatchEngineTests.cpp" "$OUT/$name-build/libcs80_core.a" -o "$OUT/$name-engine"
  "$OUT/$name-engine" --write-corpus "$OUT/$name.f32" > "$OUT/$name-engine.log" 2>&1
 done
 done
python3 - <<'PY'
import pathlib,hashlib,json
p=pathlib.Path('/tmp/cs80-portability/clang-admission'); d={'files':{},'comparisons':{}}
for f in sorted(p.glob('*.f32')):d['files'][f.stem]={'size':f.stat().st_size,'sha256':hashlib.sha256(f.read_bytes()).hexdigest()}
for profile in ['strict','unsafe']:
 for a,b in [('baseline','current-on'),('baseline','current-off'),('current-on','current-off')]:
  x=(p/f'{profile}-{a}.f32').read_bytes();y=(p/f'{profile}-{b}.f32').read_bytes()
  d['comparisons'][f'{profile}:{a}/{b}']={'equal':x==y,'byte_mismatches':sum(v!=w for v,w in zip(x,y)),'length_equal':len(x)==len(y)}
(p/'summary.json').write_text(json.dumps(d,indent=2)+'\n')
print(json.dumps(d,indent=2))
PY
