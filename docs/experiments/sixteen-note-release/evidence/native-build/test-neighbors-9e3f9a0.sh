#!/bin/bash
set -uo pipefail
ROOT=/workspace/scratch/ee447c520988
EVIDENCE=$ROOT/sixteen-release-evidence/native-build
COMMIT=9e3f9a01d5a49a972a9e76e9e5bb36273f6ebdb8
exec > >(tee "$EVIDENCE/neighbors-9e3f9a0-run.log") 2>&1
printf 'Neighbor tests start UTC: '; date --utc --iso-8601=seconds
printf 'Source HEAD: '; git -C "$ROOT/sixteen-note-release" rev-parse HEAD
printf 'Source status:\n'; git -C "$ROOT/sixteen-note-release" status --short
if [[ "$(git -C "$ROOT/sixteen-note-release" rev-parse HEAD)" != "$COMMIT" || -n "$(git -C "$ROOT/sixteen-note-release" status --porcelain)" ]]; then
  echo 'Source is not the expected clean frozen fixture.'; exit 1
fi
sha256sum --check "$EVIDENCE/fixture-9e3f9a0-binary.sha256" || exit 1
cmd=("$ROOT/pitch-power-recovery/tools/cmake/data/bin/ctest" --test-dir "$ROOT/build-sixteen-note-release-native" --output-on-failure --verbose --parallel 1 -R '^cs80_(mpe_member_onset_host|controller_reset_host|keyboard_handoff_host|voice_activity_host|undo_replay_gesture)_tests$')
printf '%q ' "${cmd[@]}"; printf '\n'
"${cmd[@]}" 2>&1 | tee "$EVIDENCE/test-neighbors-9e3f9a0.log"
test_status=${PIPESTATUS[0]}
printf '%s\n' "$test_status" > "$EVIDENCE/test-neighbors-9e3f9a0.exit"
printf 'Neighbor tests end UTC: '; date --utc --iso-8601=seconds
printf 'Source HEAD after tests: '; git -C "$ROOT/sixteen-note-release" rev-parse HEAD
printf 'Source status after tests:\n'; git -C "$ROOT/sixteen-note-release" status --short
exit "$test_status"
