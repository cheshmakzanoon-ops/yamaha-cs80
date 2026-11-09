#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
build=${1:-"$root/build/reproducible"}
mkdir -p "$build"
build=$(cd "$build" && pwd)
include="$root"
[[ ! -f "$root/src/experimental/MemoryVerifiedDiode.h" ]] || include="$root/src"
cxx=${CXX:-g++}
flags=(-std=c++20 -Wall -Wextra -Wpedantic -Werror -ffp-contract=off)
for profile in o0 o2 native; do
    case "$profile" in
        o0) optimization=(-O0);;
        o2) optimization=(-O2);;
        native) optimization=(-O3 -march=native);;
    esac
    "$cxx" "${flags[@]}" "${optimization[@]}" -I "$include" \
        "$root/tests/MemoryVerifiedDiodeTests.cpp" -o "$build/primitive-$profile"
done
"$cxx" "${flags[@]}" -O2 -g -fsanitize=address,undefined,float-cast-overflow \
    -fno-sanitize-recover=all -D_GLIBCXX_ASSERTIONS -I "$include" \
    "$root/tests/MemoryVerifiedDiodeTests.cpp" -o "$build/primitive-sanitized"
python3 - "$include/experimental/MemoryVerifiedDiode.h" "$build" <<'PY'
from pathlib import Path
import sys
s=Path(sys.argv[1]).read_text()
mutants={
 'no-outward':s.replace('if (!std::isfinite(x)) return x;',
                      'return x;\n    if (!std::isfinite(x)) return x;'),
 'trust-candidate':s.replace('    Interval bracket{across.lo',
     '    if (std::isfinite(candidate)) { output={candidate,candidate}; return true; }\n    Interval bracket{across.lo'),
 'unsafe-slope':s.replace('std::max(g.hi,0.0),slope)', 'std::max(g.hi,0.0),slope*1e12)')
                  .replace('std::max(-g.lo,0.0),slope)', 'std::max(-g.lo,0.0),slope*1e12)'),
 'bad-log-two':s.replace('0x1.62e42fefa39efp-1, 0x1.62e42fefa39f0p-1','0.7, 0.7'),
}
for name,code in mutants.items():
    assert code!=s, name
    p=Path(sys.argv[2])/name/'experimental/MemoryVerifiedDiode.h'
    p.parent.mkdir(parents=True,exist_ok=True); p.write_text(code)
PY
for mutation in no-outward trust-candidate unsafe-slope bad-log-two; do
    "$cxx" "${flags[@]}" -O2 -I "$build/$mutation" \
        "$root/tests/MemoryVerifiedDiodeTests.cpp" -o "$build/mutant-$mutation"
done
if "$cxx" -std=c++20 -O2 -ffast-math -I "$include" \
    "$root/tests/MemoryVerifiedDiodeTests.cpp" -o "$build/should-not-build" \
    2>"$build/fastmath-rejection.txt"; then
    echo 'ERROR: fast-math unexpectedly compiled' >&2
    exit 1
fi
grep 'requires strict floating-point arithmetic' "$build/fastmath-rejection.txt"
# The sandbox does not support LeakSanitizer. Other sanitizer checks stay on.
ASAN_OPTIONS=detect_leaks=0 python3 -u "$root/tests/test_memory_verified_diode.py" \
    "$build/primitive-o0" "$build/primitive-o2" "$build/primitive-native" \
    "$build/primitive-sanitized" "$build/mutant-no-outward" \
    "$build/mutant-trust-candidate" "$build/mutant-unsafe-slope" \
    "$build/mutant-bad-log-two" | tee "$build/verification.txt"

python3 "$root/tests/test_memory_centered_log_rational.py" "$build/primitive-o2" \
    | tee "$build/centered-rational.txt"
