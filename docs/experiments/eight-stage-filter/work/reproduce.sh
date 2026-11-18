#!/usr/bin/env bash
# Optional FUTURE reproduction, not needed for offline replay.
# Requires the separately pinned Engine V2 inputs. Does not measure time.
set -euo pipefail
: "${V2_SRC:?frozen V2 src directory}"
: "${STOCK_CORE:?shipping libcs80_core.a}"
: "${V2_CORE:?normal V2 libcs80_core.a}"
: "${V2_REPORTING_ENGINE_OBJECT:?frozen reporting-only EngineFilterEight object}"
: "${OUT:?new empty output directory}"
[ "${RUN_UNTIMED:-0}" = 1 ] || { echo 'Set RUN_UNTIMED=1 only for a newly authorized reproduction.'; exit 2; }
HERE="$(cd -- "$(dirname -- "$0")" && pwd)"
python3 - "$HERE" "$V2_SRC" "$STOCK_CORE" "$V2_CORE" "$V2_REPORTING_ENGINE_OBJECT" <<'PY'
import sys,json,hashlib
from pathlib import Path
here,src,stock,core,reported=map(Path,sys.argv[1:]);p=json.loads((here/'provenance.json').read_text())
h=lambda f:hashlib.sha256(f.read_bytes()).hexdigest()
for name,digest in p['core_source_sha256'].items():assert h(src/name)==digest,name
for name,path in [('stock_core',stock),('v2_core',core),('v2_reporting_engine_object',reported)]:assert h(path)==p['artifact_roles'][name]['sha256'],name
PY
mkdir "$OUT"
CXX="${CXX:-c++}"
FLAGS=(-O3 -DNDEBUG -std=gnu++20 -fPIC -I"$V2_SRC" -I"$V2_SRC/core")
for name in Ig00156Batch Ig00156Eight; do
 "$CXX" "${FLAGS[@]}" -ffp-contract=off -Wall -Wextra -Wpedantic -c "$HERE/$name-counted.cpp" -o "$OUT/$name.o"
done
"$CXX" "${FLAGS[@]}" -fno-access-control -c "$HERE/trace.cpp" -o "$OUT/trace.o"
"$CXX" "${FLAGS[@]}" "$OUT/trace.o" "$OUT/Ig00156Batch.o" "$STOCK_CORE" -o "$OUT/trace-stock"
"$CXX" "${FLAGS[@]}" "$OUT/trace.o" "$OUT/Ig00156Batch.o" "$OUT/Ig00156Eight.o" "$V2_REPORTING_ENGINE_OBJECT" "$V2_CORE" -o "$OUT/trace-candidate"
# Resource limits are safety ceilings, not measurements. No clocks are read.
(ulimit -t 60; ulimit -v 4194304; "$OUT/trace-stock" "$OUT/stock.bin" "$OUT/stock.jsonl")
(ulimit -t 60; ulimit -v 4194304; "$OUT/trace-candidate" "$OUT/candidate.bin" "$OUT/candidate.jsonl")
cmp "$OUT/stock.bin" "$OUT/candidate.bin"
python3 -B "$HERE/replay.py" --raw-dir "$OUT" --self-test
