#!/bin/bash
set -eu
cd "$(dirname "$0")"
fixture=../../audio-root-hybrid/tests/data/ig00156-cap-states.csv
for pair in 1 2 3; do
  if [ "$pair" = 2 ]; then order='new old'; else order='old new'; fi
  for variant in $order; do
    if [ "$variant" = old ]; then exe=../../audio-validation/capped-cost/cost; else exe=./cost; fi
    "$exe" "$fixture" > "$variant-$pair.csv"
    echo "$variant-$pair exit0"
  done
done
