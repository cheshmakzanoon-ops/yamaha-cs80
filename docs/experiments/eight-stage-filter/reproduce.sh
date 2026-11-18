#!/usr/bin/env bash
# Explicitly invoked research reproduction. Never modifies the supplied checkout.
set -euo pipefail
if [[ $# -ne 2 ]]; then
  echo "Usage: bash reproduce.sh SOURCE_CHECKOUT NEW_EMPTY_WORK_DIRECTORY" >&2
  exit 2
fi
here=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
source_root=$(cd -- "$1" && pwd)
work=$2
if [[ -e "$work" ]]; then
  echo "Work directory must not already exist: $work" >&2
  exit 2
fi
mkdir -p -- "$work"
work=$(cd -- "$work" && pwd)
python3 - "$here" "$source_root" "$work" <<'PY'
import hashlib,json,pathlib,shutil,sys
package,source,work=map(pathlib.Path,sys.argv[1:])
pins=json.loads((package/'pinned-hashes.json').read_text())
for rel,expected in pins['stock_dependencies'].items():
    item=source/'src'/rel
    if hashlib.sha256(item.read_bytes()).hexdigest()!=expected:
        raise SystemExit('Stock source hash mismatch: '+rel)
    dest=work/'stock/src'/rel
    dest.parent.mkdir(parents=True,exist_ok=True)
    shutil.copyfile(item,dest)
shutil.copytree(work/'stock',work/'candidate')
PY
# This private repository exists only to make git-apply path handling explicit.
git -C "$work/candidate" init -q
git -C "$work/candidate" apply --check "$here/prototype.patch"
git -C "$work/candidate" apply "$here/prototype.patch"
python3 - "$here" "$work" <<'PY'
import hashlib,json,pathlib,sys
package,work=map(pathlib.Path,sys.argv[1:])
pins=json.loads((package/'pinned-hashes.json').read_text())
for rel,expected in pins['candidate_changes'].items():
    if hashlib.sha256((work/'candidate/src'/rel).read_bytes()).hexdigest()!=expected:
        raise SystemExit('Patched candidate hash mismatch: '+rel)
PY
compiler=${CXX:-c++}
flags=(-O3 -DNDEBUG -std=gnu++20 -fPIC -Wall -Wextra -Wpedantic -ffp-contract=off)
"$compiler" --version > "$work/compiler.txt"
"$compiler" "${flags[@]}" "$here/dispatch-probe.cpp" -o "$work/dispatch-probe"
"$work/dispatch-probe" | tee "$work/dispatch-result.txt"
for mode in normal recovery; do
  defs=()
  if [[ $mode == recovery ]]; then defs=(-DCS80_ENABLE_EXPERIMENTAL_FILTER_RECOVERY=1); fi
  mkdir -p "$work/$mode"
  "$compiler" "${flags[@]}" "${defs[@]}" -I"$work/stock/src" \
    -c "$work/stock/src/core/Ig00156Batch.cpp" -o "$work/$mode/original-four.o"
  "$compiler" "${flags[@]}" "${defs[@]}" -I"$work/candidate/src" \
    -c "$work/candidate/src/core/Ig00156Batch.cpp" -o "$work/$mode/shared-friend-four.o"
  cmp "$work/$mode/original-four.o" "$work/$mode/shared-friend-four.o"
  "$compiler" "${flags[@]}" "${defs[@]}" -I"$work/candidate/src" \
    -c "$work/candidate/src/core/Ig00156Eight.cpp" -o "$work/$mode/eight.o"
  "$compiler" "${flags[@]}" "${defs[@]}" -I"$work/candidate/src" \
    "$here/direct-test.cpp" "$work/$mode/shared-friend-four.o" "$work/$mode/eight.o" \
    -o "$work/$mode/direct-test"
  # Exit77 means unsupported dispatch/build, not a passing differential.
  "$work/$mode/direct-test" | tee "$work/$mode/result.txt"
done
python3 - "$here" "$work" <<'PY'
import json,pathlib,sys
package,work=map(pathlib.Path,sys.argv[1:])
expected=json.loads((package/'observed-results.json').read_text())
for mode,key in [('normal','normal'),('recovery','recovery_enabled')]:
    if (work/mode/'result.txt').read_text().strip()!=expected[key]['result']:
        raise SystemExit('Different result/counts: '+mode)
print('Recorded normal/recovery direct result lines reproduced.')
PY
