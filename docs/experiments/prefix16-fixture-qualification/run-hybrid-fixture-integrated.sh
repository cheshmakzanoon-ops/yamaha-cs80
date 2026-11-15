#!/bin/bash
set -u
cd "$(dirname "$0")"
export PYTHONPATH="$PWD/build-tools";export PATH="$PWD/build-tools/bin:$PATH"
prefix="$PWD/linux-deps/sysroot/usr"
export PATH="$prefix/bin:$PATH"
export CPLUS_INCLUDE_PATH="$prefix/include:$prefix/include/x86_64-linux-gnu:$prefix/include/freetype2"
export C_INCLUDE_PATH="$CPLUS_INCLUDE_PATH"
export LIBRARY_PATH="$prefix/lib/x86_64-linux-gnu"
export LD_LIBRARY_PATH="$LIBRARY_PATH"
export PKG_CONFIG_PATH="$prefix/lib/x86_64-linux-gnu/pkgconfig:$prefix/share/pkgconfig"
export PKG_CONFIG_SYSROOT_DIR="$PWD/linux-deps/sysroot"
e=hybrid-fixture-integrated-evidence
mkdir -p "$e"
cmake --build hybrid-full-build -j2 --target cs80_vst3_automation_tests cs80_modern_state_notification_tests cs80_studio_state_sync_tests cs80_host_automation_curve_tests >"$e/build.log" 2>&1
s=$?;echo $s >"$e/build-exit.txt";[ $s = 0 ]||exit $s
ctest --test-dir hybrid-full-build --output-on-failure -j1 -R '^cs80_(vst3_automation(_(44100|48000|96000)_(realtime|offline))?|modern_state_notification|studio_state_sync|host_automation_curve)_tests$' >"$e/ctest.log" 2>&1
s=$?;echo $s >"$e/ctest-exit.txt";exit $s
