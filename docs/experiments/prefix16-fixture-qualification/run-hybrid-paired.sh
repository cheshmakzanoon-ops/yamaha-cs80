#!/bin/bash
set -u
cd "$(dirname "$0")"
e="$PWD/hybrid-paired-performance";mkdir -p "$e"
sha256sum hybrid-avx-build/cs80_source_factory_benchmark audio-avx-build/cs80_source_factory_benchmark audio-root-hybrid/src/core/Ig00156Filter.h audio-root-hybrid/src/core/Ig00156Batch.cpp audio-root-hybrid/src/core/Ig00156RootFallback.h > "$e/input-hashes.txt"
for round in 1 2 3; do
 order='baseline candidate'; [ "$round" != 2 ] || order='candidate baseline'
 for variant in $order; do
  b=audio-avx-build;[ "$variant" != candidate ] || b=hybrid-avx-build
  commit=c51120121490854bf088c88ebfd64fca83a6d289; [ "$variant" != candidate ] || commit=bc55c11c49d40a62dbc9dd7536cde9bfe6f47a4e
  "$b/cs80_source_factory_benchmark" --out "$e/$round-$variant" --source-commit "$commit" --suite performance --rate 48000 --block 256 --callbacks 1024 > "$e/$round-$variant.log" 2>&1
  s=$?;echo "$s" > "$e/$round-$variant-exit.txt";[ "$s" = 0 ] || exit "$s"
 done
done
