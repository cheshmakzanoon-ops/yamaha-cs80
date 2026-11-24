#!/usr/bin/env bash
set -euo pipefail
# Run from repository root; each archive is linked only against matching headers.
evidence=docs/preset-evidence/2026.11.7/verification/a15
scratch=/tmp/cs80-a15-1cee5a
current=/tmp/cs80-build-1cee5a
baseline=/tmp/cs80-reference-1cee5a
mkdir -p "$scratch/baseline" "$scratch/current-pcm" "$scratch/baseline-pcm"
git archive 18711f93 src | tar -x -C "$scratch/baseline"
"$current/cs80_preset_capture" --library curated --inventory "$scratch/current-inventory.json"
"$baseline/cs80_preset_capture" --library curated --inventory "$scratch/baseline-inventory.json"
c++ -O2 -std=c++20 -Isrc "$evidence/engine_gestures.cpp" "$current/libcs80_core.a" -o "$scratch/current-probe"
c++ -O2 -std=c++20 -I"$scratch/baseline/src" "$evidence/engine_gestures.cpp" "$baseline/libcs80_core.a" -o "$scratch/baseline-probe"
"$scratch/current-probe" "$scratch/current-pcm" > "$evidence/current-renders.csv"
"$scratch/baseline-probe" "$scratch/baseline-pcm" > "$evidence/baseline-renders.csv"
"$current/cs80_plucked_material_preset_tests" > "$evidence/material-preset-tests.log"
"$current/cs80_bass_plucked_refinement_tests" > "$evidence/bass-plucked-tests.log"
python3 "$evidence/analyze.py" "$scratch" "$evidence"
