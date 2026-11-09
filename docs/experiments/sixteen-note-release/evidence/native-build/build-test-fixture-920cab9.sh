#!/bin/bash
set -uo pipefail
ROOT=/workspace/scratch/ee447c520988
EVIDENCE=$ROOT/sixteen-release-evidence/native-build
COMMIT=920cab9219f500ddc6e7f629f94df7f0a00f6dd7
exec > >(tee "$EVIDENCE/fixture-920cab9-run.log") 2>&1
printf 'Fixture build start UTC: '; date --utc --iso-8601=seconds
printf 'Source HEAD: '; git -C "$ROOT/sixteen-note-release" rev-parse HEAD
printf 'Source status:\n'; git -C "$ROOT/sixteen-note-release" status --short
if [[ "$(git -C "$ROOT/sixteen-note-release" rev-parse HEAD)" != "$COMMIT" || -n "$(git -C "$ROOT/sixteen-note-release" status --porcelain)" ]]; then
  echo 'Source is not the expected clean frozen fixture.'; exit 1
fi
cmd=("$ROOT/pitch-power-recovery/tools/cmake/data/bin/cmake" --build "$ROOT/build-sixteen-note-release-native" --target CS80Snapshots --parallel 2)
printf '%q ' "${cmd[@]}"; printf '\n'
"${cmd[@]}" 2>&1 | tee "$EVIDENCE/build-fixture-920cab9.log"
build_status=${PIPESTATUS[0]}
printf '%s\n' "$build_status" > "$EVIDENCE/build-fixture-920cab9.exit"
printf 'Fixture build end UTC: '; date --utc --iso-8601=seconds
if [[ "$build_status" != 0 ]]; then exit "$build_status"; fi
sha256sum "$ROOT/build-sixteen-note-release-native/CS80Snapshots_artefacts/Release/CS80Snapshots" | tee "$EVIDENCE/fixture-920cab9-binary.sha256"
printf 'Source HEAD before test: '; git -C "$ROOT/sixteen-note-release" rev-parse HEAD
printf 'Source status before test:\n'; git -C "$ROOT/sixteen-note-release" status --short
if [[ "$(git -C "$ROOT/sixteen-note-release" rev-parse HEAD)" != "$COMMIT" || -n "$(git -C "$ROOT/sixteen-note-release" status --porcelain)" ]]; then
  echo 'Source changed during fixture build.'; exit 1
fi
cmd=("$ROOT/build-sixteen-note-release-native/CS80Snapshots_artefacts/Release/CS80Snapshots" --sixteen-voice-release-tests)
printf 'Fixture test start UTC: '; date --utc --iso-8601=seconds
printf '%q ' "${cmd[@]}"; printf '\n'
"${cmd[@]}" 2>&1 | tee "$EVIDENCE/test-fixture-920cab9.log"
test_status=${PIPESTATUS[0]}
printf '%s\n' "$test_status" > "$EVIDENCE/test-fixture-920cab9.exit"
printf 'Fixture test end UTC: '; date --utc --iso-8601=seconds
printf 'Source HEAD after test: '; git -C "$ROOT/sixteen-note-release" rev-parse HEAD
printf 'Source status after test:\n'; git -C "$ROOT/sixteen-note-release" status --short
exit "$test_status"
