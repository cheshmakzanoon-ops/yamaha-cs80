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
e=hybrid-baseline-automation-evidence
set -x
cmake --build hybrid-baseline-automation-build -j2 --target cs80_modern_state_notification_tests cs80_studio_state_sync_tests > "$e/state-build.log" 2>&1
s=$?;echo $s > "$e/state-build-exit.txt";[ $s = 0 ]||exit $s
ctest --test-dir hybrid-baseline-automation-build -R '^cs80_(modern_state_notification|studio_state_sync)_tests$' --output-on-failure > "$e/state-ctest.log" 2>&1
s=$?;echo $s > "$e/state-ctest-exit.txt";exit $s
