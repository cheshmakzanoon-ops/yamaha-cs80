#!/usr/bin/env bash
# Explicit component correctness only; never a CPU timing experiment.
set -euo pipefail
[[ $# == 3 ]] || { echo 'usage: run-component.sh REPO REVISION NEW_OUT' >&2; exit 2; }
repo=$(cd "$1" && pwd); revision=$2; out=$3; here=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
[[ $(git -C "$repo" rev-parse "$revision:src") == 702bf5ee4e375c7b13da7ec95b01b56587bbf75c ]]
mkdir "$out"; out=$(cd "$out" && pwd); mkdir "$out/stock" "$out/candidate" "$out/build"
git -C "$repo" archive "$revision" src | tar -x -C "$out/stock"
cp -a "$out/stock/src" "$out/candidate/"
(cd "$out/candidate" && patch --batch --fuzz=0 -p1 < "$here/candidate.patch") > "$out/patch.log" 2>&1
cp "$here"/{component.cpp,candidate.patch,PLAN.md,RESULT.md,run-component.sh} "$out/"
compiler=${CXX:-g++}; "$compiler" --version > "$out/compiler.txt"
for arm in stock candidate; do
 source="$out/$arm/src/core/Ig00156Batch.cpp"
 command=("$compiler" -std=gnu++20 -O3 -DNDEBUG -fPIC -ffp-contract=off -I"$out/stock/src" -I"$out/stock/src/core" -DBATCH_SOURCE=\""$source"\" "$out/component.cpp" -o "$out/build/$arm")
 printf '%q ' "${command[@]}" > "$out/compile-$arm.argv"; printf '\n' >> "$out/compile-$arm.argv"
 status=0; timeout 180 "${command[@]}" > "$out/compile-$arm.log" 2>&1 || status=$?; echo "$status" > "$out/compile-$arm.exit"; [[ $status == 0 ]]
 status=0; timeout 120 "$out/build/$arm" "$out/$arm.bin" > "$out/$arm.log" 2>&1 || status=$?; echo "$status" > "$out/$arm.exit"; [[ $status == 0 ]]
done
cmp "$out/stock.bin" "$out/candidate.bin"
sha256sum "$out/stock.bin" "$out/candidate.bin" > "$out/transcripts.sha256"
