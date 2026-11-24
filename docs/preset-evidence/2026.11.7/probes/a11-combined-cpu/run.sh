#!/usr/bin/env bash
set -euo pipefail
probe_dir="$(cd "$(dirname "$0")" && pwd)"
repo_dir="$(git -C "$probe_dir" rev-parse --show-toplevel)"
bench_dir="$(mktemp -d /tmp/cs80-a11-XXXXXX)"
mkdir -p "$bench_dir/pre-core"
for name in InstrumentBody.h InstrumentBodyVariants.h InstrumentBodyMaterials.h; do
    git -C "$repo_dir" show "18711f93:src/core/$name" > "$bench_dir/pre-core/$name"
done
patch -d "$bench_dir/pre-core" -p1 < "$probe_dir/pre-a07.patch"
python3 "$probe_dir/prepare.py" "$repo_dir" "$bench_dir/pre-core" "$bench_dir/headers"
g++ -std=c++17 -O3 -DNDEBUG -Wall -Wextra -Wpedantic -Wno-mismatched-new-delete \
    -I"$bench_dir/headers" "$probe_dir/benchmark.cpp" -o "$bench_dir/benchmark"
"$bench_dir/benchmark" > "$probe_dir/raw.csv" 2> "$probe_dir/run.log"
python3 "$probe_dir/summarize.py"
