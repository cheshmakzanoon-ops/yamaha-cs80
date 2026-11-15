#!/bin/bash
set -u
cd "$(dirname "$0")"
e="$PWD/idle-paired-performance";mkdir -p "$e"
sha256sum idle-native-cmake/cs80_source_factory_benchmark baseline-native-cmake/cs80_source_factory_benchmark idle-filter-reconstruction/src/core/Ig00156{Filter,RootFallback}.h > "$e/input-hashes.txt"
for round in 1 2 3; do
 order='baseline candidate'; [ "$round" != 2 ] || order='candidate baseline'
 for variant in $order; do
  b=baseline-native-cmake;[ "$variant" != candidate ] || b=idle-native-cmake
  "$b/cs80_source_factory_benchmark" --out "$e/$round-$variant" --source-commit 28999229df3662d8cf032e6dc38a0b12403aca06 --suite performance --rate 48000 --block 256 --callbacks 1024 > "$e/$round-$variant.log" 2>&1
  s=$?;echo "$s" > "$e/$round-$variant-exit.txt";[ "$s" = 0 ] || exit "$s"
 done
done
