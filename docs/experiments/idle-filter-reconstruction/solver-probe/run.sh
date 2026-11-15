#!/bin/bash
set -u
cd "$(dirname "$0")"
printf '%s\n' 'g++ -std=c++20 -O3 -DNDEBUG -I../published-main/src -I../published-main/src/core probe.cpp -o probe' > commands.txt
g++ -std=c++20 -O3 -DNDEBUG -I../published-main/src -I../published-main/src/core probe.cpp -o probe > build.log 2>&1
s=$?; echo "$s" > build-exit.txt; [ "$s" = 0 ] || exit "$s"
./probe > run.log 2>&1
s=$?;echo "$s" > run-exit.txt; sha256sum probe instrumented.h probe.cpp > hashes.txt;tail -4 run.log;exit "$s"
