#!/bin/bash
set -uo pipefail
ROOT=/workspace/scratch/ee447c520988
EVIDENCE=$ROOT/sixteen-release-evidence/native-build
exec > >(tee "$EVIDENCE/baseline-run.log") 2>&1
printf 'Configure start UTC: '; date --utc --iso-8601=seconds
bash -x "$EVIDENCE/configure.sh" 2>&1 | tee "$EVIDENCE/configure-baseline.log"
configure_status=${PIPESTATUS[0]}
printf '%s\n' "$configure_status" > "$EVIDENCE/configure-baseline.exit"
if [[ "$configure_status" != 0 ]]; then exit "$configure_status"; fi
printf 'Build start UTC: '; date --utc --iso-8601=seconds
cmd=("$ROOT/pitch-power-recovery/tools/cmake/data/bin/cmake" --build "$ROOT/build-sixteen-note-release-native" --target CS80Snapshots --parallel 2)
printf '%q ' "${cmd[@]}"; printf '\n'
"${cmd[@]}" 2>&1 | tee "$EVIDENCE/build-baseline.log"
build_status=${PIPESTATUS[0]}
printf '%s\n' "$build_status" > "$EVIDENCE/build-baseline.exit"
printf 'Build end UTC: '; date --utc --iso-8601=seconds
printf 'Source HEAD after build: '; git -C "$ROOT/sixteen-note-release" rev-parse HEAD
printf 'Source status after build:\n'; git -C "$ROOT/sixteen-note-release" status --short
if [[ "$build_status" == 0 ]]; then
  sha256sum "$ROOT/build-sixteen-note-release-native/CS80Snapshots_artefacts/Release/CS80Snapshots" | tee "$EVIDENCE/baseline-binary.sha256"
fi
exit "$build_status"
