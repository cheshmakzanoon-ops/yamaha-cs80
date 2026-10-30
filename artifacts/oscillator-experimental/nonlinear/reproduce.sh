#!/usr/bin/env bash
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd)
work=/tmp/oscillator-nonlinear-review
mkdir -p "$work/reference-coarse"
if [ "$here" != "$work" ]; then
  for file in ReconstructedOscillator.h Dsp.h nonlinear_probe.cpp variant_probe.cpp make_reference.py make_reference_coarse.py linear32_0375.h linear64_045.h linear128_049.h; do
    cp "$here/$file" "$work/$file"
  done
fi
cd "$work"
"${PYTHON:-python}" make_reference.py
"${CXX:-g++}" -std=c++17 -O3 nonlinear_probe.cpp -o nonlinear_probe
./nonlinear_probe
for variant in linear32_0375 linear64_045 linear128_049; do
  "${CXX:-g++}" -std=c++17 -O3 "-DOSCILLATOR_HEADER=\"$work/$variant.h\"" variant_probe.cpp -o "probe-$variant"
  "./probe-$variant" "$variant"
done
printf '\nRegenerated references, reports, and temporary binaries are in %s\n' "$work"
