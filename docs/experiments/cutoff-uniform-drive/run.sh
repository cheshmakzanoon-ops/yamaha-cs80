#!/usr/bin/env bash
# Run untimed differential guides with GCC/libstdc++ on x86-64 Linux.
set -euo pipefail
if [[ $# != 2 ]]; then echo "Usage: bash run.sh PROJECT NEW_OUTPUT" >&2; exit 2; fi
evidence=$(cd "$(dirname "$0")" && pwd)
project=$(cd "$1" && pwd)
out=$2
mkdir "$out"
cd "$out"
mkdir src
base=db02cd8fa56892c6ea2cf7df084bcf190204968e
git -C "$project" archive "$base" src | tar xf - -C src --strip-components=1
CXX=${CXX:-g++}
basic=(-O3 -DNDEBUG -std=gnu++20 -Isrc)
for unit in direct direct-v2 direct-v2b direct-v2c; do
  "$CXX" "${basic[@]}" -fno-fast-math -ffp-contract=off "$evidence/$unit.cpp" -o "$unit"
  "./$unit"
done
"$CXX" "${basic[@]}" -fPIC -Wall -Wextra -Wpedantic -c src/core/Engine.cpp -o stock-engine.o
"$CXX" "${basic[@]}" -fPIC -Wall -Wextra -Wpedantic -c src/core/FactoryPresets.cpp -o presets.o
"$CXX" "${basic[@]}" -fPIC -Wall -Wextra -Wpedantic -ffp-contract=off -c src/core/Ig00156Batch.cpp -o batch.o
python3 "$evidence/engine-adapter.py" src/core/Engine.cpp Engine.cpp --reach
"$CXX" "${basic[@]}" -fPIC -Wall -Wextra -Wpedantic -I"$evidence" -Isrc/core -c Engine.cpp -o reach-engine.o
"$CXX" "${basic[@]}" -c "$evidence/engine-diverse.cpp" -o driver.o
"$CXX" driver.o stock-engine.o presets.o batch.o -o stock
"$CXX" driver.o reach-engine.o presets.o batch.o -o observed
./stock stock.bin
./observed observed.bin
cmp stock.bin observed.bin
sha256sum stock.bin observed.bin

# Optional, deliberately emulated guest work, never a CPU-time reading.
if [[ -n ${VALGRIND_USR:-} ]]; then
  export VALGRIND_USR=$(cd "$VALGRIND_USR" && pwd)
  python3 "$evidence/engine-adapter.py" src/core/Engine.cpp EngineUnmetered.cpp
  "$CXX" "${basic[@]}" -fPIC -Wall -Wextra -Wpedantic -I"$evidence" -Isrc/core -c EngineUnmetered.cpp -o unmetered-engine.o
  "$CXX" "${basic[@]}" -I"$VALGRIND_USR/include" -c "$evidence/engine-callgrind-v2.cpp" -o count-driver.o
  "$CXX" count-driver.o stock-engine.o presets.o batch.o -o callgrind-stock-v2
  "$CXX" count-driver.o unmetered-engine.o presets.o batch.o -o callgrind-candidate-v2
  python3 "$evidence/count.py"
fi
