#!/usr/bin/env bash
# Archive-only correctness replay. Optional timing never overwrites retained rows.
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
out="${1:?usage: reproduce.sh NEW_OUTPUT_DIRECTORY [--timing CPU]}"
[ ! -e "$out" ] || { echo 'Output must be new' >&2; exit 2; }
shift;cpu=''
if [ "$#" -gt 0 ]; then
  [ "$#" = 2 ] && [ "$1" = --timing ] || { echo 'Expected --timing CPU' >&2; exit 2; }
  cpu="$2"
fi
mkdir -p "$out";out="$(cd "$out" && pwd)"
cxx="${CXX:-c++}"
{ date -u +%FT%TZ; "$cxx" --version; uname -a; } > "$out/environment.txt"
for variant in scalar-nearest vector; do
  source="$here/rejected/$variant"
  for profile in generic native; do
    flags=(-std=c++20 -O3 -DNDEBUG); [ "$profile" = generic ] || flags+=(-march=native)
    binary="$out/$variant-$profile"
    "$cxx" "${flags[@]}" -I"$source" "$here/harness/tests/OscillatorKneeLookupTests.cpp" -o "$binary"
    "$binary" > "$binary.log" 2>&1;cat "$binary.log"
    if [ -n "$cpu" ]; then
      "$cxx" "${flags[@]}" -I"$source" "$here/harness/tests/OscillatorKneeLookupProbe.cpp" -o "$binary-probe"
      taskset -c "$cpu" "$binary-probe" 32768 9 > "$binary-timing.csv" 2> "$binary-timing.log"
      cat "$binary-timing.log"
    fi
  done
done
# Recompute only the recorded historical matrices. New optional timings are separate.
python3 "$here/summarize.py" --output "$out/recorded-decisions.json"
