#!/usr/bin/env bash
set -euo pipefail
root=/workspace/scratch/ee447c520988
source="$root/next-product-step"
out="$root/mpe-onset-evidence/compatibility"
mkdir "$out"
for profile in generic native; do
  flags=(-O3 -DNDEBUG)
  if [ "$profile" = native ]; then flags+=(-march=native); fi
  for version in baseline candidate; do
    if [ "$version" = baseline ]; then
      headers="$root/pitch-power-recovery/baseline/src"
      library="$root/pitch-power-recovery/build-baseline-$profile/libcs80_core.a"
    else
      headers="$source/src"
      library="$root/build-mpe-onset-gcc-$profile/libcs80_core.a"
    fi
    set -x
    c++ -std=c++20 "${flags[@]}" -DCS80_DISABLE_AVX2_FILTER_BATCH=1 -Wall -Wextra -Werror -I"$headers" "$source/docs/experiments/mpe-member-onset/CompatibilityCapture.cpp" "$library" -o "$out/$version-$profile"
    "$out/$version-$profile" "$out/$version-$profile.bin" > "$out/$version-$profile.log"
    set +x
  done
  cmp "$out/baseline-$profile.bin" "$out/candidate-$profile.bin"
  sha256sum "$out/"*"-$profile.bin"
done
export PATH="$root/pitch-power-recovery/tools/cmake/data/bin:$root/pitch-power-recovery/tools/bin:$PATH"
export CS80_COMPAT_CACHE="$root/pitch-power-recovery/compat-cache"
export PYTHONDONTWRITEBYTECODE=1
cd "$source"
bash scripts/check-compat-gates.sh --strict-v2
