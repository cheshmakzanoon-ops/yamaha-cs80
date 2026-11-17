#!/usr/bin/env bash
# Explicit opt-in local research. No timing/performance study or auto-run workflow.
set -euo pipefail
if [[ $# != 3 ]]; then echo "usage: $0 REPOSITORY REVISION NEW_OUTPUT_DIRECTORY" >&2; exit 2; fi
repo=$(cd "$1" && pwd); revision=$2; out=$3
here=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
[[ $(git -C "$repo" rev-parse "$revision:src") == 702bf5ee4e375c7b13da7ec95b01b56587bbf75c ]]
[[ $(git -C "$repo" rev-parse "$revision:tests") == 64b7f72ed0139d846e8107c47c62f190cb13eefa ]]
# New directory only; never delete or overwrite prior results.
mkdir -- "$out"; out=$(cd "$out" && pwd)
mkdir "$out/baseline" "$out/candidate" "$out/build" "$out/commands" "$out/captures"
cp "$here"/{PLAN.md,QUALITY-SUBSET.md,RESULT.md,structural.cpp,capture.cpp,reduce.py,draft1x.patch,run-functional.sh,FOLLOWUP-CASES.md,followup.cpp} "$out/"
# Source comes from immutable Git objects, not dirty working-tree files.
git -C "$repo" archive "$revision" src tests | tar -x -C "$out/baseline"
cp -a "$out/baseline/src" "$out/candidate/"
(cd "$out/candidate" && patch --batch --fuzz=0 -p1 < "$here/draft1x.patch") > "$out/patch.log" 2>&1
compiler=${CXX:-g++}
"$compiler" --version > "$out/compiler.txt"
git -C "$repo" rev-parse "$revision" > "$out/revision.txt"
seq=0
run() {
 local label=$1; shift; seq=$((seq+1)); local log="$out/commands/$seq-$label"
 printf '%q ' "$@" > "$log.argv"; printf '\n' >> "$log.argv"
 # Fixed timeout per bounded compile/functional subprocess, no retry.
 local status=0; timeout "${COMMAND_TIMEOUT:-300}" "$@" > "$log.stdout" 2> "$log.stderr" || status=$?
 printf '%s\n' "$status" > "$log.exit"
 [[ $status == 0 ]]
}
for arm in baseline candidate; do
 mkdir "$out/build/$arm"
 for unit in Engine FactoryPresets Ig00156Batch; do
  extra=(); [[ $unit != Ig00156Batch ]] || extra=(-ffp-contract=off)
  run "$arm-$unit" "$compiler" -std=gnu++20 -O3 -DNDEBUG -fPIC -I"$out/$arm/src" "${extra[@]}" -c "$out/$arm/src/core/$unit.cpp" -o "$out/build/$arm/$unit.o"
 done
 extra=(); [[ $arm != candidate ]] || extra=(-DDRAFT_CANDIDATE)
 run "capture-build-$arm" "$compiler" -std=gnu++20 -O3 -DNDEBUG -fPIC "${extra[@]}" -I"$out/$arm/src" "$out/capture.cpp" "$out/build/$arm/Engine.o" "$out/build/$arm/FactoryPresets.o" "$out/build/$arm/Ig00156Batch.o" -o "$out/build/capture-$arm"
done
run structural-build "$compiler" -std=gnu++20 -O3 -DNDEBUG -fPIC -I"$out/candidate/src" "$out/structural.cpp" "$out/build/candidate/Engine.o" "$out/build/candidate/FactoryPresets.o" "$out/build/candidate/Ig00156Batch.o" -o "$out/build/structural"
run structural "$out/build/structural"
run followup-build "$compiler" -std=gnu++20 -O3 -DNDEBUG -fPIC -I"$out/candidate/src" "$out/followup.cpp" "$out/build/candidate/Engine.o" "$out/build/candidate/FactoryPresets.o" "$out/build/candidate/Ig00156Batch.o" -o "$out/build/followup"
run component-impulse "$out/build/followup"
for rate in 44100 48000 88200 96000; do
 for note in 36 96; do COMMAND_TIMEOUT=120 run "tail-$rate-$note" "$out/build/followup" "$rate" "$note"; done
done
for mode in stock off draft hq; do
 mkdir "$out/captures/$mode"; arm=candidate; [[ $mode != stock ]] || arm=baseline
 run "capture-$mode" "$out/build/capture-$arm" "$mode" "$out/captures/$mode"
done
run reduce python3 "$out/reduce.py"
python3 - "$out" <<'PY'
import hashlib,json,sys
from pathlib import Path
r=Path(sys.argv[1]);files={str(p.relative_to(r)):hashlib.sha256(p.read_bytes()).hexdigest() for p in r.rglob('*') if p.is_file()}
(r/'reproduction-manifest.json').write_text(json.dumps({'files':files,'scope':'functional research only; no CPU or sonic acceptance'},indent=2)+'\n')
PY
printf 'Functional research complete. Review quality-summary.json; this is not CPU or quality acceptance.\n'
