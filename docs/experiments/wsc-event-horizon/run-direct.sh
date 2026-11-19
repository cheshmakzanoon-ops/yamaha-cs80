#!/usr/bin/env bash
# Rebuild and exercise a new isolated direct comparison, not a timing experiment.
set -euo pipefail
if (($# != 2)); then printf 'usage: %s /path/to/repo /new/empty/work-directory\n' "$0" >&2; exit 2; fi
here=$(cd "$(dirname "$0")" && pwd); repo=$(cd "$1" && pwd); work=$2
if [[ -e "$work" ]]; then echo "Refusing existing work path." >&2; exit 2; fi
mkdir -p "$work"; cd "$work"; src=db02cd8fa56892c6ea2cf7df084bcf190204968e
git -C "$repo" show "$src:src/core/ReconstructedOscillator.h" >stock-base.h
python3 "$here/make.py" stock-base.h gen
cp "$here"/{direct.cpp,boundaries.cpp,sizes.cpp} .
flags=(-O3 -DNDEBUG -std=gnu++20 -fPIC -Wall -Wextra -Wpedantic)
for arm in direct witness mutation native-fallback explicit-off edges edges-witness; do
    src=direct.cpp; extra=(-DCS80_WSC_HORIZON_OBSERVE)
    case "$arm" in
      witness|edges-witness) extra+=(-DCS80_WSC_HORIZON_VERIFY);;
      mutation) extra+=(-DCS80_WSC_HORIZON_VERIFY -DCS80_WSC_HORIZON_BAD_BOUNDARY);;
      native-fallback) extra+=(-march=native -DCS80_WSC_HORIZON_EXPECT_OFF);;
      explicit-off) extra+=(-DCS80_WSC_HORIZON_FORCE_OFF -DCS80_WSC_HORIZON_EXPECT_OFF);;
    esac
    case "$arm" in edges|edges-witness) src=boundaries.cpp;; esac
    g++ "${flags[@]}" "${extra[@]}" "$src" -o "$arm" >"$arm-build.log" 2>&1
    rc=0; "./$arm" >"$arm.log" 2>"$arm.stderr" || rc=$?
    if [[ "$arm" == mutation ]]; then ((rc == 1)); grep -Eq 'failures=[1-9][0-9]*.*witnessDisagreements=[1-9][0-9]*' "$arm.log"; else ((rc == 0)); fi
    tail -1 "$arm.log"
done
g++ "${flags[@]}" sizes.cpp -o sizes
./sizes
