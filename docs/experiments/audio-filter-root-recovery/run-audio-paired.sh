#!/bin/bash
set -u
cd "$(dirname "$0")"
e="$PWD/audio-paired-performance";mkdir -p "$e"
sha256sum audio-native-build/cs80_source_factory_benchmark idle-native-cmake/cs80_source_factory_benchmark audio-root-recovery/src/core/Ig00156Filter.h audio-root-recovery/src/core/Ig00156Batch.cpp audio-root-recovery/src/core/Ig00156RootFallback.h > "$e/input-hashes.txt"
for round in 1 2 3; do
 order='baseline candidate'; [ "$round" != 2 ] || order='candidate baseline'
 for variant in $order; do
  b=idle-native-cmake;[ "$variant" != candidate ] || b=audio-native-build
  "$b/cs80_source_factory_benchmark" --out "$e/$round-$variant" --source-commit 6f71b0a443d83290ad767a9c9c6f5b732ab5dee2 --suite performance --rate 48000 --block 256 --callbacks 1024 > "$e/$round-$variant.log" 2>&1
  s=$?;echo "$s" > "$e/$round-$variant-exit.txt";[ "$s" = 0 ] || exit "$s"
 done
done
