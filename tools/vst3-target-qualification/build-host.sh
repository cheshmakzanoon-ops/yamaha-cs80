#!/bin/sh
# Pass the VST3_SDK directory in this project's pinned JUCE checkout.
set -eu
[ "$#" = 2 ] || { echo 'Usage: sh build-host.sh VST3_SDK NEW_HOST_BINARY' >&2; exit 2; }
root=$(CDPATH='' cd -- "$(dirname -- "$0")/../.." && pwd)
[ ! -e "$2" ] || { echo "Output already exists" >&2; exit 1; }
exec "${CXX:-c++}" -std=gnu++20 -O3 -DNDEBUG -fno-fast-math -ffp-contract=off \
    -I"$root/src" -I"$root/tests" -I"$1" "$root/tools/vst3-target-qualification/host.cpp" \
    "$1/pluginterfaces/base/coreiids.cpp" "$1/pluginterfaces/base/funknown.cpp" \
    "$1/public.sdk/source/vst/vstinitiids.cpp" -ldl -o "$2"
