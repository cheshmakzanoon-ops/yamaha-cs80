#!/bin/bash
set -u
cd "$(dirname "$0")"
g++ -std=c++20 -O3 -ffp-contract=off -I../../audio-root-recovery/src cost.cpp ../../audio-root-recovery/src/core/Ig00156Batch.cpp -o cost > build.log 2>&1
s=$?;echo $s > build-exit.txt;[ $s = 0 ]||exit $s
./cost ../../audio-root-recovery/tests/data/ig00156-cap-states.csv > results.csv
s=$?;echo $s > run-exit.txt;exit $s
