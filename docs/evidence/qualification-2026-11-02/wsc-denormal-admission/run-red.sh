#!/bin/bash
set -eu
cd /workspace/scratch/aa102d573ba3/cs80-cpu-wsc
out=/workspace/scratch/aa102d573ba3/cs80-cpu-evidence/wsc-denormal
printf 'lock acquired: '; date -u +%FT%TZ
sha256sum src/core/ReconstructedOscillator.h tests/reference/WscLoopOriginal.h tests/WscIndependentLanePreregTests.cpp tests/cpu/wsc-denormal-admission.json > "$out/red-input-sha256.txt"
git diff --binary > "$out/red-test-only.patch"
g++ -O3 -DNDEBUG -std=gnu++20 -frounding-math -Isrc -Itests tests/WscIndependentLanePreregTests.cpp -o "$out/scalar"
"$out/scalar" > "$out/scalar.log" 2>&1
printf 'scalar positive: exit 0\n'
g++ -O3 -DNDEBUG -std=gnu++20 -frounding-math -Isrc -Itests -DCS80_WSC_FOUR_LANE_BACKEND_TEST=1 tests/WscIndependentLanePreregTests.cpp -o "$out/red"
set +e
"$out/red" > "$out/red.log" 2>&1
code=$?
set -e
printf '%s\n' "$code" > "$out/red-exit.txt"
test "$code" -eq 1
grep -q 'WSC denormal-mode actual-admission work gate failed' "$out/red.log"
printf 'extended admission red: expected exit 1\n'
sha256sum "$out/scalar" "$out/red" > "$out/red-binary-sha256.txt"
printf 'completed: '; date -u +%FT%TZ
