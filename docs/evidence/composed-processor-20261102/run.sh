#!/bin/bash
set -euo pipefail
cd /workspace/scratch/aa102d573ba3
source cs80-native-tools/native-env.sh
test "$(git -C cs80-integration rev-parse HEAD)" = 5b26e431a3e1ea5cb0416580edff0580dc9a9575
test "$(git -C cs80-integration rev-parse HEAD:src)" = 0f8a26e733644ac9f5985164e0175d4143c5d917
test -z "$(git -C cs80-integration status --porcelain)"
test "$(git -C cs80-native-tools/JUCE rev-parse HEAD)" = c352e2489000ffeb9e397051377d75eef2efe641
test -z "$(git -C cs80-native-tools/JUCE status --porcelain)"
python - <<'PY'
import json,hashlib
from pathlib import Path
j=json.loads(Path('cs80-host-tool-evidence/factory-tone-host/locale-green/build-receipt.json').read_text())
for x in j['reused_module_and_font_objects']:
 assert hashlib.sha256(Path(x['file']).read_bytes()).hexdigest()==x['sha256'],x['file']
print('Pinned JUCE/font object bytes verified',len(j['reused_module_and_font_objects']),flush=True)
PY
python cs80-integration-native-evidence/build-focused-root.py root-composed
B=/workspace/scratch/aa102d573ba3/cs80-integration-native-evidence/root-composed/CS80Snapshots
"$B" --factory-tone-locale-test > cs80-integration-native-evidence/root-composed/factory-locale.log 2>&1
cat cs80-integration-native-evidence/root-composed/factory-locale.log
"$B" --render-policy-locale-test > cs80-integration-native-evidence/root-composed/policy-locale.log 2>&1
cat cs80-integration-native-evidence/root-composed/policy-locale.log
python - <<'PY'
import json
from pathlib import Path
a=json.loads(Path('cs80-host-tool-evidence/factory-tone-host/locale-green/build-receipt.json').read_text())
b=json.loads(Path('cs80-integration-native-evidence/root-composed/build-receipt.json').read_text())
assert a['reused_module_and_font_objects']==b['reused_module_and_font_objects']
assert len(b['compile_commands'])==26 and b['src_tree']=='0f8a26e733644ac9f5985164e0175d4143c5d917'
print('All26projectTUs fresh; exact unchanged JUCE/font reuse confirmed')
PY
cmake -S cs80-integration -B cs80-integration/build-integration -G Ninja -DCS80_BUILD_PLUGIN=OFF -DCS80_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build cs80-integration/build-integration -j2 --target cs80_tuning_input_boundary_tests cs80_scala_float_pitch_bounds_tests cs80_mts_counted_message_tests cs80_mts_realtime_semantics_tests cs80_tuning_midi_tests cs80_mts_envelope_tests
ctest --test-dir cs80-integration/build-integration --output-on-failure -R '^cs80_(tuning_input_boundary|scala_float_pitch_bounds|mts_counted_message|mts_realtime_semantics|tuning_midi|mts_envelope)_tests$'
test -z "$(git -C cs80-integration status --porcelain)"
printf 'COMPLETE ';date -u +%FT%TZ
