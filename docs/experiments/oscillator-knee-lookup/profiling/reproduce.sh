#!/usr/bin/env bash
# Deterministic instrumentation only, NOT a timing comparison or a shipping overlay.
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
repo="$(git -C "$here" rev-parse --show-toplevel)"
out="${1:?usage: reproduce.sh NEW_OUTPUT_DIRECTORY}"
[ ! -e "$out" ] || { echo 'Output must be new' >&2; exit 2; }
mkdir -p "$out"; out="$(cd "$out" && pwd)"
git -C "$repo" archive 35f3431400b28d95d41ef6c6a863db0516edfa01 src | tar -x -C "$out"
patch -d "$out" -p1 < "$here/path-count.patch"
# Change only the scratch CSV destination; never write to the original recorded path.
python3 - "$here/path-count.cpp" "$out/path-count.cpp" <<'PY'
import sys
from pathlib import Path
p=Path(sys.argv[1]).read_text()
old='/workspace/shared/osc-next-cpu-evidence/path-count-workload.csv'
assert p.count(old)==1
Path(sys.argv[2]).write_text(p.replace(old,'path-count-workload.csv'))
PY
cxx="${CXX:-c++}"
flags=(-std=c++20 -O3 -DNDEBUG -g -DCS80_DISABLE_AVX2_FILTER_BATCH=1 -I"$out/src")
for file in Engine FactoryPresets Ig00156Batch; do
  extra=(); [ "$file" != Ig00156Batch ] || extra=(-ffp-contract=off)
  "$cxx" "${flags[@]}" "${extra[@]}" -c "$out/src/core/$file.cpp" -o "$out/$file.o"
done
"$cxx" "${flags[@]}" "$out/path-count.cpp" "$out/Engine.o" "$out/FactoryPresets.o" "$out/Ig00156Batch.o" -o "$out/path-count"
(cd "$out"; ./path-count > path-counts.log)
grep '^COUNTS,' "$out/path-counts.log" > "$out/counts-only.csv"
grep '^COUNTS,' "$here/../evidence/path-counts.log" > "$out/expected-counts.csv"
cmp "$out/expected-counts.csv" "$out/counts-only.csv"
