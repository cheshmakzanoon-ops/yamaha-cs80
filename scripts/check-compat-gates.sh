#!/usr/bin/env bash
# Local mirror of the dsp-measurements.yml compatibility jobs.
#
#   legacy  default-off audio must be byte-identical to the shipped baseline
#   v1      seven-tone source circuit numerics must be byte-identical
#   v2      circuit-engine audio versus the pinned SOURCE_AUDIO_V2 checkpoint
#
# legacy and v1 always fail the script on any difference. v2 is reported only,
# because an intended circuit-engine sound change re-pins it (docs/STATUS.md);
# pass --strict-v2 to make a v2 difference fatal too.
#
# Baseline checkouts are built once and cached in $CS80_COMPAT_CACHE
# (default: ~/.cache/cs80-compat), so repeated runs only rebuild this tree.
set -euo pipefail

strict_v2=0
for arg in "$@"; do
    case "$arg" in
        --strict-v2) strict_v2=1 ;;
        *) echo "usage: $0 [--strict-v2]" >&2; exit 2 ;;
    esac
done

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
workflow="$root/.github/workflows/dsp-measurements.yml"
pin() { sed -n "s/^ *$1: *\([0-9a-f]\{40\}\).*/\1/p" "$workflow" | head -1; }
legacy="$(pin BASELINE)"
v1="$(pin SOURCE_V1)"
v2="$(pin SOURCE_AUDIO_V2)"
[ -n "$legacy" ] && [ -n "$v1" ] && [ -n "$v2" ] || { echo "cannot read pins from $workflow" >&2; exit 2; }

cache="${CS80_COMPAT_CACHE:-$HOME/.cache/cs80-compat}"
mkdir -p "$cache"
cxx="${CXX:-c++}"

checkout() {
    local sha="$1" dir="$cache/$1"
    if [ ! -f "$dir/CMakeLists.txt" ]; then
        git -C "$root" cat-file -e "$sha^{commit}" 2>/dev/null || git -C "$root" fetch --depth=1 origin "$sha"
        rm -rf "$dir"
        git -C "$root" worktree add --detach "$dir" "$sha" >/dev/null
    fi
    echo "$dir"
}

build_core() {
    local src="$1" out="$2"
    cmake -S "$src" -B "$out" -G Ninja -DCMAKE_BUILD_TYPE=Release \
        -DCS80_BUILD_PLUGIN=OFF -DCS80_BUILD_TESTS=OFF >/dev/null
    cmake --build "$out" --target cs80_core >/dev/null
}

# The immutable baseline and current render have independent files and RNGs.
# Run both captures concurrently, wait for both, and propagate either failure
# before comparing their bytes. This does not cache, omit or relax a capture.
run_pair() {
    local baseline_command="$1" current_command="$2" baseline_output="$3" current_output="$4"
    shift 4
    "$baseline_command" "$baseline_output" "$@" &
    local baseline_pid=$!
    "$current_command" "$current_output" "$@" &
    local current_pid=$!
    local baseline_status=0 current_status=0
    wait "$baseline_pid" || baseline_status=$?
    wait "$current_pid" || current_status=$?
    if (( baseline_status != 0 || current_status != 0 )); then
        echo "capture failed: baseline=$baseline_status current=$current_status" >&2
        return 1
    fi
}

work="$root/build-compat-gates"
mkdir -p "$work"
build_core "$root" "$work/current"

status=0

legacy_dir="$(checkout "$legacy")"
[ -f "$cache/$legacy-build/libcs80_core.a" ] || build_core "$legacy_dir" "$cache/$legacy-build"
"$cxx" -O2 -std=c++20 -I"$legacy_dir/src" "$root/src/tools/compatibility_probe.cpp" "$cache/$legacy-build/libcs80_core.a" -o "$work/legacy-probe"
"$cxx" -O2 -std=c++20 -I"$root/src" "$root/src/tools/compatibility_probe.cpp" "$work/current/libcs80_core.a" -o "$work/current-legacy-probe"
for scenario in default tails; do
    args=()
    [ "$scenario" = tails ] && args=(--tails)
    run_pair "$work/legacy-probe" "$work/current-legacy-probe" \
        "$work/legacy-$scenario.f32" "$work/current-$scenario.f32" "${args[@]}"
    if cmp -s "$work/legacy-$scenario.f32" "$work/current-$scenario.f32"; then
        echo "legacy $scenario: byte-identical to ${legacy:0:7}"
    else
        echo "legacy $scenario: DIFFERS from ${legacy:0:7}  (legacy audio must never change)"
        status=1
    fi
done

v1_dir="$(checkout "$v1")"
"$cxx" -O2 -std=c++20 -I"$v1_dir/src" "$root/src/tools/source_factory_circuit_compatibility_probe.cpp" -o "$work/v1-probe"
"$cxx" -O2 -std=c++20 -I"$root/src" "$root/src/tools/source_factory_circuit_compatibility_probe.cpp" -o "$work/current-v1-probe"
run_pair "$work/v1-probe" "$work/current-v1-probe" \
    "$work/circuit-v1.bin" "$work/circuit-current.bin"
if cmp -s "$work/circuit-v1.bin" "$work/circuit-current.bin"; then
    echo "v1 circuit numerics: byte-identical to ${v1:0:7}"
else
    echo "v1 circuit numerics: DIFFER from ${v1:0:7}  (V1 numerics are immutable)"
    status=1
fi

v2_dir="$(checkout "$v2")"
[ -f "$cache/$v2-build/libcs80_core.a" ] || build_core "$v2_dir" "$cache/$v2-build"
"$cxx" -O2 -std=c++20 -I"$v2_dir/src" "$root/src/tools/source_factory_compatibility_probe.cpp" "$cache/$v2-build/libcs80_core.a" -o "$work/v2-probe"
"$cxx" -O2 -std=c++20 -I"$root/src" "$root/src/tools/source_factory_compatibility_probe.cpp" "$work/current/libcs80_core.a" -o "$work/current-v2-probe"
run_pair "$work/v2-probe" "$work/current-v2-probe" \
    "$work/audio-v2.bin" "$work/audio-current.bin"
if cmp -s "$work/audio-v2.bin" "$work/audio-current.bin"; then
    echo "v2 circuit-engine audio: byte-identical to ${v2:0:7}"
else
    echo "v2 circuit-engine audio: differs from ${v2:0:7} (expected only for a documented, re-pinned circuit-engine change)"
    [ "$strict_v2" = 1 ] && status=1
fi

exit "$status"
