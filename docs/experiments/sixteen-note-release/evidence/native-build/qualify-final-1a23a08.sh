#!/bin/bash
set -uo pipefail
ROOT=/workspace/scratch/ee447c520988
EVIDENCE=$ROOT/sixteen-release-evidence/native-build
COMMIT=1a23a085657a07d9f066f7cc38aaf2572a6fe714
BUILD=$ROOT/build-sixteen-note-release-native
BINARY=$BUILD/CS80Snapshots_artefacts/Release/CS80Snapshots
CMAKE=$ROOT/pitch-power-recovery/tools/cmake/data/bin/cmake
CTEST=$ROOT/pitch-power-recovery/tools/cmake/data/bin/ctest
NINJA=$ROOT/pitch-power-recovery/tools/bin/ninja
exec > >(tee "$EVIDENCE/final-1a23a08-run.log") 2>&1
printf 'Final qualification start UTC: '; date --utc --iso-8601=seconds
printf 'Source HEAD: '; git -C "$ROOT/sixteen-note-release" rev-parse HEAD
printf 'Source tree: '; git -C "$ROOT/sixteen-note-release" rev-parse HEAD^{tree}
printf 'Source status:\n'; git -C "$ROOT/sixteen-note-release" status --short
if [[ "$(git -C "$ROOT/sixteen-note-release" rev-parse HEAD)" != "$COMMIT" || -n "$(git -C "$ROOT/sixteen-note-release" status --porcelain)" ]]; then
 echo 'Source is not the expected clean frozen final candidate.'; exit 1
fi
run_stage() {
 local name="$1"; shift
 printf '\nStage %s command: ' "$name"; printf '%q ' "$@"; printf '\n'
 "$@" 2>&1 | tee "$EVIDENCE/final-1a23a08-$name.log"
 local status=${PIPESTATUS[0]}
 printf '%s\n' "$status" > "$EVIDENCE/final-1a23a08-$name.exit"
 return "$status"
}
run_stage configure bash -x "$EVIDENCE/configure.sh" || exit "$?"
run_stage build "$CMAKE" --build "$BUILD" --target CS80Snapshots --parallel 2 || exit "$?"
sha256sum "$BINARY" | tee "$EVIDENCE/final-1a23a08-binary-before-tests.sha256"
run_stage ctest "$CTEST" --test-dir "$BUILD" --output-on-failure --verbose --parallel 1 -R '^cs80_sixteen_voice_release_host_tests$' || exit "$?"
run_stage direct "$BINARY" --sixteen-voice-release-tests || exit "$?"
run_stage no-work "$CMAKE" --build "$BUILD" --target CS80Snapshots --parallel 2 || exit "$?"
run_stage dry-run "$NINJA" -C "$BUILD" -n CS80Snapshots || exit "$?"
printf '\nFinal source HEAD: '; git -C "$ROOT/sixteen-note-release" rev-parse HEAD
printf 'Final source tree: '; git -C "$ROOT/sixteen-note-release" rev-parse HEAD^{tree}
printf 'Final source status:\n'; git -C "$ROOT/sixteen-note-release" status --short
if [[ "$(git -C "$ROOT/sixteen-note-release" rev-parse HEAD)" != "$COMMIT" || -n "$(git -C "$ROOT/sixteen-note-release" status --porcelain)" ]]; then
 echo 'Source changed during final qualification.'; exit 1
fi
sha256sum --check "$EVIDENCE/final-1a23a08-binary-before-tests.sha256" || exit 1
sha256sum "$BINARY" | tee "$EVIDENCE/final-1a23a08-binary.sha256"
printf 'Final qualification end UTC: '; date --utc --iso-8601=seconds
