#!/bin/bash
set -u
cd "$(dirname "$0")"
export PYTHONPATH="$PWD/build-tools";export PATH="$PWD/build-tools/bin:$PWD/linux-deps/sysroot/usr/bin:$PATH"
export LD_LIBRARY_PATH="$PWD/linux-deps/sysroot/usr/lib/x86_64-linux-gnu"
e=hybrid-full-evidence
while [ ! -f "$e/build-exit.txt" ];do sleep 10;done
[ "$(cat "$e/build-exit.txt")" = 0 ]||exit 1
exclude='^(cs80_voice_activity_ui_tests|cs80_ui_smoke|cs80_teardown_probe|cs80_locale_(query|editor)(_mixed)?)$'
printf '%s\n' "$exclude" >"$e/nonwindow-exclusion.txt"
ctest --test-dir hybrid-full-build --show-only=json-v1 -E "$exclude" >"$e/nonwindow-inventory.json"
ctest --test-dir hybrid-full-build --output-on-failure -j1 -E "$exclude" >"$e/nonwindow.log" 2>&1
s=$?;echo "$s" >"$e/nonwindow-exit.txt";exit "$s"
