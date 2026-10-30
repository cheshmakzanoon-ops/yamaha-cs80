#!/usr/bin/env bash
set -euo pipefail
here=$(cd -- "$(dirname -- "$0")" && pwd)
root=$(cd -- "$here/../../.." && pwd)
baseline=932c6d4781cad174d90fcc6297b11e685238393d
# Requires this baseline commit in local Git history. Never silently substitute HEAD.
git -C "$root" cat-file -e "$baseline^{commit}"
python3 - "$root" "$here/results.json" <<'PY'
import hashlib, json, pathlib, sys
root=pathlib.Path(sys.argv[1]); data=json.loads(pathlib.Path(sys.argv[2]).read_text())
for path, expected in data['current_core_sha256'].items():
    actual=hashlib.sha256((root/path).read_bytes()).hexdigest()
    if actual != expected: raise SystemExit(f'Current input changed: {path}; this historical result is not qualification of the new input.')
PY
work=$(mktemp -d "${TMPDIR:-/tmp}/source-performance-compatibility.XXXXXX")
mkdir "$work/baseline"
git -C "$root" archive "$baseline" src/core | tar -x -C "$work/baseline"
"${CXX:-g++}" -std=c++20 -O2 -I"$work/baseline/src" "$here/raw_regression.cpp" "$work/baseline/src/core/Engine.cpp" -o "$work/before"
"${CXX:-g++}" -std=c++20 -O2 -I"$root/src" "$here/raw_regression.cpp" "$root/src/core/Engine.cpp" -o "$work/after"
"$work/before" "$work/before.f32"
"$work/after" "$work/after.f32"
cmp "$work/before.f32" "$work/after.f32"
sha256sum "$work/before.f32" "$work/after.f32"
wc -c "$work/before.f32"
printf 'Exact pre/post comparison passed. Preserved outputs: %s\n' "$work"
