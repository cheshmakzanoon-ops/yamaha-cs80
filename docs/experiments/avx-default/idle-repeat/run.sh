#!/bin/bash
set -eu
cd /workspace/scratch/ee447c520988
out=cs80-restored/cpu-avx-idle-repeat
base=cs80-restored/ci-source-compat/fix/build-scalar/cs80_source_factory_benchmark
avx=cs80-restored/ci-source-compat/fix/build/cs80_source_factory_benchmark
run(){ label=$1; exe=$2; suite=$3; block=$4; sha=$5; quick=$6; taskset -c 0 "$exe" --out "$out/$label" --source-commit "$sha" --suite "$suite" --rate 48000 --block "$block" --callbacks 256 $quick > "$out/$label.log" 2>&1; echo 0 > "$out/$label-exit.txt"; }
sha=bfbd8f6506836c267911b41cc2fc7295a10860aa
while [ ! -f cs80-restored/cpu-next-pairs/touch-baseline-exit.txt ]; do sleep 5; done
for n in 1 2 3; do
 if [ "$n" = 2 ]; then order="candidate baseline"; else order="baseline candidate"; fi
 for v in $order; do exe="$base"; [ "$v" = baseline ] || exe="$avx"; run "$n-$v" "$exe" baseline 128 "$sha" --quick; done
done
