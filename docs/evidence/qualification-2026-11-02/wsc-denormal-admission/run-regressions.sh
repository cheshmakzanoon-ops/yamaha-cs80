#!/bin/bash
set -eu
cd /workspace/scratch/aa102d573ba3/cs80-cpu-wsc
out=/workspace/scratch/aa102d573ba3/cs80-cpu-evidence/wsc-denormal
printf 'lock acquired: '; date -u +%FT%TZ
for target in OscillatorFastPathOracle OscillatorCache ReconstructedOscillator; do
  g++ -O3 -DNDEBUG -std=gnu++20 -Isrc -Itests "tests/${target}Tests.cpp" -o "$out/$target"
done
"$out/OscillatorFastPathOracle" tests/cpu/wsc-steady-fastpath.json > "$out/original-loop.log" 2>&1
"$out/OscillatorCache" tests/parity/tolerances-v2/kernel-cache.json > "$out/cache-regression.log" 2>&1
"$out/ReconstructedOscillator" no-benchmark > "$out/physical-regression.log" 2>&1
printf 'all three regressions passed: '; date -u +%FT%TZ
