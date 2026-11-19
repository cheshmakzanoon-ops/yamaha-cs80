#!/usr/bin/env bash
# Usage: bash run-lifecycle.sh PROJECT_ROOT VST3_SDK_ROOT ROUTE.so CUTOFF.so NEW_DIRECTORY
set -euo pipefail
[[ $# == 5 ]] || { echo "Expected project, SDK, stock, cutoff, new output directory" >&2; exit 2; }
root=$(realpath "$1"); sdk=$(realpath "$2"); s=$(realpath "$3"); c=$(realpath "$4"); mkdir "$5"; out=$(realpath "$5")
here="$(cd "$(dirname "$0")" && pwd)"; cxx="${CXX:-c++}"
"$cxx" -O3 -DNDEBUG -std=gnu++20 -I"$root/tools/native-vst3-profile" -I"$root/src" -I"$root/tests" -I"$sdk" "$here/parity-host.cpp" "$sdk/pluginterfaces/base/coreiids.cpp" "$sdk/pluginterfaces/base/funknown.cpp" "$sdk/public.sdk/source/vst/vstinitiids.cpp" -ldl -o "$out/parity_host"
cd "$out"; sha256sum "$s" "$c" ./parity_host > input-hashes.txt
for scene in held8 fresh-idle post-release wake; do
 for arm in route cutoff; do
  mod="$s"; if [[ "$arm" == cutoff ]]; then mod="$c"; fi
  home="$out/home-$scene-$arm"; mkdir "$home"
  HOME="$home" XDG_CONFIG_HOME="$home/config" XDG_CACHE_HOME="$home/cache" ./parity_host "$mod" "$scene" 0 "public-$scene-$arm"
 done
 for f in all.pcm all.fp states.bin scene.json; do cmp "public-$scene-route/$f" "public-$scene-cutoff/$f"; done
done
