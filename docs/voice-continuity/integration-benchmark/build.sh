#!/usr/bin/env bash
set -euo pipefail
[[ $# == 3 ]] || { echo 'usage: build.sh IMMUTABLE-BASELINE-ROOT IMMUTABLE-CANDIDATE-ROOT NEW-BUILD-DIRECTORY' >&2; exit 2; }
here=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
base=$(cd -- "$1" && pwd)
candidate=$(cd -- "$2" && pwd)
mkdir -- "$3"
out=$(cd -- "$3" && pwd)
flags=(-std=c++20 -O3 -DNDEBUG -fPIC -Wall -Wextra -Wpedantic -Werror)
for arm in base candidate; do
    root=$base
    defines=(-Dcs80=cs80_continuity_off_base -DCS80_OFF_API=offBaselineApi)
    if [[ $arm == candidate ]]; then
        root=$candidate
        defines=(-Dcs80=cs80_continuity_off_candidate -DCS80_OFF_API=offCandidateApi -DCS80_OFF_CANDIDATE=1)
    fi
    for unit in Engine FactoryPresets; do
        "${CXX:-g++}" "${flags[@]}" "${defines[@]}" -I"$root/src" -c "$root/src/core/$unit.cpp" -o "$out/$arm-$unit.o"
    done
    "${CXX:-g++}" "${flags[@]}" "${defines[@]}" -I"$root/src" -I"$here" -c "$here/bridge.cpp" -o "$out/$arm-bridge.o"
done
"${CXX:-g++}" "${flags[@]}" -I"$here" "$here/probe.cpp" "$out/"*.o -o "$out/continuity-off-probe"
{
    "${CXX:-g++}" --version
    printf '\nFlags: %s\n' "${flags[*]}"
    printf '\nPreregistered inputs and probe:\n'
    sha256sum "$here/preregistered.json" "$here/BenchBridge.h" "$here/bridge.cpp" "$here/probe.cpp" "$here/build.sh"
    for arm in base candidate; do
        root=$base; [[ $arm != candidate ]] || root=$candidate
        printf '\n%s source tree:\n' "$arm"
        find "$root/src/core" -type f -print0 | sort -z | xargs -0 sha256sum
    done
    printf '\nObjects and executable:\n'
    sha256sum "$out/"*.o "$out/continuity-off-probe"
} > "$out/premeasurement-sha256.txt"
printf 'Built %s; no measurement started.\n' "$out/continuity-off-probe"
