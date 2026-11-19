#!/usr/bin/env bash
# Usage: bash run.sh PROJECT_ROOT VST3_SDK_ROOT STOCK_PLUGIN.so COMPARISON_PLUGIN.so NEW_DIRECTORY
set -euo pipefail
[[ $# == 5 ]] || { echo "Expected project, SDK, stock, comparison, new output directory" >&2; exit 2; }
root=$(realpath "$1"); sdk=$(realpath "$2"); stock=$(realpath "$3"); candidate=$(realpath "$4"); mkdir "$5"; out=$(realpath "$5")
here="$(cd "$(dirname "$0")" && pwd)"
cxx="${CXX:-c++}"; vg="${VALGRIND:-valgrind}"; headers="${VALGRIND_INCLUDE_ROOT:-/usr/include}"
"$cxx" -O3 -DNDEBUG -std=gnu++20 -I"$headers" -I"$root/src" -I"$root/tests" -I"$root/docs/experiments/eight-stage-filter/native-cost/host" -I"$sdk" "$here/host.cpp" "$sdk/pluginterfaces/base/coreiids.cpp" "$sdk/pluginterfaces/base/funknown.cpp" "$sdk/public.sdk/source/vst/vstinitiids.cpp" -ldl -o "$out/host"
cd "$out"; sha256sum "$stock" "$candidate" ./host > input-hashes.txt
for s in fresh-quiet held-short; do
 ./host "$stock" "$s" first4 "native-$s-s"; ./host "$candidate" "$s" first4 "native-$s-c"
done
for arm in A A2 P S C; do
 for s in fresh-quiet held-short; do
  count=first4; mod="$stock"; case "$arm" in P) count=all5;; S) count=fifth;; C) mod="$candidate";; esac
  tag="$s-$arm"
  "$vg" --tool=callgrind --instr-atstart=no --collect-atstart=no --toggle-collect=cs80_counted_process --cache-sim=yes --I1=32768,8,64 --D1=32768,8,64 --LL=8388608,16,64 "--callgrind-out-file=$out/cg-$tag" "--log-file=$out/vg-$tag.log" ./host "$mod" "$s" "$count" "guest-$tag"
 done
done
cp "$here/verify.py" .; python3 verify.py > measured.json
