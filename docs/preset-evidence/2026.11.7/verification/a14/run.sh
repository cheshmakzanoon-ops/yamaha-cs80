#!/usr/bin/env bash
# Run from repository root. PCM and matching baseline headers stay temporary.
set -euo pipefail
mkdir -p /tmp/cs80-a14/baseline-src
git archive 18711f93 src | tar -x -C /tmp/cs80-a14/baseline-src
c++ -O2 -std=c++20 -Isrc docs/preset-evidence/2026.11.7/verification/a14/engine_probe.cpp /tmp/cs80-build-1cee5a/libcs80_core.a -o /tmp/cs80-a14/probe-current
c++ -O2 -std=c++20 -I/tmp/cs80-a14/baseline-src/src docs/preset-evidence/2026.11.7/verification/a14/engine_probe.cpp /tmp/cs80-reference-1cee5a/libcs80_core.a -o /tmp/cs80-a14/probe-baseline
/tmp/cs80-a14/probe-current /tmp/cs80-a14/current > docs/preset-evidence/2026.11.7/verification/a14/current-probe.log
/tmp/cs80-a14/probe-baseline /tmp/cs80-a14/baseline > docs/preset-evidence/2026.11.7/verification/a14/baseline-probe.log
/tmp/cs80-build-1cee5a/cs80_preset_capture --inventory /tmp/cs80-a14/current-inventory.json --library all
OPENBLAS_NUM_THREADS=1 python3 docs/preset-evidence/2026.11.7/verification/a14/analyze.py > docs/preset-evidence/2026.11.7/verification/a14/analysis.log
/tmp/cs80-build-1cee5a/cs80_bass_plucked_refinement_tests > docs/preset-evidence/2026.11.7/verification/a14/bass-plucked.log
/tmp/cs80-build-1cee5a/cs80_string_body_formant_tests > docs/preset-evidence/2026.11.7/verification/a14/string-formants.log
