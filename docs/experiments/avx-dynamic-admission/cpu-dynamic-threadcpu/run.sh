#!/bin/bash
set -eu
cd /workspace/scratch/ee447c520988
while [ ! -f cs80-restored/cpu-stage-clock-pairs/3-candidate-exit.txt ]; do sleep 5; done
out=cs80-restored/cpu-dynamic-threadcpu
s=cs80-restored/cpu-callback-clock
for v in baseline candidate; do
 lib=/tmp/cs80-dynamic-base-build/libcs80_core.a
 [ "$v" = baseline ] || lib=/tmp/cs80-dynamic-integrated-build/libcs80_core.a
 taskset -c 3-4 g++ -std=c++20 -O3 -DNDEBUG -I"$s/src" "$s/src/tools/source_factory_benchmark.cpp" "$lib" -o "$out/$v" > "$out/$v-build.log" 2>&1
done
sha256sum "$out/baseline" "$out/candidate" > "$out/input-hashes.txt"
for n in 1 2 3; do
 order='baseline candidate'; [ "$n" != 2 ] || order='candidate baseline'
 for v in $order; do
  sha=b5b252979764c865bbd331827db4415498979e7b
  [ "$v" = baseline ] || sha=$(git -C /tmp/cs80-dynamic-integrated rev-parse HEAD)
  taskset -c 0 "$out/$v" --out "$out/$n-$v" --source-commit "$sha" --suite performance --rate 48000 --block 128 --callbacks 256 --quick --callback-cpu > "$out/$n-$v.log" 2>&1
  echo 0 > "$out/$n-$v-exit.txt"
 done
done
