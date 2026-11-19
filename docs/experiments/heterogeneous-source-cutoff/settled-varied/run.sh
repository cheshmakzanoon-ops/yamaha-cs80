#!/usr/bin/env bash
# Fresh, unprivileged deterministic release-follow-up; no callbacks are timed.
set -euo pipefail
[[ $# == 5 ]] || { echo 'PROJECT SDK STOCK.so CANDIDATE.so NEW_DIRECTORY' >&2; exit 2; }
root=$(realpath "$1"); sdk=$(realpath "$2"); stock=$(realpath "$3"); candidate=$(realpath "$4"); mkdir "$5"; out=$(realpath "$5"); here=$(cd "$(dirname "$0")" && pwd)
cxx=${CXX:-c++}; vg=${VALGRIND:-valgrind}; vh=${VALGRIND_INCLUDE_ROOT:-/usr/include}
"$cxx" -O3 -DNDEBUG -std=gnu++20 -I"$vh" -I"$root/src" -I"$root/tests" -I"$root/docs/experiments/eight-stage-filter/native-cost/host" -I"$sdk" "$here/host.cpp" "$sdk/pluginterfaces/base/coreiids.cpp" "$sdk/pluginterfaces/base/funknown.cpp" "$sdk/public.sdk/source/vst/vstinitiids.cpp" -ldl -o "$out/host"
cd "$out"; mkdir runs home; export HOME="$out/home" XDG_CONFIG_HOME="$out/home/cfg" XDG_CACHE_HOME="$out/home/cache" LC_ALL=C
for kind in s c; do mod="$stock"; if [[ "$kind" == c ]];then mod="$candidate"; fi; ./host "$mod" settled-varied-release first4 "runs/native-$kind"; done
for a in A A2 P S C; do mode=first4; mod="$stock"; case "$a" in P)mode=all5;;S)mode=fifth;;C)mod="$candidate";;esac; tag="guest-$a"
 "$vg" --tool=callgrind --instr-atstart=no --collect-atstart=no --toggle-collect=cs80_counted_process --cache-sim=yes --I1=32768,8,64 --D1=32768,8,64 --LL=8388608,16,64 "--callgrind-out-file=$out/runs/cg-$tag" "--log-file=$out/runs/vg-$tag.log" ./host "$mod" settled-varied-release "$mode" "runs/$tag"; done
cp "$here/verify.py" "$here/count-parser.py" .; python3 verify.py > measured.json
