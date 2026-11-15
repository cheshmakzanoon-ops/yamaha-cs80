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
git -C audio-root-recovery rev-parse HEAD > "$e/source-commit.txt"
git -C audio-root-recovery status --short > "$e/source-status.txt"
cmake -S audio-root-recovery -B hybrid-baseline-automation-build -DCMAKE_BUILD_TYPE=Release -DCS80_BUILD_PLUGIN=ON -DCS80_BUILD_TESTS=ON -DCS80_ENABLE_AVX2_FILTER_BATCH=ON -DCMAKE_CXX_FLAGS=-ffp-contract=off -DFETCHCONTENT_SOURCE_DIR_JUCE="$PWD/hybrid-full-build/_deps/juce-src" -DFETCHCONTENT_SOURCE_DIR_CS80_LIBXML2="$PWD/hybrid-full-build/_deps/cs80_libxml2-src" > "$e/configure.log" 2>&1
s=$?;echo $s > "$e/configure-exit.txt";[ $s = 0 ]||exit $s
cmake --build hybrid-baseline-automation-build -j2 --target cs80_vst3_automation_tests > "$e/build.log" 2>&1
s=$?;echo $s > "$e/build-exit.txt";[ $s = 0 ]||exit $s
ctest --test-dir hybrid-baseline-automation-build -R '^cs80_vst3_automation_tests$' --output-on-failure > "$e/ctest.log" 2>&1
s=$?;echo $s > "$e/ctest-exit.txt";exit $s
