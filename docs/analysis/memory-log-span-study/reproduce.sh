#!/usr/bin/env bash
# Optional reproduction of REJECTED research prototypes; never activates them.
set -euo pipefail
root=$(cd "$(dirname "$0")/../../.." && pwd)
study="$root/docs/analysis/memory-log-span-study"
build=${1:?give a scratch build directory}
mkdir -p "$build"; build=$(cd "$build" && pwd)
cxx=${CXX:-g++}
flags=(-std=c++20 -O2 -Wall -Wextra -Wpedantic -Werror -ffp-contract=off)
for version in baseline rejected-lower rejected-centered; do
    mkdir -p "$build/$version/experimental"
    cp "$study/$version-MemoryVerifiedDiode.h" "$build/$version/experimental/MemoryVerifiedDiode.h"
    "$cxx" "${flags[@]}" -I "$build/$version" "$root/tests/MemoryVerifiedDiodeTests.cpp" -o "$build/$version-probe"
done
CS80_COMPARE_DIODE_AVAILABILITY=1 python3 "$root/tests/test_memory_verified_diode.py" \
    "$build/baseline-probe" "$build/rejected-lower-probe" "$build/rejected-centered-probe"
python3 - "$study/rejected-centered-MemoryVerifiedDiode.h" "$build" <<'PY'
from pathlib import Path
import sys
source=Path(sys.argv[1]).read_text()
mutant=source.replace('sub(center,{0.0,left.hi})','sub(center,{0.0,0.0})').replace('add(center,{0.0,right.hi})','add(center,{0.0,0.0})')
assert mutant!=source
p=Path(sys.argv[2])/'mutant-span/experimental/MemoryVerifiedDiode.h'
p.parent.mkdir(parents=True,exist_ok=True);p.write_text(mutant)
PY
"$cxx" "${flags[@]}" -I "$build/mutant-span" "$root/tests/MemoryVerifiedDiodeTests.cpp" -o "$build/mutant-span-omitted"
python3 "$root/tests/test_memory_log_span_rational.py" "$build/baseline-probe" \
    "$build/rejected-lower-probe" "$build/rejected-centered-probe" "$build/mutant-span-omitted"
# Centered prototype only; no claim that the rejected lower prototype ran this grid.
"$cxx" "${flags[@]}" -I "$build/rejected-centered" -I "$root/src" \
    "$root/tests/MemorySparseUpdateTests.cpp" -o "$build/centered-sparse"
"$build/centered-sparse" "$root/tests/parity/tolerances-v2/memory-sparse-update.json"
