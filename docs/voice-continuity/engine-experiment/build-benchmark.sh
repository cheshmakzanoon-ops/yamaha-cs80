#!/usr/bin/env bash
set -euo pipefail
here=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
[[ $# == 1 ]] || { echo 'usage: build-benchmark.sh VERIFIED-REPLAY-WORK-DIRECTORY' >&2; exit 2; }
work=$(cd -- "$1" && pwd)
[[ $(cat "$work/results/base-commit.txt") == 428e4b2bb0b4bcee0a8f3bb8826449f699bdb618 ]]
mkdir "$work/benchmark-build"
flags=(-std=c++20 -O3 -DNDEBUG -Wall -Wextra -Werror)
for variant in base candidate; do
    src="$work/$variant/src"
    defines=(-Dcs80=cs80_bench_$variant)
    if [[ $variant == candidate ]]; then
        defines+=(-DCS80_BENCH_CANDIDATE=1 -DCS80_BENCH_API=candidateApi)
    else
        defines+=(-DCS80_BENCH_API=baselineApi)
    fi
    ${CXX:-g++} "${flags[@]}" "${defines[@]}" -I"$src" -c "$src/core/Engine.cpp" -o "$work/benchmark-build/$variant-engine.o"
    ${CXX:-g++} "${flags[@]}" "${defines[@]}" -I"$src" -I"$here" -c "$here/engine_continuity_bridge.cpp" -o "$work/benchmark-build/$variant-bridge.o"
done
${CXX:-g++} "${flags[@]}" -I"$here" "$here/engine_continuity_probe.cpp" "$work/benchmark-build/"*.o -o "$work/engine-continuity-probe"
{
    sha256sum "$here/build-benchmark.sh" "$here/BenchmarkBridge.h" "$here/engine_continuity_bridge.cpp" "$here/engine_continuity_probe.cpp" "$work/engine-continuity-probe"
    for variant in base candidate; do
        find "$work/$variant/src/core" -maxdepth 1 -type f -print0 | sort -z | xargs -0 sha256sum
    done
    sha256sum "$work/candidate/src/experimental/"*.h
} > "$work/results/benchmark-source-sha256.txt"
printf 'Built %s/engine-continuity-probe (no measurement run)\n' "$work"
