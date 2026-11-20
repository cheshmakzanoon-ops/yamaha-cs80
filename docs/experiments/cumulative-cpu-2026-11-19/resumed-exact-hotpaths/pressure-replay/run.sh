#!/usr/bin/env bash
# Two existing genuine, matched Release VST3 module ELF files, never an instrumented plug-in.
set -euo pipefail
[[ $# == 5 ]] || { echo "Usage: run.sh PROJECT VST3_SDK BASELINE.so CANDIDATE.so NEW_OUT" >&2; exit 2; }
root=$(realpath "$1"); sdk=$(realpath "$2"); stock=$(realpath "$3"); candidate=$(realpath "$4")
mkdir "$5"; out=$(realpath "$5")
here=$(cd "$(dirname "$0")" && pwd); vg=${VALGRIND:-valgrind}; inc=${VALGRIND_INCLUDE_ROOT:-/usr/include}
"${CXX:-c++}" -O3 -DNDEBUG -std=gnu++20 -I"$inc" -I"$root/src" -I"$root/tests" \
 -I"$root/docs/experiments/eight-stage-filter/native-cost/host" -I"$sdk" \
 "$here/pressure-host.cpp" "$sdk/pluginterfaces/base/coreiids.cpp" \
 "$sdk/pluginterfaces/base/funknown.cpp" "$sdk/public.sdk/source/vst/vstinitiids.cpp" \
 -ldl -o "$out/host"
if [[ "$vg" != */* ]]; then vg=$(command -v "$vg"); fi
for role in stock candidate; do
 if [[ "$role" == stock ]]; then module=$stock; else module=$candidate; fi
 python3 "$here/pressure-work.py" take "$module" "$out/$role" --host "$out/host" \
   --valgrind "$vg" --frames 64 --certify --jobs "${JOBS:-2}"
done
python3 "$here/pressure-work.py" compare "$out/stock" "$out/candidate" "$out/comparison.json"
