#!/bin/bash
set -uo pipefail
ROOT=/workspace/scratch/ee447c520988
EVIDENCE=$ROOT/sixteen-release-evidence/native-build
COMMIT=77dfd74b0e80328a4e0f09c01b7636c65519f191
exec > >(tee "$EVIDENCE/candidate-77dfd74-run.log") 2>&1
printf 'Candidate build start UTC: '; date --utc --iso-8601=seconds
printf 'Source HEAD: '; git -C "$ROOT/sixteen-note-release" rev-parse HEAD
printf 'Source status:\n'; git -C "$ROOT/sixteen-note-release" status --short
if [[ "$(git -C "$ROOT/sixteen-note-release" rev-parse HEAD)" != "$COMMIT" || -n "$(git -C "$ROOT/sixteen-note-release" status --porcelain)" ]]; then
  echo 'Source is not the expected clean frozen candidate.'; exit 1
fi
cmd=("$ROOT/pitch-power-recovery/tools/cmake/data/bin/cmake" --build "$ROOT/build-sixteen-note-release-native" --target CS80Snapshots --parallel 2)
printf '%q ' "${cmd[@]}"; printf '\n'
"${cmd[@]}" 2>&1 | tee "$EVIDENCE/build-candidate-77dfd74.log"
build_status=${PIPESTATUS[0]}
printf '%s\n' "$build_status" > "$EVIDENCE/build-candidate-77dfd74.exit"
printf 'Candidate build end UTC: '; date --utc --iso-8601=seconds
printf 'Source HEAD after build: '; git -C "$ROOT/sixteen-note-release" rev-parse HEAD
printf 'Source status after build:\n'; git -C "$ROOT/sixteen-note-release" status --short
if [[ "$build_status" == 0 ]]; then
  sha256sum "$ROOT/build-sixteen-note-release-native/CS80Snapshots_artefacts/Release/CS80Snapshots" | tee "$EVIDENCE/candidate-77dfd74-binary.sha256"
fi
exit "$build_status"
