#!/bin/bash
set -euo pipefail
set -x
root=/workspace/shared/cs80-audits/draft1x-design
out=$root/sanitizer
cd "$out"
g++ --version > compiler.txt
find "$root/candidate/src" -type f -print0 | sort -z | xargs -0 sha256sum > source-before.sha256
sha256sum "$root/structural.cpp" "$root/capture.cpp" > harness.sha256
flags=(-std=gnu++20 -O1 -g -fno-omit-frame-pointer -fno-fast-math -ffp-contract=off -fsanitize=address,undefined -fno-sanitize-recover=all -I"$root/candidate/src")
for unit in Engine FactoryPresets Ig00156Batch; do
 g++ "${flags[@]}" -c "$root/candidate/src/core/$unit.cpp" -o "$out/$unit.o"
done
for harness in structural capture; do
 g++ "${flags[@]}" -DDRAFT_CANDIDATE "$root/$harness.cpp" "$out/Engine.o" "$out/FactoryPresets.o" "$out/Ig00156Batch.o" -o "$out/$harness"
done
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
timeout 180 ./structural > structural.log 2>&1
for mode in draft off hq; do
 mkdir "$mode"
 timeout 240 ./capture "$mode" "$out/$mode" > "capture-$mode.log" 2>&1
done
sha256sum -c source-before.sha256 > source-unchanged.log
sha256sum structural capture *.o > binaries.sha256
printf 'PASS\n' > COMPLETED
