#!/bin/bash
set -eu
cd /workspace/scratch/ee447c520988
while [ ! -f /tmp/cs80-dynamic-integrated-evidence/compute-idle.txt ] || [ ! -f cs80-restored/cpu-stage-clock-evidence/compute-idle.txt ] || [ ! -f cs80-restored/cpu-callback-cpu-evidence/compute-idle.txt ]; do sleep 5; done
out=cs80-restored/cpu-dynamic-wide
base=/tmp/cs80-dynamic-base-build/cs80_source_factory_benchmark
candidate=/tmp/cs80-dynamic-integrated-build/cs80_source_factory_benchmark
sha256sum "$base" "$candidate" > "$out/input-hashes.txt"
run(){ name=$1; exe=$2; sha=$3; suite=$4; block=$5; quick=$6; taskset -c 0 "$exe" --out "$out/$name" --source-commit "$sha" --suite "$suite" --rate 48000 --block "$block" --callbacks 256 $quick > "$out/$name.log" 2>&1; echo 0 > "$out/$name-exit.txt"; }
base_sha=b5b252979764c865bbd331827db4415498979e7b
candidate_sha=$(git -C /tmp/cs80-dynamic-integrated rev-parse HEAD)
run wide-baseline "$base" "$base_sha" performance 256 ''
run wide-candidate "$candidate" "$candidate_sha" performance 256 ''
run idle-candidate "$candidate" "$candidate_sha" baseline 128 --quick
run idle-baseline "$base" "$base_sha" baseline 128 --quick
