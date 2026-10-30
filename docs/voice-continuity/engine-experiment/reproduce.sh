#!/usr/bin/env bash
# Immutable baseline + small patch, never a production Engine replacement.
set -euo pipefail
here=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
repo=$(git -C "$here" rev-parse --show-toplevel)
base=428e4b2bb0b4bcee0a8f3bb8826449f699bdb618
work=${1:-$(mktemp -d /tmp/cs80-engine-continuity-replay.XXXXXX)}
if [[ -n ${1:-} ]]; then mkdir -- "$work"; fi
work=$(cd -- "$work" && pwd)
mkdir "$work/base" "$work/candidate" "$work/results"
printf '%s\n' "$base" > "$work/results/base-commit.txt"
git -C "$repo" archive "$base" src/core src/tools/compatibility_probe.cpp src/tools/source_factory_compatibility_probe.cpp | tar -x -C "$work/base"
cp -a "$work/base/." "$work/candidate/"
kernel="$repo/src/experimental/ContinuousVoiceCircuit.h"
printf '%s  %s\n' e55baa85f381ffef857c7521b9ef5e8b11b43ac01ebaa19a67c1210590e6d560 "$kernel" | sha256sum -c -
mkdir "$work/candidate/src/experimental"
cp "$kernel" "$here/ExperimentalVoiceContinuityProfile.h" "$work/candidate/src/experimental/"
patch --batch --fuzz=0 -d "$work/candidate" -p1 < "$here/kernel-domain.patch"
patch --batch --fuzz=0 -d "$work/candidate" -p1 < "$here/continuity.patch"
sha256sum "$here/continuity.patch" "$here/kernel-domain.patch" "$here/ExperimentalVoiceContinuityProfile.h" "$kernel" > "$work/results/source-sha256.txt"
${CXX:-g++} --version > "$work/results/compiler.txt"
flags=(-std=c++20 -O3 -DNDEBUG -Wall -Wextra -Werror)
for variant in base candidate; do
    src="$work/$variant/src"
    ${CXX:-g++} "${flags[@]}" -I"$src" -c "$src/core/Engine.cpp" -o "$work/$variant/Engine.o"
    ${CXX:-g++} "${flags[@]}" -I"$src" -c "$src/core/FactoryPresets.cpp" -o "$work/$variant/FactoryPresets.o"
    for probe in compatibility_probe source_factory_compatibility_probe; do
        ${CXX:-g++} "${flags[@]}" -I"$src" "$src/tools/$probe.cpp" "$work/$variant/Engine.o" "$work/$variant/FactoryPresets.o" -o "$work/$variant/$probe"
    done
    defines=()
    if [[ $variant == candidate ]]; then defines=(-DCS80_CONTINUITY_AUDIT_CANDIDATE=1); fi
    ${CXX:-g++} "${flags[@]}" "${defines[@]}" -I"$src" "$here/EngineContinuityAuditTests.cpp" "$work/$variant/Engine.o" -o "$work/$variant/audit"
    "$work/$variant/audit" --raw "$work/results/$variant-three-policies.raw"
    "$work/$variant/audit" --raw-masked "$work/results/$variant-masked.raw"
    if [[ $variant == candidate ]]; then "$work/$variant/audit" --raw-disabled "$work/results/candidate-explicit-disabled.raw"; fi
    "$work/$variant/compatibility_probe" "$work/results/$variant-factory.raw"
    "$work/$variant/compatibility_probe" "$work/results/$variant-factory-tails.raw" --tails
    "$work/$variant/source_factory_compatibility_probe" "$work/results/$variant-source-factory.raw"
done
cmp "$work/results/base-three-policies.raw" "$work/results/candidate-explicit-disabled.raw"
for corpus in three-policies masked factory factory-tails source-factory; do
    cmp "$work/results/base-$corpus.raw" "$work/results/candidate-$corpus.raw"
    sha256sum "$work/results/base-$corpus.raw" "$work/results/candidate-$corpus.raw" >> "$work/results/corpora-sha256.txt"
    wc -c "$work/results/base-$corpus.raw" >> "$work/results/corpora-bytes.txt"
done
${CXX:-g++} "${flags[@]}" -I"$work/candidate/src" "$here/EngineContinuityTests.cpp" "$work/candidate/Engine.o" -o "$work/candidate/tests"
"$work/candidate/tests" | tee "$work/results/tests.txt"
"$work/candidate/audit" | tee "$work/results/audit.txt"
# Rerun the frozen primitive tests on v2. Only the audit domain-table upper
# endpoint changes in this TEMPORARY copy; original repo tests stay untouched.
sed 's/{\&Circuit::RankControls::vcoMultiplier,0,4096}/{\&Circuit::RankControls::vcoMultiplier,0,262144}/' \
    "$repo/tests/ContinuousVoiceCircuitAuditTests.cpp" > "$work/kernel-audit-v2.cpp"
for test in "$repo/tests/ContinuousVoiceCircuitTests.cpp" "$work/kernel-audit-v2.cpp"; do
    name=$(basename "$test" .cpp)
    ${CXX:-g++} "${flags[@]}" -I"$work/candidate/src" "$test" -o "$work/$name"
    "$work/$name" | tee "$work/results/$name.txt"
done
printf 'Verified immutable experiment: %s\n' "$work"
