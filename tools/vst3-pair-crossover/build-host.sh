#!/bin/sh
# Build only the zero-clock worker; no measured mode.
set -eu
[ "$#" = 2 ] || { echo 'usage: sh build-host.sh VST3_SDK NEW_HOST' >&2; exit 2; }
root=$(CDPATH='' cd -- "$(dirname -- "$0")/../.." && pwd)
[ ! -e "$2" ] || { echo 'host already exists' >&2; exit 1; }
exec "${CXX:-c++}" -std=gnu++20 -O3 -DNDEBUG -fno-fast-math -ffp-contract=off \
  -I"$root/src" -I"$root/tests" -I"$1" "$root/tools/vst3-pair-crossover/host.cpp" \
  "$1/pluginterfaces/base/coreiids.cpp" "$1/pluginterfaces/base/funknown.cpp" "$1/public.sdk/source/vst/vstinitiids.cpp" -ldl -o "$2"
