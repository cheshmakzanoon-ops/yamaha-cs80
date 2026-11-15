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
e=hybrid-full-evidence
sha256sum audio-root-hybrid/src/core/Ig00156RootFallback.h > "$e/source-hash.txt"
cmake -S audio-root-hybrid -B hybrid-full-build -DCMAKE_BUILD_TYPE=Release -DCS80_BUILD_PLUGIN=ON -DCS80_BUILD_TESTS=ON -DCS80_ENABLE_AVX2_FILTER_BATCH=ON -DCMAKE_CXX_FLAGS=-ffp-contract=off >"$e/configure.log" 2>&1
s=$?;echo $s >"$e/configure-exit.txt";[ $s = 0 ]||exit $s
cmake --build hybrid-full-build -j2 >"$e/build.log" 2>&1
s=$?;echo $s >"$e/build-exit.txt";exit $s
