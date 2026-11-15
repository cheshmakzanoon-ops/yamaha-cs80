#!/bin/bash
set -eu
cd /workspace/scratch/ee447c520988
while [ ! -f /tmp/cs80-thermal-evidence/run.exit ] || [ ! -f cs80-restored/cpu-dynamic-combined-check/exit.txt ]; do sleep 5; done
out=cs80-restored/cpu-dynamic-idle-confirm
exeout=cs80-restored/cpu-dynamic-threadcpu
sha256sum "$exeout/baseline" "$exeout/candidate" > "$out/input-hashes.txt"
for n in 1 2; do
 order='baseline candidate'; [ "$n" != 2 ] || order='candidate baseline'
 for v in $order; do
  sha=b5b252979764c865bbd331827db4415498979e7b
  [ "$v" = baseline ] || sha=$(git -C /tmp/cs80-dynamic-integrated rev-parse HEAD)
  taskset -c 0 "$exeout/$v" --out "$out/$n-$v" --source-commit "$sha" --suite baseline --rate 48000 --block 128 --callbacks 512 --quick --callback-cpu > "$out/$n-$v.log" 2>&1
  echo 0 > "$out/$n-$v-exit.txt"
 done
done
