#!/bin/bash
set -eu
cd /workspace/scratch/ee447c520988
out=cs80-restored/cpu-avx-dynamic-pairs
base=/tmp/cs80-avx-dynamic-check/bench-baseline
candidate=/tmp/cs80-avx-dynamic-check/bench
while [ ! -f cs80-restored/cpu-avx-admission-pairs/3-candidate-exit.txt ]; do sleep 5; done
sha256sum "$base" "$candidate" cs80-restored/ci-source-compat/fix/src/core/SourceFactoryTones.h > "$out/input-hashes.txt"
for round in 1 2 3; do
 order='baseline candidate'; [ "$round" != 2 ] || order='candidate baseline'
 for v in $order; do
  exe=$base; sha=bfbd8f6506836c267911b41cc2fc7295a10860aa
  if [ "$v" = candidate ]; then exe=$candidate; sha=$(git -C /tmp/cs80-avx-dynamic rev-parse HEAD); fi
  taskset -c 0 "$exe" --out "$out/$round-$v" --source-commit "$sha" --suite performance --rate 48000 --block 128 --callbacks 256 --quick > "$out/$round-$v.log" 2>&1
  echo 0 > "$out/$round-$v-exit.txt"
 done
done
