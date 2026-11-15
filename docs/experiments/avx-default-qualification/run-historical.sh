#!/bin/bash
set -eu
cd /workspace/scratch/ee447c520988
src=cs80-restored/cpu-avx-default
out=cs80-restored/cpu-avx-default-evidence
taskset -c 1-8 g++ -O2 -std=c++20 -I"$src/src" "$src/src/tools/source_factory_compatibility_probe.cpp" cs80-restored/cpu-avx-default-build/libcs80_core.a -o "$out/compatibility" > "$out/compatibility-build.log" 2>&1
taskset -c 1-8 "$out/compatibility" "$out/historical-audio.bin" > "$out/compatibility.log" 2>&1
sha256sum "$out/historical-audio.bin" > "$out/historical.sha256"
cmp "$out/historical-audio.bin" cs80-restored/ci-source-compat/audio-v2/audio.bin
grep '^08a381f4ac9de1709a9c9197a78baefb12ffac93cd79f76d4b1e92d4f329e195 ' "$out/historical.sha256"
echo PASS > "$out/historical-complete.txt"
