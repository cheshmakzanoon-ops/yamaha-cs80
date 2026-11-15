#!/bin/bash
set -eu
cd /workspace/scratch/ee447c520988
out=cs80-restored/cpu-next-pairs
base=cs80-restored/ci-source-compat/fix/build-scalar/cs80_source_factory_benchmark
avx=cs80-restored/ci-source-compat/fix/build/cs80_source_factory_benchmark
run(){ label=$1; exe=$2; suite=$3; block=$4; sha=$5; quick=$6; taskset -c 0 "$exe" --out "$out/$label" --source-commit "$sha" --suite "$suite" --rate 48000 --block "$block" --callbacks 256 $quick > "$out/$label.log" 2>&1; echo 0 > "$out/$label-exit.txt"; }
sha=bfbd8f6506836c267911b41cc2fc7295a10860aa
sha256sum "$base" "$avx" /tmp/cs80-lto/bench cs80-restored/cpu-touch-build/cs80_source_factory_benchmark > "$out/input-hashes.txt"
run avxwide-baseline "$base" performance 256 "$sha" ''
run avxwide-candidate "$avx" performance 256 "$sha" ''
run avxidle-candidate "$avx" baseline 128 "$sha" --quick
run avxidle-baseline "$base" baseline 128 "$sha" --quick
run lto-baseline "$base" performance 128 "$sha" --quick
run lto-candidate /tmp/cs80-lto/bench performance 128 "$sha" --quick
run touch-candidate cs80-restored/cpu-touch-build/cs80_source_factory_benchmark performance 128 f37d2174d02b34cb3ae70414a4f964f836681002 --quick
run touch-baseline "$base" performance 128 "$sha" --quick
