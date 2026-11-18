#!/usr/bin/env bash
# Untimed, isolated GNU/Linux reproducer. This convenience script was not run
# during the editorial export; the archived probes were run by equivalent steps.
set -euo pipefail

if [[ $# != 2 ]]; then
    echo "usage: bash reproduce.sh REPOSITORY NEW_OUTPUT_DIRECTORY" >&2
    exit 2
fi
here=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
repo=$(git -C "$1" rev-parse --show-toplevel)
out=$2
cxx=${CXX:-g++}
base=e9e8175f4278d2457279fcb8809cbba1ddb7be2b

[[ $(uname -s) == Linux && $(uname -m) == x86_64 ]] || {
    echo "This reproducer requires glibc Linux x86-64." >&2; exit 2;
}
getconf GNU_LIBC_VERSION >/dev/null
[[ $("$cxx" -dumpfullversion) == 14.2.0 ]] || {
    echo "The recorded code generation requires GNU C++ 14.2.0." >&2; exit 2;
}
compiler_macros=$("$cxx" -dM -E -x c++ /dev/null)
grep -q '^#define __GNUC__ 14$' <<< "$compiler_macros" || {
    echo "A GNU C++ 14 compiler is required." >&2; exit 2;
}
git -C "$repo" cat-file -e "$base^{commit}"
[[ ! -e $out ]] || { echo "Output directory must not exist: $out" >&2; exit 2; }
mkdir -p -- "$out"
out=$(cd -- "$out" && pwd)
mkdir -- "$out/stock"
git -C "$repo" archive "$base" src | tar -xf - -C "$out/stock"
cp -R -- "$out/stock" "$out/v1"
cp -R -- "$out/stock" "$out/v2"
patch --batch -d "$out/v1" -p1 < "$here/v1.patch"
patch --batch -d "$out/v2" -p1 < "$here/v2.patch"

check_header() {
    local arm=$1 digest=$2
    printf '%s  %s\n' "$digest" "$out/$arm/src/core/ReconstructedOscillator.h" | sha256sum -c -
}
check_header stock c037a8970f939bdcd7d98ea56348006f82f2412cc64cd912fbf0f85b5ffa9373
check_header v1 4e8648b29619a566a92bac01b8ed5ff1663b9311a4424491900113a521386680
check_header v2 37be5b062e113f8b66dc37c05d1c4e73616c4040e56b8107fe4f7cf84fbfeac2
"$cxx" --version > "$out/compiler.txt"
flags=(-O3 -DNDEBUG -std=gnu++20 -fPIC -Wall -Wextra -Wpedantic)
"$cxx" "${flags[@]}" -ffp-contract=off -I "$out/stock/src" \
    -c "$out/stock/src/core/Ig00156Batch.cpp" -o "$out/Ig00156Batch.o"
for arm in stock v1 v2; do
    "$cxx" "${flags[@]}" -I "$out/$arm/src" \
        -c "$out/$arm/src/core/Engine.cpp" -o "$out/$arm/Engine.o"
    "$cxx" -shared -Wl,-Bsymbolic "$out/$arm/Engine.o" "$out/Ig00156Batch.o" \
        -o "$out/$arm/core.so"
done
for probe in output elapsed; do
    "$cxx" -O3 -DNDEBUG -std=gnu++20 -I "$out/stock/src" \
        "$here/${probe}_trap.cpp" -ldl -o "$out/${probe}_trap"
done

run_expected() {
    local label=$1 expected=$2 summary=$3
    shift 3
    local rc=0
    "$@" > "$out/$label.log" 2>&1 || rc=$?
    cat -- "$out/$label.log"
    [[ $rc == "$expected" ]] || {
        echo "$label: expected exit $expected, got $rc" >&2; exit 1;
    }
    grep -Fxq -- "$summary" "$out/$label.log" || {
        echo "$label: observed counts differ from the archived result" >&2; exit 1;
    }
}
run_expected output_aa 0 'traps=6 whole_object_interrupt_mismatches=0 bytes_per_object=5984' \
    "$out/output_trap" "$out/stock/core.so" "$out/stock/core.so"
run_expected output_v1 1 'traps=6 whole_object_interrupt_mismatches=2 bytes_per_object=5984' \
    "$out/output_trap" "$out/stock/core.so" "$out/v1/core.so"
run_expected output_v2 0 'traps=6 whole_object_interrupt_mismatches=0 bytes_per_object=5984' \
    "$out/output_trap" "$out/stock/core.so" "$out/v2/core.so"
run_expected elapsed_aa 0 'traps=4 whole_object_interrupt_mismatches=0 direct_public_continuation_mismatches=0 bytes_per_object=5984' \
    "$out/elapsed_trap" "$out/stock/core.so" "$out/stock/core.so"
run_expected elapsed_v2 1 'traps=4 whole_object_interrupt_mismatches=2 direct_public_continuation_mismatches=4 bytes_per_object=5984' \
    "$out/elapsed_trap" "$out/stock/core.so" "$out/v2/core.so"
grep -Fq 'seed_valid_cache=1 direct_public_continuation_output_mismatches=0 direct_public_diagnostic_mismatches=4' \
    "$out/elapsed_v2.log"
printf 'Archived rejection pattern reproduced. Review logs in %s\n' "$out"
