#!/usr/bin/env bash
set -euo pipefail
# Run from the repository root with the integrated source-matching core library.
for family in plucked bell; do
  c++ -O2 -std=c++20 -I. -Isrc "docs/preset-evidence/2026.11.6/verification/a37/${family}-gain-probe.cpp" build-preset/libcs80_core.a -pthread -o "/tmp/cs80-a37-${family}-gain-probe"
  "/tmp/cs80-a37-${family}-gain-probe"
done
