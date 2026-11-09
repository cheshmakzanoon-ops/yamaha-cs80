#!/usr/bin/env bash
# Offline only. Never overlays or edits src/. Optional timing preserves all pairs.
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
out="${1:?usage: reproduce.sh NEW_OUTPUT_DIRECTORY [--timing CPU]}"
[ ! -e "$out" ] || { echo 'Output must not exist' >&2; exit 2; }
shift
cpu=''
if [ "$#" -gt 0 ]; then
  [ "$#" = 2 ] && [ "$1" = --timing ] || { echo 'Expected --timing CPU' >&2; exit 2; }
  cpu="$2"
fi
mkdir -p "$out"
out="$(cd "$out" && pwd)"
cxx="${CXX:-c++}"
source="$here/rejected/quotients"
{ date -u +%FT%TZ; "$cxx" --version; uname -a; sha256sum "$source/core/ReconstructedOscillator.h" "$source/tests/"*.cpp "$source/tests/fixtures/"*.h; } > "$out/environment.txt"
for profile in generic native; do
  flags=(-std=c++20 -O3 -DNDEBUG)
  [ "$profile" = generic ] || flags+=(-march=native)
  "$cxx" "${flags[@]}" -I"$source" "$source/tests/OscillatorProfileCacheTests.cpp" -o "$out/tests-$profile"
  "$out/tests-$profile" > "$out/tests-$profile.log" 2>&1
  cat "$out/tests-$profile.log"
  if [ -n "$cpu" ]; then
    "$cxx" "${flags[@]}" -I"$source" "$source/tests/OscillatorProfileCacheProbe.cpp" -o "$out/probe-$profile"
    taskset -c "$cpu" "$out/probe-$profile" 32768 9 > "$out/timing-$profile.csv" 2> "$out/timing-$profile.log"
    cat "$out/timing-$profile.log"
  fi
done
# This validates the committed historical rows, not the fresh optional timing run.
python3 "$here/summarize.py" --output "$out/recorded-decision.json"
