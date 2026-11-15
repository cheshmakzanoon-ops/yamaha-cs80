#!/bin/bash
set -u
cd "$(dirname "$0")"
g++ -std=c++20 -O3 -ffp-contract=off -I../audio-root-recovery/src batch-cap.cpp batch-observed.cpp -o batch-cap > batch-build.log 2>&1
s=$?;echo $s > batch-build-exit.txt;[ $s = 0 ]||exit $s
./batch-cap ../audio-root-recovery/tests/data/ig00156-cap-states.csv > batch-run.log 2>&1
s=$?;echo $s > batch-run-exit.txt;cat batch-run.log;exit $s
