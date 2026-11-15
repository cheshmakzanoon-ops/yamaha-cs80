#!/bin/bash
set -u
cd "$(dirname "$0")/../.."
e="$PWD/hybrid-validation/trajectory";r="$PWD/audio-root-hybrid"
for mode in native sanitizer;do
 flags='-O3 -march=native';[ "$mode" = sanitizer ]&&flags='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer'
 echo "g++ -std=c++20 $flags -I$r/src -I$r/src/core $PWD/audio-validation/trajectory.cpp -o $e/$mode" >> "$e/commands.txt"
 g++ -std=c++20 $flags -I"$r/src" -I"$r/src/core" "$PWD/audio-validation/trajectory.cpp" -o "$e/$mode" > "$e/$mode-build.log" 2>&1
 s=$?;echo $s > "$e/$mode-build-exit.txt";[ $s = 0 ]||exit $s
 ASAN_OPTIONS=detect_leaks=0 "$e/$mode" > "$e/$mode.log" 2>&1
 s=$?;echo $s > "$e/$mode-exit.txt";[ $s = 0 ]||exit $s
done
sha256sum "$r/src/core/Ig00156RootFallback.h" "$r/src/core/Ig00156Filter.h" "$PWD/audio-validation/trajectory.cpp" "$PWD/solver-probe/instrumented.h" "$e/native" "$e/sanitizer" > "$e/hashes.txt"
