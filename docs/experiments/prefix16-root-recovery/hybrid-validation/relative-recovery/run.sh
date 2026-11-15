#!/bin/bash
set -eu
cd "$(dirname "$0")"
for mode in generic native;do
 flags='-O3 -ffp-contract=off';[ "$mode" != native ]||flags='-O3 -march=native'
 echo "g++ -std=c++20 $flags -I../../audio-root-hybrid/src relative.cpp -o $mode" >> commands.txt
 g++ -std=c++20 $flags -I../../audio-root-hybrid/src relative.cpp -o "$mode" > "$mode-build.log" 2>&1
 "./$mode" > "$mode.log"
 echo "$mode exit0" >> exits.txt
done
sha256sum relative.cpp old-helper.h ../../audio-root-recovery/src/core/Ig00156RootFallback.h ../../audio-root-hybrid/src/core/Ig00156RootFallback.h generic native > hashes.txt
